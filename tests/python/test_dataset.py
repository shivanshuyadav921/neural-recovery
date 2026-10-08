import json
import tempfile
from pathlib import Path

import pytest
from ml.compiler_interface import get_tokens, is_valid_neurolang
from ml.corruption.corruptions import (
    CORRUPTION_REGISTRY,
    MissingClosingBraceCorruption,
    MissingClosingParenCorruption,
    MissingCommaCorruption,
    MissingSemicolonCorruption,
    ReplaceDelimiterCorruption,
    UnexpectedTokenCorruption,
)
from ml.dataset.generator import DatasetGenerator
from ml.dataset.schema import (
    ContextRecord,
    DatasetSample,
    ErrorRecord,
    RepairLabel,
)
from ml.dataset.splitter import DatasetSplitter
from ml.dataset.validator import DatasetValidator


@pytest.fixture
def sample_valid_program():
    return (
        "function compute(a, b) {\n"
        "    let x = a + b;\n"
        "    return x;\n"
        "}\n"
        "let result = compute(1, 2);\n"
    )


def test_missing_semicolon_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = MissingSemicolonCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 2

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "INSERT_TOKEN"
    assert repair.token == ";"

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_missing_closing_paren_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = MissingClosingParenCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 2

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "INSERT_TOKEN"
    assert repair.token == ")"

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_missing_closing_brace_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = MissingClosingBraceCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 1

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "INSERT_TOKEN"
    assert repair.token == "}"

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_missing_comma_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = MissingCommaCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 2

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "INSERT_TOKEN"
    assert repair.token == ","

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_replace_delimiter_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = ReplaceDelimiterCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 2

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "REPLACE_TOKEN"
    assert repair.replaced_token == "]"

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_unexpected_token_corruption(sample_valid_program):
    tokens = get_tokens(sample_valid_program)
    strat = UnexpectedTokenCorruption()
    candidates = strat.find_candidates(tokens, sample_valid_program)
    assert len(candidates) >= 2

    corrupted, repair = strat.apply(candidates[0], tokens, sample_valid_program)
    assert corrupted != sample_valid_program
    assert repair.operation == "DELETE_TOKEN"
    assert repair.token == ";"

    repaired = strat.apply_repair(corrupted, repair)
    assert is_valid_neurolang(repaired)


def test_dataset_sample_serialization():
    sample = DatasetSample(
        sample_id="test_id_123",
        source_file="test.nl",
        original_source="let x = 10;",
        corrupted_source="let x = 10",
        corruption_type="missing_semicolon",
        repair=RepairLabel(operation="INSERT_TOKEN", token=";", position=3),
        error=ErrorRecord(token_index=3, line=1, column=10, message="Expected ';'"),
        context=ContextRecord(tokens_before=[], current_token={}, tokens_after=[]),
    )

    data = sample.to_dict()
    assert data["sample_id"] == "test_id_123"
    assert data["repair"]["operation"] == "INSERT_TOKEN"

    deserialized = DatasetSample.from_dict(data)
    assert deserialized.sample_id == sample.sample_id
    assert deserialized.repair.token == sample.repair.token


def test_deterministic_generator_seed():
    valid_files = sorted(list(Path("examples/valid").glob("*.nl")))
    assert len(valid_files) >= 1

    gen1 = DatasetGenerator(seed=123)
    samples1 = gen1.generate_from_files(valid_files, target_samples=20)

    gen2 = DatasetGenerator(seed=123)
    samples2 = gen2.generate_from_files(valid_files, target_samples=20)

    assert len(samples1) == len(samples2)
    for s1, s2 in zip(samples1, samples2):
        assert s1.sample_id == s2.sample_id
        assert s1.corrupted_source == s2.corrupted_source
        assert s1.corruption_type == s2.corruption_type


def test_splitter_leakage_prevention():
    # Create fake samples originating from 4 distinct files
    samples = []
    for file_idx in range(4):
        file_name = f"source_{file_idx}.nl"
        for i in range(5):
            samples.append(
                DatasetSample(
                    sample_id=f"sample_{file_idx}_{i}",
                    source_file=file_name,
                    original_source="let x = 1;",
                    corrupted_source="let x = 1",
                    corruption_type="missing_semicolon",
                    repair=RepairLabel(operation="INSERT_TOKEN", token=";", position=3),
                    error=ErrorRecord(token_index=3, line=1, column=9, message="err"),
                    context=ContextRecord(tokens_before=[], current_token={}, tokens_after=[]),
                )
            )

    splitter = DatasetSplitter(train_ratio=0.5, validation_ratio=0.25, test_ratio=0.25, seed=42)
    train, val, test = splitter.split(samples)

    train_sources = {s.source_file for s in train}
    val_sources = {s.source_file for s in val}
    test_sources = {s.source_file for s in test}

    # Strict leakage prevention: disjoint source files
    assert train_sources.isdisjoint(val_sources)
    assert train_sources.isdisjoint(test_sources)
    assert val_sources.isdisjoint(test_sources)


def test_invalid_source_rejection():
    with tempfile.NamedTemporaryFile("w", suffix=".nl", delete=False, encoding="utf-8") as f:
        f.write("let x = + ;\n")  # Syntax error
        temp_path = Path(f.name)

    try:
        gen = DatasetGenerator(seed=42)
        samples = gen.generate_from_files([temp_path])
        assert len(samples) == 0
        assert gen.rejection_stats["invalid_base_source"] == 1
    finally:
        if temp_path.exists():
            temp_path.unlink()


def test_dataset_validator_success():
    dataset_dir = Path("ml/dataset/generated")
    if dataset_dir.exists():
        validator = DatasetValidator(check_compiler_roundtrip=True)
        res = validator.validate_directory(dataset_dir)
        assert res["valid"] is True
        assert res["total_samples"] > 0


def test_unsupported_corruption_type():
    with pytest.raises(ValueError, match="Unsupported corruption type"):
        DatasetGenerator(corruption_types=["unknown_type"])


def test_zero_requested_samples():
    valid_files = sorted(list(Path("examples/valid").glob("*.nl")))
    gen = DatasetGenerator(seed=42)
    samples = gen.generate_from_files(valid_files, target_samples=0)
    assert len(samples) == 0


def test_insufficient_source_programs():
    gen = DatasetGenerator(seed=42)
    samples = gen.generate_from_files([], target_samples=10)
    assert len(samples) == 0


def test_validator_catches_leakage(tmp_path):
    train_file = tmp_path / "train.jsonl"
    val_file = tmp_path / "validation.jsonl"
    test_file = tmp_path / "test.jsonl"

    sample_leaked = DatasetSample(
        sample_id="s1",
        source_file="shared.nl",
        original_source="let x = 1;",
        corrupted_source="let x = 1",
        corruption_type="missing_semicolon",
        repair=RepairLabel(operation="INSERT_TOKEN", token=";", position=3),
        error=ErrorRecord(token_index=3, line=1, column=9, message="err"),
        context=ContextRecord(tokens_before=[], current_token={}, tokens_after=[]),
    )

    with open(train_file, "w", encoding="utf-8") as f:
        f.write(json.dumps(sample_leaked.to_dict()) + "\n")
    with open(val_file, "w", encoding="utf-8") as f:
        f.write(json.dumps(sample_leaked.to_dict()) + "\n")
    with open(test_file, "w", encoding="utf-8") as f:
        pass

    validator = DatasetValidator(check_compiler_roundtrip=False)
    res = validator.validate_directory(tmp_path)
    assert res["valid"] is False
    assert any("leakage" in err.lower() for err in res["errors"])
