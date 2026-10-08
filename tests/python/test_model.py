import json
from pathlib import Path

import pytest
import torch

from ml.dataset.schema import ContextRecord, DatasetSample, RepairLabel
from ml.models.config import ModelConfig
from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.encoder import ContextEncoder
from ml.models.heads import MultiTaskRepairLoss
from ml.models.repair_model import RepairModel
from ml.models.sanity_check import run_sanity_check
from ml.models.vocabulary import Vocabulary


@pytest.fixture
def sample_config():
    vocab = Vocabulary()
    return ModelConfig(
        vocab_size=vocab.size,
        embedding_dim=32,
        num_layers=2,
        num_heads=4,
        feed_forward_dim=64,
        dropout=0.1,
        max_sequence_length=32,
        num_operations=vocab.num_operations,
        num_token_types=vocab.size,
    )


@pytest.fixture
def sample_context():
    return ContextRecord(
        tokens_before=[
            {"type": "let", "lexeme": "let"},
            {"type": "IDENTIFIER", "lexeme": "x"},
            {"type": "=", "lexeme": "="},
            {"type": "INTEGER", "lexeme": "10"},
        ],
        current_token={"type": "EOF", "lexeme": "EOF"},
        tokens_after=[],
        expected_tokens=[";"],
        construct="let binding",
    )


def test_model_config_valid(sample_config):
    assert sample_config.vocab_size > 0
    assert sample_config.embedding_dim % sample_config.num_heads == 0


def test_model_config_invalid():
    with pytest.raises(ValueError, match="vocab_size must be positive"):
        ModelConfig(vocab_size=0)

    with pytest.raises(ValueError, match="must be divisible"):
        ModelConfig(embedding_dim=33, num_heads=4)

    with pytest.raises(ValueError, match="dropout must be"):
        ModelConfig(dropout=1.0)


def test_model_config_serialization(sample_config):
    json_str = sample_config.to_json()
    loaded = ModelConfig.from_json(json_str)
    assert loaded.embedding_dim == sample_config.embedding_dim
    assert loaded.num_layers == sample_config.num_layers


def test_vocabulary_encoding():
    vocab = Vocabulary()
    assert vocab.size > 30
    assert vocab.pad_id == 0

    tok_id = vocab.encode_token({"type": "function", "lexeme": "function"})
    assert vocab.decode_token(tok_id) == "function"

    op_id = vocab.encode_operation("INSERT_TOKEN")
    assert vocab.decode_operation(op_id) == "INSERT_TOKEN"


def test_vocabulary_normalization():
    vocab = Vocabulary()
    assert vocab.normalize_token({"type": "IDENTIFIER", "lexeme": "my_var_name"}) == "IDENTIFIER"
    assert vocab.normalize_token({"type": "INTEGER", "lexeme": "42"}) == "INTEGER"
    assert vocab.normalize_token({"type": "UNKNOWN", "lexeme": "$$$"}) == "<UNK>"


def test_context_encoder(sample_config, sample_context):
    encoder = ContextEncoder(sample_config)
    encoded = encoder.encode_context(
        sample_context,
        repair=RepairLabel(operation="INSERT_TOKEN", token=";", position=4),
    )

    assert encoded.input_ids.shape[0] == sample_config.max_sequence_length
    assert encoded.attention_mask.shape[0] == sample_config.max_sequence_length
    assert encoded.segment_ids.shape[0] == sample_config.max_sequence_length
    assert encoded.target_operation is not None
    assert encoded.target_token is not None


def test_model_forward_shapes(sample_config):
    model = RepairModel(sample_config)
    batch_size = 3
    seq_len = sample_config.max_sequence_length

    input_ids = torch.randint(0, sample_config.vocab_size, (batch_size, seq_len))
    attention_mask = torch.ones(batch_size, seq_len, dtype=torch.long)

    output = model(input_ids=input_ids, attention_mask=attention_mask)

    assert output.operation_logits.shape == (batch_size, sample_config.num_operations)
    assert output.token_logits.shape == (batch_size, sample_config.num_token_types)
    assert output.position_logits.shape == (batch_size, sample_config.max_sequence_length)
    assert output.confidence.shape == (batch_size,)
    assert not torch.isnan(output.operation_logits).any()


def test_model_backward_and_loss(sample_config):
    model = RepairModel(sample_config)
    batch_size = 2
    seq_len = sample_config.max_sequence_length

    input_ids = torch.randint(0, sample_config.vocab_size, (batch_size, seq_len))
    attention_mask = torch.ones(batch_size, seq_len, dtype=torch.long)
    target_ops = torch.tensor([0, 1], dtype=torch.long)
    target_toks = torch.tensor([5, 6], dtype=torch.long)
    target_pos = torch.tensor([2, 3], dtype=torch.long)

    output = model(
        input_ids=input_ids,
        attention_mask=attention_mask,
        target_operations=target_ops,
        target_tokens=target_toks,
        target_positions=target_pos,
    )

    assert output.loss is not None
    assert not torch.isnan(output.loss)
    assert not torch.isinf(output.loss)

    output.loss.backward()
    grads = [p.grad for p in model.parameters() if p.requires_grad]
    assert any(g is not None and g.norm().item() > 0 for g in grads)


def test_model_eval_determinism(sample_config):
    model = RepairModel(sample_config)
    model.eval()

    input_ids = torch.randint(0, sample_config.vocab_size, (2, sample_config.max_sequence_length))
    attention_mask = torch.ones_like(input_ids)

    with torch.no_grad():
        out1 = model(input_ids=input_ids, attention_mask=attention_mask)
        out2 = model(input_ids=input_ids, attention_mask=attention_mask)

    assert torch.equal(out1.operation_logits, out2.operation_logits)
    assert torch.equal(out1.token_logits, out2.token_logits)


def test_model_size_report(sample_config):
    model = RepairModel(sample_config)
    size_info = model.get_model_size()

    assert "total_parameters" in size_info
    assert "trainable_parameters" in size_info
    assert "fp32_memory_mb" in size_info
    assert size_info["total_parameters"] > 0
    assert size_info["fp32_memory_mb"] > 0


def test_sanity_check_function():
    dataset_path = Path("ml/dataset/generated/train.jsonl")
    if dataset_path.exists():
        assert run_sanity_check(dataset_path) is True
