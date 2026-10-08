from collections import defaultdict
from dataclasses import asdict, dataclass
from typing import Any, Dict, List, Optional, Set, Tuple

import numpy as np
import torch

from ml.models.vocabulary import Vocabulary


@dataclass
class ClassificationReport:
    accuracy: float
    macro_precision: float
    macro_recall: float
    macro_f1: float
    weighted_f1: float
    per_class: Dict[str, Dict[str, float]]
    confusion_matrix: List[List[int]]
    classes: List[str]


def compute_classification_metrics(
    y_true: List[int],
    y_pred: List[int],
    class_names: List[str],
) -> ClassificationReport:
    num_classes = len(class_names)
    cm = [[0] * num_classes for _ in range(num_classes)]

    for t, p in zip(y_true, y_pred):
        if 0 <= t < num_classes and 0 <= p < num_classes:
            cm[t][p] += 1

    total = len(y_true)
    correct = sum(cm[i][i] for i in range(num_classes))
    accuracy = correct / total if total > 0 else 0.0

    per_class: Dict[str, Dict[str, float]] = {}
    precisions: List[float] = []
    recalls: List[float] = []
    f1s: List[float] = []
    supports: List[int] = []

    for i, name in enumerate(class_names):
        tp = cm[i][i]
        fp = sum(cm[r][i] for r in range(num_classes)) - tp
        fn = sum(cm[i][c] for c in range(num_classes)) - tp
        support = tp + fn

        prec = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        rec = tp / (tp + fn) if (tp + fn) > 0 else 0.0
        f1 = 2 * prec * rec / (prec + rec) if (prec + rec) > 0 else 0.0

        per_class[name] = {
            "precision": round(prec, 4),
            "recall": round(rec, 4),
            "f1": round(f1, 4),
            "support": support,
        }
        precisions.append(prec)
        recalls.append(rec)
        f1s.append(f1)
        supports.append(support)

    macro_prec = sum(precisions) / num_classes if num_classes > 0 else 0.0
    macro_rec = sum(recalls) / num_classes if num_classes > 0 else 0.0
    macro_f1 = sum(f1s) / num_classes if num_classes > 0 else 0.0

    total_support = sum(supports)
    weighted_f1 = (
        sum(f * s for f, s in zip(f1s, supports)) / total_support
        if total_support > 0
        else 0.0
    )

    return ClassificationReport(
        accuracy=round(accuracy, 4),
        macro_precision=round(macro_prec, 4),
        macro_recall=round(macro_rec, 4),
        macro_f1=round(macro_f1, 4),
        weighted_f1=round(weighted_f1, 4),
        per_class=per_class,
        confusion_matrix=cm,
        classes=class_names,
    )


@dataclass
class EvaluationResults:
    num_samples: int
    operation_report: ClassificationReport
    token_accuracy: float
    position_accuracy: float
    exact_repair_accuracy: float
    invalid_prediction_rate: float
    mean_confidence: float
    mean_confidence_correct: float
    mean_confidence_incorrect: float
    per_corruption: Dict[str, Dict[str, float]]
    failure_examples: List[Dict[str, Any]]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "num_samples": self.num_samples,
            "operation_accuracy": self.operation_report.accuracy,
            "operation_macro_f1": self.operation_report.macro_f1,
            "operation_weighted_f1": self.operation_report.weighted_f1,
            "token_accuracy": self.token_accuracy,
            "position_accuracy": self.position_accuracy,
            "exact_repair_accuracy": self.exact_repair_accuracy,
            "invalid_prediction_rate": self.invalid_prediction_rate,
            "mean_confidence": self.mean_confidence,
            "mean_confidence_correct": self.mean_confidence_correct,
            "mean_confidence_incorrect": self.mean_confidence_incorrect,
            "per_corruption": self.per_corruption,
            "operation_per_class": self.operation_report.per_class,
        }


