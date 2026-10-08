"""
Benchmark Runner for NeuroParse Phase 15.
Executes reproducible comparative evaluation between baseline deterministic recovery
and NeuroParse neural-assisted recovery on identical test inputs.
"""

import json
import os
import platform
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from ml.compiler_interface import get_executable_path, run_compiler
from ml.evaluation.benchmark_metrics import (
    LatencyStats,
    PipelineStats,
    QualityMetrics,
    categorize_failure,
    check_exact_repair,
    compare_ast_structures,
)

ROOT = Path(__file__).resolve().parent.parent.parent


def get_system_environment() -> Dict[str, Any]:
    """Captures the exact hardware and software environment."""
    env = {
        "os": platform.platform(),
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python_version": sys.version,
        "executable_path": get_executable_path(),
    }
    # Check git commit if available
    try:
        git_rev = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
        )
        if git_rev.returncode == 0:
            env["git_commit"] = git_rev.stdout.strip()
    except Exception:
        env["git_commit"] = "unknown"

    # Compiler version
    try:
        ver_res = subprocess.run(
            [get_executable_path(), "--version"],
            capture_output=True,
            text=True,
        )
        if ver_res.returncode == 0:
            env["compiler_version"] = ver_res.stdout.strip()
    except Exception:
        env["compiler_version"] = "unknown"

    # ONNX model info
    onnx_path = ROOT / "models" / "repair_model.onnx"
    env["onnx_model_path"] = str(onnx_path)
    env["onnx_model_exists"] = onnx_path.exists()
    if onnx_path.exists():
        env["onnx_model_size_bytes"] = onnx_path.stat().st_size

    try:
        import onnxruntime
        env["onnxruntime_version"] = onnxruntime.__version__
    except ImportError:
        env["onnxruntime_version"] = "native-c++"

    return env


def parse_recovery_explanation(output: str) -> Dict[str, Any]:
    """Parses the formatted CLI output from --explain-recovery."""
    data = {
        "total_errors": 0,
        "recovered_errors": 0,
        "failed_recoveries": 0,
        "cascading_errors": 0,
        "recovery_rate": 0.0,
        "total_latency_us": 0.0,
        "events": [],
    }

    m_tot = re.search(r"Total Syntax Errors:\s+(\d+)", output)
    if m_tot:
        data["total_errors"] = int(m_tot.group(1))

    m_rec = re.search(r"Successfully Recovered:\s+(\d+)", output)
    if m_rec:
        data["recovered_errors"] = int(m_rec.group(1))

    m_fail = re.search(r"Failed Recoveries:\s+(\d+)", output)
    if m_fail:
        data["failed_recoveries"] = int(m_fail.group(1))

    m_casc = re.search(r"Cascading Errors:\s+(\d+)", output)
    if m_casc:
        data["cascading_errors"] = int(m_casc.group(1))

    m_rate = re.search(r"Recovery Rate:\s+([\d\.]+)%", output)
    if m_rate:
        data["recovery_rate"] = float(m_rate.group(1))

    m_lat = re.search(r"Total Recovery Latency:\s+([\d\.]+)\s+µs", output)
    if m_lat:
        data["total_latency_us"] = float(m_lat.group(1))

    # Detect diagnostic error entries in output
    diag_matches = re.findall(r"\[\d+\] .*?: syntax error:", output)
    if not diag_matches:
        diag_matches = re.findall(r"syntax error:", output)
    if data["total_errors"] == 0 and diag_matches:
        data["total_errors"] = len(diag_matches)
        data["failed_recoveries"] = len(diag_matches)

    # Parse individual events
    lines = output.splitlines()
    current_event: Dict[str, Any] = {}
    for line in lines:
        line_str = line.strip()
        if line_str.startswith("Strategy:"):
            current_event["strategy"] = line_str.split(":", 1)[1].strip()
        elif line_str.startswith("Action:"):
            current_event["action"] = line_str.split(":", 1)[1].strip()
        elif line_str.startswith("Status:"):
            current_event["status"] = line_str.split(":", 1)[1].strip()
        elif line_str.startswith("Description:"):
            current_event["description"] = line_str.split(":", 1)[1].strip()
        elif line_str.startswith("Latency:"):
            m = re.search(r"Latency:\s+([\d\.]+)\s+µs", line_str)
            if m:
                current_event["latency_us"] = float(m.group(1))
            if current_event:
                data["events"].append(dict(current_event))
                current_event = {}

    return data


def extract_predicted_repair_from_desc(description: str) -> Tuple[str, str, int, float]:
    """
    Extracts (operation, token, position, confidence) from a recovery description string.
    Example: 'Validated neural repair applied: INSERT_TOKEN token=\';\' pos=12 conf=0.95'
    """
    op = "NONE"
    tok = ""
    pos = -1
    conf = 0.0

    m_op = re.search(r"\b(INSERT_TOKEN|DELETE_TOKEN|REPLACE_TOKEN|INSERT|DELETE|REPLACE)\b", description)
    if m_op:
        op = m_op.group(1)

    m_tok = re.search(r"token=['\"](.*?)['\"]", description)
    if m_tok:
        tok = m_tok.group(1)

    m_pos = re.search(r"pos=(\d+)", description)
    if m_pos:
        pos = int(m_pos.group(1))

    m_conf = re.search(r"conf=([\d\.]+)", description)
    if m_conf:
        conf = float(m_conf.group(1))

    return op, tok, pos, conf


