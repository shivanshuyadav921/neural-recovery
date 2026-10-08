import json
from pathlib import Path
from typing import Any, Dict, List, Set

from ml.compiler_interface import is_valid_neurolang
from ml.corruption.corruptions import CORRUPTION_REGISTRY
from ml.dataset.schema import DatasetSample


class DatasetValidator:
    def __init__(self, check_compiler_roundtrip: bool = True):
        self.check_compiler_roundtrip = check_compiler_roundtrip

    def validate_directory(self, dir_path: Path) -> Dict[str, Any]:
        errors: List[str] = []
        warnings: List[str] = []

        meta_path = dir_path / "metadata.json"
        if not meta_path.exists():
            errors.append("metadata.json not found.")

        split_files = ["train.jsonl", "validation.jsonl", "test.jsonl"]
        seen_sample_ids: Set[str] = set()
        split_sources: Dict[str, Set[str]] = {
            "train": set(),
            "validation": set(),
            "test": set(),
        }

        total_samples = 0
        samples_per_split: Dict[str, int] = {}

        for split_name in ["train", "validation", "test"]:
            file_path = dir_path / f"{split_name}.jsonl"
            if not file_path.exists():
                warnings.append(f"{split_name}.jsonl does not exist.")
                samples_per_split[split_name] = 0
                continue

            split_count = 0
            with open(file_path, "r", encoding="utf-8") as f:
                for line_idx, line in enumerate(f, 1):
                    line_str = line.strip()
                    if not line_str:
                        continue

                    try:
                        data = json.loads(line_str)
                    except json.JSONDecodeError as e:
                        errors.append(f"{split_name}.jsonl:{line_idx}: Invalid JSON: {e}")
                        continue

                    try:
                        sample = DatasetSample.from_dict(data)
                    except Exception as e:
                        errors.append(f"{split_name}.jsonl:{line_idx}: Schema violation: {e}")
                        continue

                    # Check unique ID
                    if sample.sample_id in seen_sample_ids:
                        errors.append(f"Duplicate sample_id: {sample.sample_id}")
                    seen_sample_ids.add(sample.sample_id)

                    # Track source file for leakage detection
                    if sample.source_file:
                        split_sources[split_name].add(sample.source_file)

                    # Verify roundtrip & compiler validity if requested
                    if self.check_compiler_roundtrip:
                        strat = CORRUPTION_REGISTRY.get(sample.corruption_type)
                        if strat:
                            repaired = strat.apply_repair(sample.corrupted_source, sample.repair)
                            if not is_valid_neurolang(repaired):
                                errors.append(
                                    f"Sample {sample.sample_id}: Ground-truth repair does not restore valid NeuroLang program."
                                )

                    split_count += 1

            samples_per_split[split_name] = split_count
            total_tokens = split_count
            total_samples += split_count

        # Leakage check: source files must not overlap between train and test/validation
        train_val_overlap = split_sources["train"] & split_sources["validation"]
        train_test_overlap = split_sources["train"] & split_sources["test"]
        val_test_overlap = split_sources["validation"] & split_sources["test"]

        if train_val_overlap:
            errors.append(f"Data leakage detected between train and validation: {train_val_overlap}")
        if train_test_overlap:
            errors.append(f"Data leakage detected between train and test: {train_test_overlap}")
        if val_test_overlap:
            errors.append(f"Data leakage detected between validation and test: {val_test_overlap}")

        is_valid = len(errors) == 0
        return {
            "valid": is_valid,
            "total_samples": total_samples,
            "samples_per_split": samples_per_split,
            "errors": errors,
            "warnings": warnings,
        }