def evaluate_predictions(
    target_ops: List[int],
    pred_ops: List[int],
    target_tokens: List[int],
    pred_tokens: List[int],
    target_positions: List[int],
    pred_positions: List[int],
    confidences: List[float],
    corruptions: List[str],
    sample_ids: Optional[List[str]] = None,
    vocabulary: Optional[Vocabulary] = None,
) -> EvaluationResults:
    vocab = vocabulary if vocabulary is not None else Vocabulary()
    num_samples = len(target_ops)
    if num_samples == 0:
        empty_report = ClassificationReport(
            accuracy=0.0,
            macro_precision=0.0,
            macro_recall=0.0,
            macro_f1=0.0,
            weighted_f1=0.0,
            per_class={},
            confusion_matrix=[],
            classes=vocab.operations,
        )
        return EvaluationResults(
            num_samples=0,
            operation_report=empty_report,
            token_accuracy=0.0,
            position_accuracy=0.0,
            exact_repair_accuracy=0.0,
            invalid_prediction_rate=0.0,
            mean_confidence=0.0,
            mean_confidence_correct=0.0,
            mean_confidence_incorrect=0.0,
            per_corruption={},
            failure_examples=[],
        )

    # Operation evaluation
    op_report = compute_classification_metrics(target_ops, pred_ops, vocab.operations)

    # Token accuracy (evaluated only when token prediction is meaningful)
    token_applicable_indices = [
        i
        for i, op in enumerate(target_ops)
        if vocab.id_to_op.get(op) in ("INSERT_TOKEN", "REPLACE_TOKEN")
    ]
    if token_applicable_indices:
        token_correct = sum(
            1
            for i in token_applicable_indices
            if pred_tokens[i] == target_tokens[i]
        )
        token_acc = token_correct / len(token_applicable_indices)
    else:
        token_acc = 1.0

    # Position accuracy
    pos_correct = sum(
        1 for t_p, p_p in zip(target_positions, pred_positions) if t_p == p_p
    )
    pos_acc = pos_correct / num_samples

    # Exact structured repair match
    exact_matches: List[bool] = []
    invalid_predictions = 0
    failures: List[Dict[str, Any]] = []

    insert_op_id = vocab.op_to_id.get("INSERT_TOKEN", 0)
    replace_op_id = vocab.op_to_id.get("REPLACE_TOKEN", 2)
    delete_op_id = vocab.op_to_id.get("DELETE_TOKEN", 1)

    for i in range(num_samples):
        t_op = target_ops[i]
        p_op = pred_ops[i]
        t_tok = target_tokens[i]
        p_tok = pred_tokens[i]
        t_pos = target_positions[i]
        p_pos = pred_positions[i]
        conf = confidences[i]
        corr = corruptions[i] if i < len(corruptions) else "unknown"
        sid = sample_ids[i] if sample_ids and i < len(sample_ids) else f"sample_{i}"

        # Structural validity check
        if p_op not in vocab.id_to_op:
            invalid_predictions += 1
            is_valid = False
        elif p_op in (insert_op_id, replace_op_id) and p_tok not in vocab.id_to_token:
            invalid_predictions += 1
            is_valid = False
        else:
            is_valid = True

        # Check exact repair equivalence
        op_match = (p_op == t_op)
        pos_match = (p_pos == t_pos)

        if t_op in (insert_op_id, replace_op_id):
            tok_match = (p_tok == t_tok)
        else:
            tok_match = True  # For DELETE_TOKEN or NO_REPAIR, token identity is irrelevant

        is_exact = op_match and tok_match and pos_match
        exact_matches.append(is_exact)

        if not is_exact:
            failures.append({
                "sample_id": sid,
                "corruption_type": corr,
                "expected_operation": vocab.decode_operation(t_op),
                "predicted_operation": vocab.decode_operation(p_op),
                "expected_token": vocab.decode_token(t_tok) if t_op in (insert_op_id, replace_op_id) else None,
                "predicted_token": vocab.decode_token(p_tok) if p_op in (insert_op_id, replace_op_id) else None,
                "expected_position": t_pos,
                "predicted_position": p_pos,
                "confidence": round(conf, 4),
            })

    exact_repair_acc = sum(exact_matches) / num_samples
    invalid_rate = invalid_predictions / num_samples

    # Confidence breakdowns
    correct_confs = [c for c, m in zip(confidences, exact_matches) if m]
    incorrect_confs = [c for c, m in zip(confidences, exact_matches) if not m]

    mean_conf = float(np.mean(confidences)) if confidences else 0.0
    mean_conf_corr = float(np.mean(correct_confs)) if correct_confs else 0.0
    mean_conf_inc = float(np.mean(incorrect_confs)) if incorrect_confs else 0.0

    # Per-corruption breakdown
    corruption_groups = defaultdict(list)
    for i, corr in enumerate(corruptions):
        corruption_groups[corr].append(i)

    per_corruption: Dict[str, Dict[str, float]] = {}
    for corr_name, indices in corruption_groups.items():
        sub_n = len(indices)
        sub_op_correct = sum(1 for idx in indices if pred_ops[idx] == target_ops[idx])
        sub_exact_correct = sum(1 for idx in indices if exact_matches[idx])

        sub_token_indices = [
            idx for idx in indices
            if vocab.id_to_op.get(target_ops[idx]) in ("INSERT_TOKEN", "REPLACE_TOKEN")
        ]
        sub_tok_correct = sum(
            1 for idx in sub_token_indices if pred_tokens[idx] == target_tokens[idx]
        )
        sub_tok_acc = sub_tok_correct / len(sub_token_indices) if sub_token_indices else 1.0

        per_corruption[corr_name] = {
            "samples": sub_n,
            "operation_accuracy": round(sub_op_correct / sub_n, 4),
            "token_accuracy": round(sub_tok_acc, 4),
            "exact_repair_accuracy": round(sub_exact_correct / sub_n, 4),
        }

    return EvaluationResults(
        num_samples=num_samples,
        operation_report=op_report,
        token_accuracy=round(token_acc, 4),
        position_accuracy=round(pos_acc, 4),
        exact_repair_accuracy=round(exact_repair_acc, 4),
        invalid_prediction_rate=round(invalid_rate, 4),
        mean_confidence=round(mean_conf, 4),
        mean_confidence_correct=round(mean_conf_corr, 4),
        mean_confidence_incorrect=round(mean_conf_inc, 4),
        per_corruption=per_corruption,
        failure_examples=failures,
    )