class BenchmarkRunner:
    def __init__(
        self,
        dataset_path: Path,
        modes: Optional[List[str]] = None,
        iterations: int = 1,
    ):
        self.dataset_path = dataset_path
        self.modes = modes or ["none", "panic", "heuristic", "neural", "auto"]
        self.iterations = iterations

    def load_dataset(self) -> List[Dict[str, Any]]:
        samples = []
        with open(self.dataset_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    samples.append(json.loads(line))
        return samples

    def run_single_sample(
        self,
        sample: Dict[str, Any],
        mode: str,
    ) -> Dict[str, Any]:
        """Runs the compiler on a single sample in a given recovery mode."""
        corrupted_source = sample["corrupted_source"]
        original_source = sample.get("original_source", "")
        gt_repair = sample.get("repair", {})
        gt_op = gt_repair.get("operation", "")
        gt_tok = gt_repair.get("token", "")
        gt_pos = gt_repair.get("position", -1)

        # 1. Run compiler with --dump-ast --recovery=<mode>
        t0 = time.perf_counter_ns()
        ast_res = run_compiler(f"--recovery={mode}", "--dump-ast", input_text=corrupted_source)
        t1 = time.perf_counter_ns()
        compiler_latency_us = (t1 - t0) / 1000.0

        # Check if AST was constructed
        ast_output = ast_res.stdout.strip()
        has_ast = bool(ast_output and ast_output.startswith("Program"))

        # 2. Run compiler with --explain-recovery --recovery=<mode>
        explain_res = run_compiler(f"--recovery={mode}", "--explain-recovery", input_text=corrupted_source)
        report_data = parse_recovery_explanation(explain_res.stdout)

        # 3. Get ground-truth AST from original source
        orig_ast_output = ""
        if original_source:
            orig_ast_res = run_compiler("--dump-ast", input_text=original_source)
            orig_ast_output = orig_ast_res.stdout.strip()

        # AST structural comparison
        ast_struct_valid = False
        if has_ast and orig_ast_output:
            ast_struct_valid = compare_ast_structures(ast_output, orig_ast_output)
        elif has_ast:
            ast_struct_valid = True  # If no original AST to compare, non-empty Program AST

        # Parse success: AST constructed and no unrecovered errors
        recovered_count = report_data["recovered_errors"]
        failed_count = report_data["failed_recoveries"]
        total_errors = report_data["total_errors"]
        
        if mode == "none":
            parse_success = (has_ast and total_errors == 0)
            recovery_success = False
        else:
            parse_success = (has_ast and (failed_count == 0 or (recovered_count > 0 and recovered_count >= total_errors)))
            recovery_success = (recovered_count > 0 and has_ast)

        # Check exact repair from events
        pred_op = ""
        pred_tok = ""
        pred_pos = -1
        pred_conf = 0.0
        used_strategy = "none"
        event_status = "FAILED"

        if report_data["events"]:
            first_event = report_data["events"][0]
            used_strategy = first_event.get("strategy", "none")
            event_status = first_event.get("status", "FAILED")
            desc = first_event.get("description", "")
            pred_op, pred_tok, pred_pos, pred_conf = extract_predicted_repair_from_desc(desc)

        is_exact, op_match, tok_match, pos_match = check_exact_repair(
            pred_op, pred_tok, pred_pos, gt_op, gt_tok, gt_pos
        )

        recovery_attempted = (mode != "none" and total_errors > 0)
        validator_accepted = ("Validated" in explain_res.stdout or "RECOVERED" in event_status)
        applied = (recovered_count > 0)

        failure_cat = categorize_failure(
            recovery_attempted=recovery_attempted,
            validator_accepted=validator_accepted,
            applied=applied,
            parse_success=parse_success,
            op_match=op_match,
            token_match=tok_match,
            pos_match=pos_match,
            error_message=ast_res.stderr,
        )

        return {
            "sample_id": sample.get("sample_id", "unknown"),
            "corruption_type": sample.get("corruption_type", "unknown"),
            "mode": mode,
            "compiler_latency_us": compiler_latency_us,
            "recovery_latency_us": report_data["total_latency_us"],
            "total_errors": total_errors,
            "recovered_errors": recovered_count,
            "failed_recoveries": failed_count,
            "cascading_errors": report_data["cascading_errors"],
            "parse_success": parse_success,
            "recovery_success": recovery_success,
            "ast_valid": ast_struct_valid,
            "exact_repair": is_exact,
            "op_match": op_match,
            "tok_match": tok_match,
            "pos_match": pos_match,
            "predicted_op": pred_op,
            "predicted_tok": pred_tok,
            "predicted_pos": pred_pos,
            "predicted_conf": pred_conf,
            "used_strategy": used_strategy,
            "validator_accepted": validator_accepted,
            "failure_category": failure_cat,
        }

    def run_benchmarks(self) -> Dict[str, Any]:
        """Runs the complete benchmark suite across all modes and samples."""
        samples = self.load_dataset()
        env = get_system_environment()

        mode_results: Dict[str, List[Dict[str, Any]]] = {m: [] for m in self.modes}
        mode_quality: Dict[str, QualityMetrics] = {m: QualityMetrics() for m in self.modes}
        mode_latency: Dict[str, List[float]] = {m: [] for m in self.modes}
        mode_rec_latency: Dict[str, List[float]] = {m: [] for m in self.modes}
        mode_pipeline: Dict[str, PipelineStats] = {m: PipelineStats() for m in self.modes}
        mode_failures: Dict[str, Dict[str, int]] = {m: {} for m in self.modes}

        # Per-corruption breakdowns
        per_corruption: Dict[str, Dict[str, QualityMetrics]] = {}

        for sample in samples:
            corr_type = sample.get("corruption_type", "unknown")
            if corr_type not in per_corruption:
                per_corruption[corr_type] = {m: QualityMetrics() for m in self.modes}

            for mode in self.modes:
                res = self.run_single_sample(sample, mode)
                mode_results[mode].append(res)

                # Update Quality
                qm = mode_quality[mode]
                qm.total_samples += 1
                if res["parse_success"]:
                    qm.parse_success_count += 1
                if res["recovery_success"]:
                    qm.recovery_success_count += 1
                if res["exact_repair"]:
                    qm.exact_repair_count += 1
                if res["op_match"]:
                    qm.exact_operation_count += 1
                if res["tok_match"]:
                    qm.exact_token_count += 1
                if res["pos_match"]:
                    qm.exact_position_count += 1
                if res["ast_valid"]:
                    qm.ast_valid_count += 1
                if res["used_strategy"] == "heuristic" and mode in ["neural", "auto"]:
                    qm.fallback_count += 1
                qm.initial_errors_sum += res["total_errors"]
                qm.remaining_errors_sum += res["failed_recoveries"]

                # Per-corruption quality
                cqm = per_corruption[corr_type][mode]
                cqm.total_samples += 1
                if res["parse_success"]:
                    cqm.parse_success_count += 1
                if res["recovery_success"]:
                    cqm.recovery_success_count += 1
                if res["exact_repair"]:
                    cqm.exact_repair_count += 1
                if res["ast_valid"]:
                    cqm.ast_valid_count += 1

                # Latencies
                mode_latency[mode].append(res["compiler_latency_us"])
                if res["recovery_latency_us"] > 0:
                    mode_rec_latency[mode].append(res["recovery_latency_us"])

                # Pipeline stats (for neural/auto)
                ps = mode_pipeline[mode]
                if res["total_errors"] > 0 and mode in ["neural", "auto"]:
                    ps.total_proposals += 1
                    if res["validator_accepted"]:
                        ps.accepted_proposals += 1
                        if res["recovered_errors"] > 0:
                            ps.applied_repairs += 1
                            if res["parse_success"]:
                                ps.successful_repairs += 1
                            else:
                                ps.failed_repairs += 1
                    else:
                        ps.rejected_proposals += 1

                # Failure taxonomy
                f_cat = res["failure_category"]
                mode_failures[mode][f_cat] = mode_failures[mode].get(f_cat, 0) + 1

        # Calculate latency statistics
        latency_stats = {
            m: {
                "compiler": LatencyStats.calculate(mode_latency[m]).to_dict(),
                "recovery": LatencyStats.calculate(mode_rec_latency[m]).to_dict(),
            }
            for m in self.modes
        }

        # Calculate relative overhead compared to heuristic baseline
        baseline_mean = latency_stats.get("heuristic", {}).get("compiler", {}).get("mean_us", 1.0)
        overhead_pct = {}
        for m in self.modes:
            m_mean = latency_stats[m]["compiler"]["mean_us"]
            if baseline_mean > 0:
                overhead_pct[m] = round(((m_mean - baseline_mean) / baseline_mean) * 100.0, 2)
            else:
                overhead_pct[m] = 0.0

        # Structure final results object
        output = {
            "metadata": {
                "benchmark_name": "Phase 15 - NeuroParse Comparative Evaluation",
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "dataset_path": str(self.dataset_path),
                "total_samples": len(samples),
                "modes_evaluated": self.modes,
                "environment": env,
            },
            "overall_quality": {m: mode_quality[m].to_dict() for m in self.modes},
            "per_corruption": {
                corr: {m: per_corruption[corr][m].to_dict() for m in self.modes}
                for corr in per_corruption
            },
            "latency": latency_stats,
            "overhead_percentage_vs_heuristic": overhead_pct,
            "pipeline_statistics": {m: mode_pipeline[m].to_dict() for m in self.modes},
            "failure_taxonomy": mode_failures,
            "raw_results": mode_results,
        }

        return output
