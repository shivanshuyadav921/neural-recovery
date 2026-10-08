"""
Unit tests for Phase 15: Benchmarking & Experimental Evaluation.
Tests metrics calculation, exact repair matching, AST comparison, latency percentiles,
failure categorization, dataset leakage audit, and benchmark runner mechanics.
"""

import json
from pathlib import Path

import pytest

from ml.evaluation.benchmark_metrics import (
    LatencyStats,
    PipelineStats,
    QualityMetrics,
    categorize_failure,
    check_exact_repair,
    compare_ast_structures,
    normalize_ast_dump,
)
from ml.evaluation.benchmark_runner import (
    BenchmarkRunner,
    extract_predicted_repair_from_desc,
    parse_recovery_explanation,
)
from scripts.run_benchmark import audit_dataset_leakage

ROOT = Path(__file__).resolve().parent.parent.parent


def test_quality_metrics_calculation():
    qm = QualityMetrics(
        total_samples=10,
        parse_success_count=8,
        recovery_success_count=7,
        exact_repair_count=6,
        exact_operation_count=9,
        exact_token_count=7,
        exact_position_count=8,
        ast_valid_count=8,
        fallback_count=1,
        initial_errors_sum=12,
        remaining_errors_sum=3,
    )
    assert qm.parse_success_rate == 0.8
    assert qm.recovery_success_rate == 0.7
    assert qm.exact_repair_rate == 0.6
    assert qm.operation_accuracy == 0.9
    assert qm.token_accuracy == 0.7
    assert qm.position_accuracy == 0.8
    assert qm.ast_valid_rate == 0.8
    assert qm.fallback_rate == 0.1

    d = qm.to_dict()
    assert d["total_samples"] == 10
    assert d["exact_repair_rate"] == 0.6


def test_latency_stats_calculation():
    # Odd count
    vals = [10.0, 20.0, 30.0, 40.0, 50.0]
    s = LatencyStats.calculate(vals)
    assert s.count == 5
    assert s.mean_us == 30.0
    assert s.median_us == 30.0
    assert s.min_us == 10.0
    assert s.max_us == 50.0
    assert s.p95_us == 50.0

    # Even count
    vals_even = [10.0, 20.0, 30.0, 40.0]
    s_even = LatencyStats.calculate(vals_even)
    assert s_even.count == 4
    assert s_even.median_us == 25.0
    assert s_even.mean_us == 25.0


def test_exact_repair_matching():
    # Exact insert
    is_exact, op_m, tok_m, pos_m = check_exact_repair(
        predicted_op="INSERT_TOKEN",
        predicted_token=";",
        predicted_pos=12,
        gt_op="INSERT_TOKEN",
        gt_token=";",
        gt_pos=12,
    )
    assert is_exact is True
    assert op_m is True
    assert tok_m is True
    assert pos_m is True

    # Token mismatch
    is_exact, op_m, tok_m, pos_m = check_exact_repair(
        predicted_op="INSERT_TOKEN",
        predicted_token=")",
        predicted_pos=12,
        gt_op="INSERT_TOKEN",
        gt_token=";",
        gt_pos=12,
    )
    assert is_exact is False
    assert op_m is True
    assert tok_m is False
    assert pos_m is True

    # Position mismatch
    is_exact, op_m, tok_m, pos_m = check_exact_repair(
        predicted_op="INSERT_TOKEN",
        predicted_token=";",
        predicted_pos=10,
        gt_op="INSERT_TOKEN",
        gt_token=";",
        gt_pos=12,
    )
    assert is_exact is False
    assert op_m is True
    assert tok_m is True
    assert pos_m is False


def test_failure_categorization():
    cat = categorize_failure(
        recovery_attempted=True,
        validator_accepted=False,
        applied=False,
        parse_success=False,
        op_match=True,
        token_match=True,
        pos_match=True,
    )
    assert cat == "validator_rejection"

    cat_op = categorize_failure(
        recovery_attempted=True,
        validator_accepted=True,
        applied=True,
        parse_success=False,
        op_match=False,
        token_match=True,
        pos_match=True,
    )
    assert cat_op == "wrong_operation"

    cat_none = categorize_failure(
        recovery_attempted=True,
        validator_accepted=True,
        applied=True,
        parse_success=True,
        op_match=True,
        token_match=True,
        pos_match=True,
    )
    assert cat_none == "none"


def test_ast_dump_comparison():
    ast1 = "Program\n├── FunctionDeclaration: foo()\n│   └── BlockStatement\n"
    ast2 = "Program\n├── FunctionDeclaration: foo()\n│   └── BlockStatement\n\n"
    assert compare_ast_structures(ast1, ast2) is True

    ast3 = "Program\n└── VariableDeclaration: x\n"
    assert compare_ast_structures(ast1, ast3) is False


def test_extract_predicted_repair_from_desc():
    desc = "Validated neural repair applied: INSERT_TOKEN token=';' pos=12 conf=0.95"
    op, tok, pos, conf = extract_predicted_repair_from_desc(desc)
    assert op == "INSERT_TOKEN"
    assert tok == ";"
    assert pos == 12
    assert conf == 0.95


def test_parse_recovery_explanation():
    sample_text = """
=== NeuroParse Recovery Report ===
File: test.nl
Total Syntax Errors: 2
Successfully Recovered: 1
Failed Recoveries: 1
Cascading Errors: 0
Recovery Rate: 50.00%
Total Recovery Latency: 250.00 µs
Average Latency per Attempt: 125.00 µs

--- Diagnostics & Recovery Events ---
[1] test.nl:2:5: syntax error: Expected ';'
    Strategy: neural
    Action: insert_token
    Status: RECOVERED
    Description: Validated neural repair applied: INSERT_TOKEN token=';' pos=5 conf=0.98
    Latency: 120.00 µs
    """
    data = parse_recovery_explanation(sample_text)
    assert data["total_errors"] == 2
    assert data["recovered_errors"] == 1
    assert data["failed_recoveries"] == 1
    assert data["recovery_rate"] == 50.0
    assert len(data["events"]) == 1
    assert data["events"][0]["strategy"] == "neural"


def test_dataset_leakage_audit():
    train_p = ROOT / "ml" / "dataset" / "generated" / "train.jsonl"
    val_p = ROOT / "ml" / "dataset" / "generated" / "validation.jsonl"
    test_p = ROOT / "ml" / "dataset" / "generated" / "test.jsonl"

    audit = audit_dataset_leakage(train_p, val_p, test_p)
    assert audit["is_leakage_free"] is True
    assert len(audit["train_test_overlap"]) == 0
    assert audit["test_samples"] == 30
    assert audit["train_samples"] == 110


def test_benchmark_runner_single_sample():
    dataset_p = ROOT / "ml" / "dataset" / "generated" / "test.jsonl"
    runner = BenchmarkRunner(dataset_path=dataset_p, modes=["heuristic", "neural"])
    samples = runner.load_dataset()
    assert len(samples) > 0

    first_sample = samples[0]
    res_heur = runner.run_single_sample(first_sample, "heuristic")
    assert res_heur["mode"] == "heuristic"
    assert "compiler_latency_us" in res_heur
    assert "recovery_latency_us" in res_heur

    res_neur = runner.run_single_sample(first_sample, "neural")
    assert res_neur["mode"] == "neural"
    assert "exact_repair" in res_neur
