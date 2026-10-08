"""Phase 11: ONNX Model Export, Graph Validation, and Numerical Equivalence Tests."""

from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import pytest
import torch

from ml.export.onnx_exporter import (
    ONNXExportWrapper,
    export_model_to_onnx,
    verify_onnx_numerical_parity,
)
from ml.models.config import ModelConfig
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary
from ml.training.checkpoint import CheckpointManager


@pytest.fixture
def vocab() -> Vocabulary:
    return Vocabulary()


@pytest.fixture
def small_config(vocab: Vocabulary) -> ModelConfig:
    return ModelConfig(
        vocab_size=vocab.size,
        embedding_dim=32,
        num_layers=2,
        num_heads=2,
        feed_forward_dim=64,
        max_sequence_length=64,
        dropout=0.0,
    )


@pytest.fixture
def trained_checkpoint_path() -> Path:
    chk = Path("artifacts/training/checkpoints/best_checkpoint.pt")
    if not chk.exists():
        pytest.skip("Trained checkpoint not found at 'artifacts/training/checkpoints/best_checkpoint.pt'")
    return chk


@pytest.fixture
def exported_onnx_model(trained_checkpoint_path: Path, tmp_path: Path) -> Path:
    out_path = tmp_path / "model.onnx"
    export_model_to_onnx(
        checkpoint_path=trained_checkpoint_path,
        output_onnx_path=out_path,
        opset_version=18,
    )
    return out_path


def test_checkpoint_loading_and_reconstruction(trained_checkpoint_path: Path):
    """Verifies that the trained Phase 10 checkpoint can be loaded and model reconstructed."""
    model, info = CheckpointManager.load_checkpoint(
        trained_checkpoint_path, device=torch.device("cpu")
    )
    assert isinstance(model, RepairModel)
    assert model.config is not None
    assert info["epoch"] >= 0
    assert "best_metric" in info or "val_loss" in info or "val_total_loss" in info


def test_onnx_export_wrapper(small_config: ModelConfig):
    """Verifies that the ONNXExportWrapper properly forwards inputs and returns raw logit tuple."""
    model = RepairModel(small_config)
    model.eval()
    wrapper = ONNXExportWrapper(model)
    wrapper.eval()

    batch_size = 2
    seq_len = 16
    input_ids = torch.randint(0, small_config.vocab_size, (batch_size, seq_len))
    attention_mask = torch.ones((batch_size, seq_len), dtype=torch.long)
    segment_ids = torch.ones((batch_size, seq_len), dtype=torch.long)
    error_positions = torch.zeros(batch_size, dtype=torch.long)

    with torch.no_grad():
        op_logits, tok_logits, pos_logits = wrapper(
            input_ids=input_ids,
            attention_mask=attention_mask,
            segment_ids=segment_ids,
            error_positions=error_positions,
        )

    assert op_logits.shape == (batch_size, small_config.num_operations)
    assert tok_logits.shape == (batch_size, small_config.vocab_size)
    assert pos_logits.shape == (batch_size, small_config.max_sequence_length)


def test_onnx_graph_validation(exported_onnx_model: Path):
    """Validates the ONNX graph structure using onnx.checker."""
    assert exported_onnx_model.exists()
    assert exported_onnx_model.stat().st_size > 0

    onnx_model = onnx.load(str(exported_onnx_model))
    onnx.checker.check_model(onnx_model)

    assert len(onnx_model.graph.input) == 4
    input_names = [inp.name for inp in onnx_model.graph.input]
    assert "input_ids" in input_names
    assert "attention_mask" in input_names
    assert "segment_ids" in input_names
    assert "error_positions" in input_names

    assert len(onnx_model.graph.output) == 3
    output_names = [out.name for out in onnx_model.graph.output]
    assert "operation_logits" in output_names
    assert "token_logits" in output_names
    assert "position_logits" in output_names


