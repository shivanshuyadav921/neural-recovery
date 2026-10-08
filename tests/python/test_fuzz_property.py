import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest
import torch
from hypothesis import given, settings, strategies as st

from ml.dataset.generator import DatasetGenerator
from ml.dataset.schema import ContextRecord, DatasetSample, RepairLabel
from ml.models.config import ModelConfig
from ml.models.encoder import ContextEncoder
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary


COMPILER_BIN = Path(__file__).resolve().parent.parent.parent / "build" / "neuroparse.exe"
CORPUS_DIR = Path(__file__).resolve().parent.parent / "fuzz" / "corpus"


@pytest.fixture(scope="module")
def shared_vocab():
    return Vocabulary()


@pytest.fixture(scope="module")
def shared_model(shared_vocab):
    config = ModelConfig(
        vocab_size=shared_vocab.size,
        embedding_dim=32,
        num_layers=2,
        num_heads=4,
        feed_forward_dim=64,
        dropout=0.0,
        max_sequence_length=32,
        num_operations=shared_vocab.num_operations,
        num_token_types=shared_vocab.size,
    )
    model = RepairModel(config)
    model.eval()
    return model, config


# 1. Property: ContextEncoder with arbitrary token sequences
@given(
    token_types=st.lists(
        st.sampled_from(["let", "function", "return", "if", "else", "IDENTIFIER", "INTEGER", ";", "{", "}", "(", ")", "UNKNOWN_XYZ"]),
        min_size=0,
        max_size=50,
    )
)
@settings(max_examples=30, deadline=None)
def test_prop_encoder_arbitrary_tokens(shared_model, shared_vocab, token_types):
    _, config = shared_model
    tokens = [{"type": t, "lexeme": t} for t in token_types]
    context = ContextRecord(
        tokens_before=tokens,
        current_token={"type": "EOF", "lexeme": "EOF"},
        tokens_after=[],
        expected_tokens=[";"],
        construct="fuzz_test",
    )
    encoder = ContextEncoder(config=config, vocabulary=shared_vocab)
    encoded = encoder.encode_context(context)

    assert encoded.input_ids.shape == (32,)
    assert encoded.attention_mask.shape == (32,)
    assert (encoded.input_ids >= 0).all()
    assert (encoded.input_ids < shared_vocab.size).all()
    assert ((encoded.attention_mask == 0) | (encoded.attention_mask == 1)).all()


# 2. Property: Model forward pass on arbitrary sequence batches
@given(
    batch_size=st.integers(min_value=1, max_value=4),
    seq_len=st.integers(min_value=1, max_value=32),
)
@settings(max_examples=20, deadline=None)
def test_prop_model_forward_arbitrary_tensors(shared_model, shared_vocab, batch_size, seq_len):
    model, config = shared_model
    input_ids = torch.randint(0, shared_vocab.size, (batch_size, seq_len), dtype=torch.long)
    attention_mask = torch.ones((batch_size, seq_len), dtype=torch.long)

    with torch.no_grad():
        outputs = model(input_ids, attention_mask)

    assert hasattr(outputs, "operation_logits")
    assert hasattr(outputs, "token_logits")
    assert hasattr(outputs, "position_logits")

    assert outputs.operation_logits.shape == (batch_size, config.num_operations)
    assert outputs.token_logits.shape == (batch_size, config.num_token_types)
    assert outputs.position_logits.shape == (batch_size, config.max_sequence_length)
    assert not torch.isnan(outputs.operation_logits).any()


# 3. Property: Malformed model outputs decoding safety (NaNs, Infs)
def test_prop_malformed_model_output_decoding(shared_vocab):
    nan_logits = torch.full((1, shared_vocab.num_operations), float("nan"))
    pred_op = torch.argmax(nan_logits, dim=-1).item()
    assert 0 <= pred_op < shared_vocab.num_operations

    inf_logits = torch.tensor([[float("-inf"), 10.0, float("inf")]])
    pred_idx = torch.argmax(inf_logits, dim=-1).item()
    assert 0 <= pred_idx < 3


# 4. Property: Synthetic Dataset Generator Invariants on Seed Files
@given(seed=st.integers(min_value=1, max_value=1000))
@settings(max_examples=5, deadline=None)
def test_prop_dataset_generator_invariants(seed):
    seed_files = [CORPUS_DIR / "seed_01_minimal.nl", CORPUS_DIR / "seed_02_expressions.nl"]
    existing_seeds = [f for f in seed_files if f.exists()]
    if not existing_seeds:
        pytest.skip("Seed files not found")

    generator = DatasetGenerator(seed=seed)
    dataset = generator.generate_from_files(existing_seeds, target_samples=3)

    for sample in dataset:
        assert isinstance(sample, DatasetSample)
        assert sample.original_code
        assert sample.corrupted_code
        assert isinstance(sample.repair, RepairLabel)
        assert sample.repair.operation in ["INSERT_TOKEN", "DELETE_TOKEN", "REPLACE_TOKEN"]
        assert isinstance(sample.context, ContextRecord)


# 5. Property: Compiler CLI Robustness on arbitrary inputs
@given(
    content=st.text(
        alphabet=st.characters(blacklist_categories=("Cs",)),
        min_size=0,
        max_size=300,
    )
)
@settings(max_examples=25, deadline=None)
def test_prop_compiler_cli_random_inputs(content):
    if not COMPILER_BIN.exists():
        pytest.skip(f"Compiler executable not found at {COMPILER_BIN}")

    with tempfile.NamedTemporaryFile(mode="w", suffix=".nl", delete=False, encoding="utf-8") as f:
        f.write(content)
        temp_path = f.name

    try:
        proc = subprocess.run(
            [str(COMPILER_BIN), temp_path, "--diagnostics=json", "--recovery=auto"],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        # Property: Compiler must exit with 0 or 1, never crash with unhandled exception or abort
        assert proc.returncode in (0, 1)

        # Property: If JSON is emitted, it must be valid JSON
        out = proc.stdout.strip()
        if out.startswith("{"):
            parsed = json.loads(out)
            assert "diagnostics" in parsed or "success" in parsed
    finally:
        Path(temp_path).unlink(missing_ok=True)