def format_evaluation_summary(results: EvaluationResults) -> str:
    lines = [
        "=" * 60,
        " NeuroParse Evaluation Metrics Report",
        "=" * 60,
        f"Total Samples:             {results.num_samples}",
        f"Operation Accuracy:        {results.operation_report.accuracy * 100:.2f}% (Macro F1: {results.operation_report.macro_f1:.4f})",
        f"Token Accuracy:            {results.token_accuracy * 100:.2f}%",
        f"Position Accuracy:         {results.position_accuracy * 100:.2f}%",
        f"Exact Repair Accuracy:     {results.exact_repair_accuracy * 100:.2f}%",
        f"Invalid Prediction Rate:   {results.invalid_prediction_rate * 100:.2f}%",
        f"Mean Confidence (All):     {results.mean_confidence:.4f}",
        f"Mean Confidence (Correct): {results.mean_confidence_correct:.4f}",
        f"Mean Confidence (Wrong):   {results.mean_confidence_incorrect:.4f}",
        "-" * 60,
        "Operation Breakdown:",
    ]
    for op_name, stats in results.operation_report.per_class.items():
        lines.append(
            f"  {op_name:18s} | Prec: {stats['precision']:.4f} | Rec: {stats['recall']:.4f} | F1: {stats['f1']:.4f} | Supp: {stats['support']}"
        )

    lines.extend([
        "-" * 60,
        "Performance by Corruption Type:",
    ])
    for corr, stats in sorted(results.per_corruption.items()):
        lines.append(
            f"  {corr:25s} | N={stats['samples']:2d} | Op Acc: {stats['operation_accuracy']*100:.1f}% | Tok Acc: {stats['token_accuracy']*100:.1f}% | Exact: {stats['exact_repair_accuracy']*100:.1f}%"
        )
    lines.append("=" * 60)
    return "\n".join(lines)
