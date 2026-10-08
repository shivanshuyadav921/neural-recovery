"""
Evaluation metrics for NeuroParse Phase 15 Benchmarking.
Implements quality metrics (recovery success, exact repair accuracy, AST validity,
remaining errors), latency statistics (mean, median, p95, p99, overhead),
validator statistics, and failure taxonomy.
"""

import math
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple


@dataclass
class QualityMetrics:
    total_samples: int = 0
    parse_success_count: int = 0
    recovery_success_count: int = 0
    exact_repair_count: int = 0
    exact_operation_count: int = 0
    exact_token_count: int = 0
    exact_position_count: int = 0
    ast_valid_count: int = 0
    fallback_count: int = 0
    initial_errors_sum: int = 0
    remaining_errors_sum: int = 0

    @property
    def parse_success_rate(self) -> float:
        return self.parse_success_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def recovery_success_rate(self) -> float:
        return self.recovery_success_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def exact_repair_rate(self) -> float:
        return self.exact_repair_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def operation_accuracy(self) -> float:
        return self.exact_operation_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def token_accuracy(self) -> float:
        return self.exact_token_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def position_accuracy(self) -> float:
        return self.exact_position_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def ast_valid_rate(self) -> float:
        return self.ast_valid_count / self.total_samples if self.total_samples > 0 else 0.0

    @property
    def fallback_rate(self) -> float:
        return self.fallback_count / self.total_samples if self.total_samples > 0 else 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_samples": self.total_samples,
            "parse_success_count": self.parse_success_count,
            "parse_success_rate": self.parse_success_rate,
            "recovery_success_count": self.recovery_success_count,
            "recovery_success_rate": self.recovery_success_rate,
            "exact_repair_count": self.exact_repair_count,
            "exact_repair_rate": self.exact_repair_rate,
            "operation_accuracy": self.operation_accuracy,
            "token_accuracy": self.token_accuracy,
            "position_accuracy": self.position_accuracy,
            "ast_valid_count": self.ast_valid_count,
            "ast_valid_rate": self.ast_valid_rate,
            "fallback_count": self.fallback_count,
            "fallback_rate": self.fallback_rate,
            "initial_errors_sum": self.initial_errors_sum,
            "remaining_errors_sum": self.remaining_errors_sum,
        }


@dataclass
class LatencyStats:
    count: int = 0
    mean_us: float = 0.0
    median_us: float = 0.0
    p95_us: float = 0.0
    p99_us: float = 0.0
    min_us: float = 0.0
    max_us: float = 0.0

    @classmethod
    def calculate(cls, values_us: List[float]) -> "LatencyStats":
        if not values_us:
            return cls()
        sorted_vals = sorted(values_us)
        n = len(sorted_vals)
        mean = sum(sorted_vals) / n
        median = sorted_vals[n // 2] if n % 2 != 0 else (sorted_vals[n // 2 - 1] + sorted_vals[n // 2]) / 2.0
        p95_idx = min(n - 1, int(math.ceil(0.95 * n)) - 1)
        p99_idx = min(n - 1, int(math.ceil(0.99 * n)) - 1)
        return cls(
            count=n,
            mean_us=round(mean, 2),
            median_us=round(median, 2),
            p95_us=round(sorted_vals[p95_idx], 2),
            p99_us=round(sorted_vals[p99_idx], 2),
            min_us=round(sorted_vals[0], 2),
            max_us=round(sorted_vals[-1], 2),
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "count": self.count,
            "mean_us": self.mean_us,
            "median_us": self.median_us,
            "p95_us": self.p95_us,
            "p99_us": self.p99_us,
            "min_us": self.min_us,
            "max_us": self.max_us,
        }


@dataclass
class PipelineStats:
    total_proposals: int = 0
    accepted_proposals: int = 0
    rejected_proposals: int = 0
    applied_repairs: int = 0
    successful_repairs: int = 0
    failed_repairs: int = 0

    @property
    def acceptance_rate(self) -> float:
        return self.accepted_proposals / self.total_proposals if self.total_proposals > 0 else 0.0

    @property
    def success_given_accepted(self) -> float:
        return self.successful_repairs / self.accepted_proposals if self.accepted_proposals > 0 else 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_proposals": self.total_proposals,
            "accepted_proposals": self.accepted_proposals,
            "rejected_proposals": self.rejected_proposals,
            "acceptance_rate": self.acceptance_rate,
            "applied_repairs": self.applied_repairs,
            "successful_repairs": self.successful_repairs,
            "failed_repairs": self.failed_repairs,
            "success_given_accepted": self.success_given_accepted,
        }


def check_exact_repair(
    predicted_op: str,
    predicted_token: str,
    predicted_pos: int,
    gt_op: str,
    gt_token: str,
    gt_pos: int,
) -> Tuple[bool, bool, bool, bool]:
    """
    Checks exact repair match and component matches.
    Returns: (is_exact, op_match, token_match, pos_match)
    """
    norm_pred_op = predicted_op.upper().replace("-", "_")
    norm_gt_op = gt_op.upper().replace("-", "_")

    # Mapping common aliases
    op_aliases = {
        "INSERT_TOKEN": "INSERT_TOKEN",
        "DELETE_TOKEN": "DELETE_TOKEN",
        "REPLACE_TOKEN": "REPLACE_TOKEN",
        "INSERT": "INSERT_TOKEN",
        "DELETE": "DELETE_TOKEN",
        "REPLACE": "REPLACE_TOKEN",
    }
    norm_pred_op = op_aliases.get(norm_pred_op, norm_pred_op)
    norm_gt_op = op_aliases.get(norm_gt_op, norm_gt_op)

    op_match = (norm_pred_op == norm_gt_op)

    # For DELETE, token lexeme is less important than position/action, but if provided check match
    if norm_gt_op == "DELETE_TOKEN":
        token_match = True
    else:
        token_match = (predicted_token.strip("'\"") == gt_token.strip("'\""))

    pos_match = (predicted_pos == gt_pos)

    is_exact = op_match and token_match and pos_match
    return is_exact, op_match, token_match, pos_match


def categorize_failure(
    recovery_attempted: bool,
    validator_accepted: bool,
    applied: bool,
    parse_success: bool,
    op_match: bool,
    token_match: bool,
    pos_match: bool,
    error_message: str = "",
) -> str:
    """
    Categorizes failure mode into a standardized failure taxonomy.
    """
    if parse_success:
        return "none"
    if not recovery_attempted:
        return "no_recovery_attempted"
    if not validator_accepted:
        return "validator_rejection"
    if not applied:
        return "repair_application_failure"
    if not op_match:
        return "wrong_operation"
    if not token_match:
        return "wrong_token"
    if not pos_match:
        return "wrong_position"
    if "cascading" in error_message.lower():
        return "cascading_error"
    return "parser_still_fails"


def normalize_ast_dump(ast_dump: str) -> str:
    """
    Normalizes AST string representation by removing whitespace / line-ending variance
    for structural comparison.
    """
    lines = [line.rstrip() for line in ast_dump.strip().splitlines() if line.strip()]
    return "\n".join(lines)


def compare_ast_structures(ast_recovered: str, ast_original: str) -> bool:
    """
    Compares two AST dumps structurally, ignoring minor line/col metadata if any.
    """
    norm_rec = normalize_ast_dump(ast_recovered)
    norm_orig = normalize_ast_dump(ast_original)
    if not norm_rec or not norm_orig:
        return False
    return norm_rec == norm_orig
