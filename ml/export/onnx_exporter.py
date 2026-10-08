import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

import numpy as np
import onnx
import onnxruntime as ort
import torch
import torch.nn as nn
import torch.nn.functional as F

from ml.models.config import ModelConfig
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary
from ml.training.checkpoint import CheckpointManager


class ONNXExportWrapper(nn.Module):
    """Wrapper around RepairModel returning a tuple of raw logit tensors for ONNX export."""

    def __init__(self, model: RepairModel):
        super().__init__()
        self.model = model

    def forward(
        self,
        input_ids: torch.Tensor,
        attention_mask: torch.Tensor,
        segment_ids: torch.Tensor,
        error_positions: torch.Tensor,
    ) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        """Inference forward pass returning raw logits for each prediction head.

        Args:
            input_ids: [batch_size, seq_len] (int64)
            attention_mask: [batch_size, seq_len] (int64)
            segment_ids: [batch_size, seq_len] (int64)
            error_positions: [batch_size] (int64)

        Returns:
            (operation_logits, token_logits, position_logits)
        """
        output = self.model(
            input_ids=input_ids,
            attention_mask=attention_mask,
            segment_ids=segment_ids,
            error_positions=error_positions,
        )
        return output.operation_logits, output.token_logits, output.position_logits


@dataclass
class ParityCaseResult:
    case_name: str
    batch_size: int
    seq_len: int
    op_max_abs_diff: float
    tok_max_abs_diff: float
    pos_max_abs_diff: float
    predictions_match: bool


@dataclass
class ParityReport:
    is_equivalent: bool
    num_cases_tested: int
    max_op_abs_diff: float
    max_tok_abs_diff: float
    max_pos_abs_diff: float
    all_predictions_match: bool
    case_results: List[ParityCaseResult]
    opset_version: int
    onnx_file_size_bytes: int


