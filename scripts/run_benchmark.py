#!/usr/bin/env python3
"""
Reproducible Benchmark Runner for NeuroParse Phase 15.
Evaluates quality and cost across Baseline Deterministic recovery vs NeuroParse Neural-assisted recovery.
"""

import argparse
import json
import math
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

# Add project root to sys.path
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from ml.evaluation.benchmark_metrics import LatencyStats
from ml.evaluation.benchmark_runner import BenchmarkRunner, get_system_environment
from ml.models.vocabulary import Vocabulary


def audit_dataset_leakage(
    train_path: Path, val_path: Path, test_path: Path
) -> Dict[str, Any]:
    """Audits data leakage across train, validation, and test splits."""
    def get_source_files(path: Path) -> Tuple[set, int]:
        files = set()
        count = 0
        if path.exists():
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    if line.strip():
                        count += 1
                        data = json.loads(line)
                        files.add(data.get("source_file", "unknown"))
        return files, count

    train_files, train_count = get_source_files(train_path)
    val_files, val_count = get_source_files(val_path)
    test_files, test_count = get_source_files(test_path)

    train_val_overlap = train_files.intersection(val_files)
    train_test_overlap = train_files.intersection(test_files)
    val_test_overlap = val_files.intersection(test_files)

    return {
        "train_samples": train_count,
        "val_samples": val_count,
        "test_samples": test_count,
        "train_unique_programs": len(train_files),
        "val_unique_programs": len(val_files),
        "test_unique_programs": len(test_files),
        "train_files": sorted(list(train_files)),
        "val_files": sorted(list(val_files)),
        "test_files": sorted(list(test_files)),
        "train_val_overlap": sorted(list(train_val_overlap)),
        "train_test_overlap": sorted(list(train_test_overlap)),
        "val_test_overlap": sorted(list(val_test_overlap)),
        "is_leakage_free": (
            len(train_val_overlap) == 0
            and len(train_test_overlap) == 0
            and len(val_test_overlap) == 0
        ),
    }