def test_onnxruntime_cpu_inference(exported_onnx_model: Path, vocab: Vocabulary):
    """Verifies that ONNX Runtime can load the model on CPU and run forward inference."""
    session = ort.InferenceSession(
        str(exported_onnx_model),
        providers=["CPUExecutionProvider"],
    )

    batch_size = 1
    seq_len = 32
    ort_inputs = {
        "input_ids": np.zeros((batch_size, seq_len), dtype=np.int64),
        "attention_mask": np.ones((batch_size, seq_len), dtype=np.int64),
        "segment_ids": np.ones((batch_size, seq_len), dtype=np.int64),
        "error_positions": np.array([5], dtype=np.int64),
    }

    outputs = session.run(None, ort_inputs)
    assert len(outputs) == 3

    op_logits, tok_logits, pos_logits = outputs
    assert op_logits.shape == (1, 4)
    assert tok_logits.shape == (1, vocab.size)
    assert pos_logits.shape == (1, 128)


def test_dynamic_sequence_and_batch_dimensions(exported_onnx_model: Path, vocab: Vocabulary):
    """Verifies that the ONNX model supports dynamic batch sizes and dynamic sequence lengths."""
    session = ort.InferenceSession(
        str(exported_onnx_model),
        providers=["CPUExecutionProvider"],
    )

    test_shapes = [
        (1, 10),
        (2, 25),
        (4, 64),
        (3, 128),
    ]

    for b, s in test_shapes:
        ort_inputs = {
            "input_ids": np.random.randint(0, vocab.size, size=(b, s), dtype=np.int64),
            "attention_mask": np.ones((b, s), dtype=np.int64),
            "segment_ids": np.ones((b, s), dtype=np.int64),
            "error_positions": np.full((b,), min(2, s - 1), dtype=np.int64),
        }
        outputs = session.run(None, ort_inputs)
        assert outputs[0].shape == (b, 4)
        assert outputs[1].shape == (b, vocab.size)
        assert outputs[2].shape == (b, 128)


def test_numerical_parity_and_prediction_match(
    trained_checkpoint_path: Path, exported_onnx_model: Path
):
    """Verifies numerical equivalence and structured prediction equivalence between PyTorch and ONNX Runtime."""
    model, _ = CheckpointManager.load_checkpoint(
        trained_checkpoint_path, device=torch.device("cpu")
    )
    report = verify_onnx_numerical_parity(
        model=model,
        onnx_path=exported_onnx_model,
        atol=1e-4,
        rtol=1e-4,
        seed=1337,
    )

    assert report.is_equivalent
    assert report.all_predictions_match
    assert report.max_op_abs_diff < 1e-4
    assert report.max_tok_abs_diff < 1e-4
    assert report.max_pos_abs_diff < 1e-4
    assert report.num_cases_tested == 5

    for case in report.case_results:
        assert case.predictions_match
        assert case.op_max_abs_diff < 1e-4
        assert case.tok_max_abs_diff < 1e-4
        assert case.pos_max_abs_diff < 1e-4


def test_deterministic_repeated_inference(exported_onnx_model: Path, vocab: Vocabulary):
    """Verifies that repeated inference with identical inputs produces bitwise identical results."""
    session = ort.InferenceSession(
        str(exported_onnx_model),
        providers=["CPUExecutionProvider"],
    )

    np.random.seed(999)
    ort_inputs = {
        "input_ids": np.random.randint(0, vocab.size, size=(2, 48), dtype=np.int64),
        "attention_mask": np.ones((2, 48), dtype=np.int64),
        "segment_ids": np.ones((2, 48), dtype=np.int64),
        "error_positions": np.array([10, 20], dtype=np.int64),
    }

    out1 = session.run(None, ort_inputs)
    out2 = session.run(None, ort_inputs)

    for arr1, arr2 in zip(out1, out2):
        np.testing.assert_array_equal(arr1, arr2)


def test_export_error_handling_missing_file():
    """Verifies that missing checkpoint files raise FileNotFoundError."""
    with pytest.raises(FileNotFoundError):
        export_model_to_onnx(
            checkpoint_path="nonexistent/checkpoint.pt",
            output_onnx_path="test.onnx",
        )