def export_model_to_onnx(
    checkpoint_path: Union[str, Path],
    output_onnx_path: Union[str, Path],
    config: Optional[ModelConfig] = None,
    opset_version: int = 18,
    dynamic_axes: bool = True,
    verbose: bool = False,
) -> Path:
    """Exports a trained RepairModel checkpoint to ONNX and validates graph integrity."""
    chk_path = Path(checkpoint_path)
    if not chk_path.exists():
        raise FileNotFoundError(f"Checkpoint file not found: '{chk_path}'")

    out_path = Path(output_onnx_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Load model from checkpoint
    model, chk_info = CheckpointManager.load_checkpoint(chk_path, device=torch.device("cpu"))
    model.eval()

    if config is None:
        config = model.config

    wrapper = ONNXExportWrapper(model)
    wrapper.eval()

    # Create dummy inputs for tracing
    batch_size = 2
    seq_len = config.max_sequence_length
    dummy_input_ids = torch.zeros((batch_size, seq_len), dtype=torch.long)
    dummy_attention_mask = torch.ones((batch_size, seq_len), dtype=torch.long)
    dummy_segment_ids = torch.ones((batch_size, seq_len), dtype=torch.long)
    dummy_error_positions = torch.zeros(batch_size, dtype=torch.long)

    input_names = ["input_ids", "attention_mask", "segment_ids", "error_positions"]
    output_names = ["operation_logits", "token_logits", "position_logits"]

    dynamic_shapes = None
    if dynamic_axes:
        batch_dim = torch.export.Dim("batch_size", min=1, max=64)
        seq_dim = torch.export.Dim("sequence_length", min=1, max=config.max_sequence_length)
        dynamic_shapes = {
            "input_ids": {0: batch_dim, 1: seq_dim},
            "attention_mask": {0: batch_dim, 1: seq_dim},
            "segment_ids": {0: batch_dim, 1: seq_dim},
            "error_positions": {0: batch_dim},
        }

    with torch.no_grad():
        torch.onnx.export(
            wrapper,
            (
                dummy_input_ids,
                dummy_attention_mask,
                dummy_segment_ids,
                dummy_error_positions,
            ),
            str(out_path),
            export_params=True,
            opset_version=opset_version,
            input_names=input_names,
            output_names=output_names,
            dynamic_shapes=dynamic_shapes,
        )

    # Validate ONNX model graph structure
    onnx_model = onnx.load(str(out_path))
    onnx.checker.check_model(onnx_model)

    if verbose:
        print(f"[ONNX EXPORT] Successfully exported model to '{out_path}' (Opset {opset_version}).")

    return out_path


def verify_onnx_numerical_parity(
    model: RepairModel,
    onnx_path: Union[str, Path],
    atol: float = 1e-4,
    rtol: float = 1e-4,
    seed: int = 42,
) -> ParityReport:
    """Validates that ONNX Runtime inference is numerically equivalent to PyTorch model."""
    onnx_path = Path(onnx_path)
    if not onnx_path.exists():
        raise FileNotFoundError(f"ONNX model file not found: '{onnx_path}'")

    model.eval()
    wrapper = ONNXExportWrapper(model)
    wrapper.eval()

    # Load ONNX Runtime session with CPU execution provider
    session_options = ort.SessionOptions()
    session_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    session = ort.InferenceSession(
        str(onnx_path),
        sess_options=session_options,
        providers=["CPUExecutionProvider"],
    )

    # Define diverse test configurations (batch sizes and sequence lengths)
    test_configs = [
        ("single_short", 1, 16, 5),
        ("single_medium", 1, 32, 12),
        ("single_full", 1, model.config.max_sequence_length, 25),
        ("batch_short", 2, 20, 8),
        ("batch_full", 4, model.config.max_sequence_length, 30),
    ]

    torch.manual_seed(seed)
    np.random.seed(seed)

    case_results: List[ParityCaseResult] = []
    max_op_diff = 0.0
    max_tok_diff = 0.0
    max_pos_diff = 0.0
    all_match = True

    for name, b_sz, seq_l, err_p in test_configs:
        input_ids = torch.randint(0, model.config.vocab_size, (b_sz, seq_l), dtype=torch.long)
        attention_mask = torch.ones((b_sz, seq_l), dtype=torch.long)
        segment_ids = torch.randint(1, 4, (b_sz, seq_l), dtype=torch.long)
        error_positions = torch.full((b_sz,), min(err_p, seq_l - 1), dtype=torch.long)

        # 1. PyTorch forward pass
        with torch.no_grad():
            py_op, py_tok, py_pos = wrapper(
                input_ids=input_ids,
                attention_mask=attention_mask,
                segment_ids=segment_ids,
                error_positions=error_positions,
            )

        # 2. ONNX Runtime forward pass
        ort_inputs = {
            "input_ids": input_ids.numpy(),
            "attention_mask": attention_mask.numpy(),
            "segment_ids": segment_ids.numpy(),
            "error_positions": error_positions.numpy(),
        }
        ort_outputs = session.run(None, ort_inputs)
        ort_op, ort_tok, ort_pos = ort_outputs[0], ort_outputs[1], ort_outputs[2]

        # Numerical comparison
        op_diff = float(np.max(np.abs(py_op.numpy() - ort_op)))
        tok_diff = float(np.max(np.abs(py_tok.numpy() - ort_tok)))
        pos_diff = float(np.max(np.abs(py_pos.numpy() - ort_pos)))

        max_op_diff = max(max_op_diff, op_diff)
        max_tok_diff = max(max_tok_diff, tok_diff)
        max_pos_diff = max(max_pos_diff, pos_diff)

        # Verify argmax prediction equality
        py_pred_op = torch.argmax(py_op, dim=-1).numpy()
        ort_pred_op = np.argmax(ort_op, axis=-1)
        py_pred_tok = torch.argmax(py_tok, dim=-1).numpy()
        ort_pred_tok = np.argmax(ort_tok, axis=-1)
        py_pred_pos = torch.argmax(py_pos, dim=-1).numpy()
        ort_pred_pos = np.argmax(ort_pos, axis=-1)

        preds_match = (
            np.array_equal(py_pred_op, ort_pred_op)
            and np.array_equal(py_pred_tok, ort_pred_tok)
            and np.array_equal(py_pred_pos, ort_pred_pos)
        )
        if not preds_match:
            all_match = False

        case_results.append(
            ParityCaseResult(
                case_name=name,
                batch_size=b_sz,
                seq_len=seq_l,
                op_max_abs_diff=round(op_diff, 7),
                tok_max_abs_diff=round(tok_diff, 7),
                pos_max_abs_diff=round(pos_diff, 7),
                predictions_match=preds_match,
            )
        )

    is_equivalent = (
        max_op_diff <= atol and max_tok_diff <= atol and max_pos_diff <= atol and all_match
    )
    onnx_file_size = onnx_path.stat().st_size
    onnx_model = onnx.load(str(onnx_path))
    opset_v = onnx_model.opset_import[0].version if onnx_model.opset_import else 18

    return ParityReport(
        is_equivalent=is_equivalent,
        num_cases_tested=len(test_configs),
        max_op_abs_diff=round(max_op_diff, 7),
        max_tok_abs_diff=round(max_tok_diff, 7),
        max_pos_abs_diff=round(max_pos_diff, 7),
        all_predictions_match=all_match,
        case_results=case_results,
        opset_version=opset_v,
        onnx_file_size_bytes=onnx_file_size,
    )