def benchmark_onnx_inference(
    onnx_path: Path,
    test_samples: List[Dict[str, Any]],
    warmup_runs: int = 10,
    measured_runs: int = 50,
) -> Dict[str, Any]:
    """Measures raw ONNX inference latency (cold start vs warm inference)."""
    if not onnx_path.exists():
        return {"status": "NOT AVAILABLE - ONNX model missing"}

    import onnxruntime as ort

    # 1. Measure Cold Start (Session creation + first inference)
    t_cold_start = time.perf_counter_ns()
    session = ort.InferenceSession(str(onnx_path), providers=["CPUExecutionProvider"])
    vocab = Vocabulary()

    # Create dummy batch of length 64
    dummy_input = np.zeros((1, 64), dtype=np.int64)
    dummy_mask = np.ones((1, 64), dtype=np.int64)
    dummy_seg = np.ones((1, 64), dtype=np.int64)
    dummy_pos = np.zeros((1,), dtype=np.int64)
    dummy_inputs = {
        "input_ids": dummy_input,
        "attention_mask": dummy_mask,
        "segment_ids": dummy_seg,
        "error_positions": dummy_pos,
    }
    
    _ = session.run(None, dummy_inputs)
    t_cold_end = time.perf_counter_ns()
    cold_start_latency_us = (t_cold_end - t_cold_start) / 1000.0

    # 2. Warm-up
    for _ in range(warmup_runs):
        session.run(None, dummy_inputs)

    # 3. Steady-State / Warm Inference on actual sample contexts
    latencies_us = []
    confidence_scores = []

    for sample in test_samples:
        # Encode tokens
        ctx = sample.get("context", {})
        tokens = []
        for t in ctx.get("tokens_before", []):
            tokens.append(vocab.encode_token(t))
        curr = ctx.get("current_token", {})
        if curr:
            tokens.append(vocab.encode_token(curr))
        err_pos_in_seq = max(0, len(tokens) - 1)
        for t in ctx.get("tokens_after", []):
            tokens.append(vocab.encode_token(t))

        # Truncate / pad to 64
        seq_len = 64
        if len(tokens) > seq_len:
            tokens = tokens[-seq_len:]
            err_pos_in_seq = min(seq_len - 1, err_pos_in_seq)
        pad_len = seq_len - len(tokens)
        input_ids = np.array([tokens + [vocab.pad_id] * pad_len], dtype=np.int64)
        attn_mask = np.array([[1] * len(tokens) + [0] * pad_len], dtype=np.int64)
        seg_ids = np.array([[1] * len(tokens) + [0] * pad_len], dtype=np.int64)
        err_pos = np.array([err_pos_in_seq], dtype=np.int64)

        sample_inputs = {
            "input_ids": input_ids,
            "attention_mask": attn_mask,
            "segment_ids": seg_ids,
            "error_positions": err_pos,
        }

        for _ in range(max(1, measured_runs // max(1, len(test_samples)))):
            t0 = time.perf_counter_ns()
            outputs = session.run(None, sample_inputs)
            t1 = time.perf_counter_ns()
            latencies_us.append((t1 - t0) / 1000.0)

            # Compute softmax confidence for operation head (output 0)
            op_logits = outputs[0][0]
            exp_logits = np.exp(op_logits - np.max(op_logits))
            op_probs = exp_logits / np.sum(exp_logits)
            max_conf = float(np.max(op_probs))
            confidence_scores.append(max_conf)

    stats = LatencyStats.calculate(latencies_us)

    # Bin confidence
    low_conf = sum(1 for c in confidence_scores if c < 0.6)
    med_conf = sum(1 for c in confidence_scores if 0.6 <= c < 0.85)
    high_conf = sum(1 for c in confidence_scores if c >= 0.85)

    return {
        "status": "VERIFIED",
        "cold_start_latency_us": round(cold_start_latency_us, 2),
        "warm_inference": stats.to_dict(),
        "total_measured_inferences": len(latencies_us),
        "confidence_distribution": {
            "low_conf (<0.60)": low_conf,
            "med_conf (0.60-0.85)": med_conf,
            "high_conf (>=0.85)": high_conf,
            "mean_confidence": round(float(np.mean(confidence_scores)), 4) if confidence_scores else 0.0,
        },
    }


def evaluate_multiple_errors() -> Dict[str, Any]:
    """Evaluates multi-error recovery on real and synthetic multi-error cases."""
    from ml.compiler_interface import run_compiler

    test_programs = [
        {
            "name": "double_missing_semicolon",
            "source": "let x = 10\nlet y = 20\nlet z = x + y;\n",
            "expected_errors": 2,
        },
        {
            "name": "missing_paren_and_brace",
            "source": "function test(a, b {\n    return a + b;\n\n",
            "expected_errors": 2,
        },
        {
            "name": "multiple_corruptions_block",
            "source": "function calc(x, y) {\n    let a = x + 1\n    let b = y + 2\n    return a + b\n}\n",
            "expected_errors": 3,
        },
    ]

    results = {}
    for prog in test_programs:
        prog_name = prog["name"]
        src = prog["source"]
        results[prog_name] = {}
        for mode in ["none", "panic", "heuristic", "neural", "auto"]:
            res = run_compiler(f"--recovery={mode}", "--explain-recovery", input_text=src)
            from ml.evaluation.benchmark_runner import parse_recovery_explanation
            data = parse_recovery_explanation(res.stdout)
            ast_res = run_compiler(f"--recovery={mode}", "--dump-ast", input_text=src)
            has_ast = "Program" in ast_res.stdout
            results[prog_name][mode] = {
                "total_errors": data["total_errors"],
                "recovered_errors": data["recovered_errors"],
                "failed_recoveries": data["failed_recoveries"],
                "cascading_errors": data["cascading_errors"],
                "ast_constructed": has_ast,
                "recovery_rate": data["recovery_rate"],
            }

    return results


def format_markdown_tables(results: Dict[str, Any]) -> str:
    """Generates human-readable Markdown research tables from actual results."""
    lines = []
    lines.append("# NeuroParse Phase 15 Experimental Benchmark Results\n")
    lines.append(f"**Date/Time:** {results['metadata']['timestamp']}")
    lines.append(f"**Dataset:** {results['metadata']['dataset_path']} ({results['metadata']['total_samples']} held-out samples)")
    lines.append(f"**Hardware Platform:** {results['metadata']['environment']['machine']} ({results['metadata']['environment']['processor']})")
    lines.append(f"**Operating System:** {results['metadata']['environment']['os']}")
    lines.append(f"**Compiler Build:** {results['metadata']['environment']['compiler_version']}\n")

    # Table 1: Overall Recovery
    lines.append("### Table 1 — Overall Recovery Comparison")
    lines.append("| System / Strategy | Samples | Parse Success | Recovery Success | AST Valid | Remaining Errors | Fallback Rate |")
    lines.append("|---|---|---|---|---|---|---|")
    for m in results["metadata"]["modes_evaluated"]:
        q = results["overall_quality"][m]
        lines.append(
            f"| **{m.capitalize()}** | {q['total_samples']} | "
            f"{q['parse_success_count']} ({q['parse_success_rate']*100:.1f}%) | "
            f"{q['recovery_success_count']} ({q['recovery_success_rate']*100:.1f}%) | "
            f"{q['ast_valid_count']} ({q['ast_valid_rate']*100:.1f}%) | "
            f"{q['remaining_errors_sum']} | "
            f"{q['fallback_rate']*100:.1f}% |"
        )
    lines.append("")

    # Table 2: Corruption Type Breakdown
    lines.append("### Table 2 — Per-Corruption-Type Recovery & Exact Repair")
    lines.append("| Corruption Type | Samples | Baseline (Heuristic) | NeuroParse (Neural) | Exact Repair (Neural) |")
    lines.append("|---|---|---|---|---|")
    for corr, modes_dict in results["per_corruption"].items():
        n_samp = modes_dict.get("heuristic", {}).get("total_samples", 0)
        base_rec = modes_dict.get("heuristic", {}).get("recovery_success_rate", 0.0) * 100
        neural_rec = modes_dict.get("neural", {}).get("recovery_success_rate", 0.0) * 100
        exact_rep = modes_dict.get("neural", {}).get("exact_repair_rate", 0.0) * 100
        lines.append(
            f"| `{corr}` | {n_samp} | {base_rec:.1f}% | **{neural_rec:.1f}%** | {exact_rep:.1f}% |"
        )
    lines.append("")

    # Table 3: Latency & Overhead
    lines.append("### Table 3 — Compiler Execution & Recovery Latency")
    lines.append("| System | Compiler Mean (µs) | Median (µs) | P95 (µs) | Recovery Mean (µs) | Overhead vs Baseline |")
    lines.append("|---|---|---|---|---|---|")
    for m in results["metadata"]["modes_evaluated"]:
        comp_lat = results["latency"][m]["compiler"]
        rec_lat = results["latency"][m]["recovery"]
        ovh = results["overhead_percentage_vs_heuristic"].get(m, 0.0)
        ovh_str = f"{ovh:+.1f}%" if m != "heuristic" else "0.0% (ref)"
        lines.append(
            f"| **{m.capitalize()}** | {comp_lat['mean_us']} | {comp_lat['median_us']} | {comp_lat['p95_us']} | {rec_lat['mean_us']} | {ovh_str} |"
        )
    lines.append("")

    # Table 4: Neural Recovery Pipeline
    lines.append("### Table 4 — Neural Recovery Pipeline Statistics")
    lines.append("| System | Total Proposals | Accepted | Rejected | Applied | Successful | Success given Accepted |")
    lines.append("|---|---|---|---|---|---|---|")
    for m in ["neural", "auto"]:
        p = results["pipeline_statistics"][m]
        lines.append(
            f"| **{m.capitalize()}** | {p['total_proposals']} | {p['accepted_proposals']} ({p['acceptance_rate']*100:.1f}%) | "
            f"{p['rejected_proposals']} | {p['applied_repairs']} | {p['successful_repairs']} | {p['success_given_accepted']*100:.1f}% |"
        )
    lines.append("")

    # ONNX Inference Latency
    lines.append("### Neural Inference Engine Latency (ONNX Runtime CPU)")
    onnx_res = results.get("onnx_inference", {})
    if onnx_res.get("status") == "VERIFIED":
        w = onnx_res["warm_inference"]
        lines.append(f"- **Cold Start Latency (Session Init + 1st inference):** {onnx_res['cold_start_latency_us']} µs")
        lines.append(f"- **Warm Inference Mean:** {w['mean_us']} µs")
        lines.append(f"- **Warm Inference Median:** {w['median_us']} µs")
        lines.append(f"- **Warm Inference P95:** {w['p95_us']} µs")
        lines.append(f"- **Confidence Distribution:** {onnx_res['confidence_distribution']}")
    else:
        lines.append(f"- {onnx_res.get('status')}")
    lines.append("")

    # Data Leakage Audit
    leak = results.get("leakage_audit", {})
    lines.append("### Dataset Leakage Audit")
    lines.append(f"- **Train Originals:** {leak.get('train_unique_programs')} programs ({leak.get('train_samples')} samples)")
    lines.append(f"- **Validation Originals:** {leak.get('val_unique_programs')} programs ({leak.get('val_samples')} samples)")
    lines.append(f"- **Test Originals:** {leak.get('test_unique_programs')} programs ({leak.get('test_samples')} samples)")
    lines.append(f"- **Train-Test Overlap:** {leak.get('train_test_overlap')}")
    lines.append(f"- **Leakage-Free Verified:** **{'YES (0 overlap)' if leak.get('is_leakage_free') else 'NO'}**\n")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="NeuroParse Phase 15 Benchmark Runner")
    parser.add_argument("--dataset", type=str, default="ml/dataset/generated/test.jsonl", help="Path to test dataset JSONL")
    parser.add_argument("--output", type=str, default="benchmarks/results/phase15_results.json", help="Path to save JSON results")
    parser.add_argument("--report", type=str, default="benchmarks/results/phase15_report.md", help="Path to save Markdown report")
    args = parser.parse_args()

    dataset_path = Path(args.dataset)
    if not dataset_path.is_absolute():
        dataset_path = ROOT / dataset_path

    out_path = Path(args.output)
    if not out_path.is_absolute():
        out_path = ROOT / out_path
    out_path.parent.mkdir(parents=True, exist_ok=True)

    report_path = Path(args.report)
    if not report_path.is_absolute():
        report_path = ROOT / report_path
    report_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"[*] Running NeuroParse Benchmarks on: {dataset_path}")
    runner = BenchmarkRunner(dataset_path=dataset_path)
    results = runner.run_benchmarks()

    # Audit data leakage
    print("[*] Performing Dataset Leakage Audit...")
    leak_audit = audit_dataset_leakage(
        train_path=ROOT / "ml" / "dataset" / "generated" / "train.jsonl",
        val_path=ROOT / "ml" / "dataset" / "generated" / "validation.jsonl",
        test_path=dataset_path,
    )
    results["leakage_audit"] = leak_audit

    # ONNX inference profiling
    print("[*] Profiling ONNX Neural Inference Engine...")
    onnx_path = ROOT / "models" / "repair_model.onnx"
    test_samples = runner.load_dataset()
    onnx_bench = benchmark_onnx_inference(onnx_path, test_samples)
    results["onnx_inference"] = onnx_bench

    # Multi-error evaluation
    print("[*] Evaluating Multi-Error Programs...")
    multi_error_res = evaluate_multiple_errors()
    results["multi_error_evaluation"] = multi_error_res

    # Memory measurement
    results["memory_measurement"] = {
        "status": "NOT VERIFIED",
        "rationale": "High-precision isolated process memory profiling requires OS-level instrumentation and varies with memory caching; compiler binary memory is minimal (<10MB).",
    }

    # Save structured JSON
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print(f"[+] Structured benchmark results saved to: {out_path}")

    # Generate Markdown report
    md_text = format_markdown_tables(results)
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(md_text)
    print(f"[+] Markdown report saved to: {report_path}")

    print("\n" + md_text)


if __name__ == "__main__":
    main()
