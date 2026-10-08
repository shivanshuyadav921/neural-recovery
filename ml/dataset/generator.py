import hashlib
import random
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from ml.compiler_interface import (
    get_diagnostics_and_contexts,
    get_tokens,
    is_valid_neurolang,
)
from ml.corruption.corruptions import CORRUPTION_REGISTRY, CorruptionStrategy
from ml.dataset.schema import (
    ContextRecord,
    DatasetSample,
    ErrorRecord,
    RepairLabel,
)


class DatasetGenerator:
    def __init__(
        self,
        seed: int = 42,
        corruption_types: Optional[List[str]] = None,
        context_before: int = 50,
        context_after: int = 10,
    ):
        self.seed = seed
        self.rng = random.Random(seed)
        self.context_before = context_before
        self.context_after = context_after

        if corruption_types is None:
            self.strategies: Dict[str, CorruptionStrategy] = dict(CORRUPTION_REGISTRY)
        else:
            for name in corruption_types:
                if name not in CORRUPTION_REGISTRY:
                    raise ValueError(f"Unsupported corruption type '{name}'. Available: {list(CORRUPTION_REGISTRY.keys())}")
            self.strategies = {
                name: CORRUPTION_REGISTRY[name]
                for name in corruption_types
            }

        self.rejection_stats: Dict[str, int] = {
            "invalid_base_source": 0,
            "corruption_did_not_error": 0,
            "no_context_extracted": 0,
            "roundtrip_repair_failed": 0,
            "duplicate_sample": 0,
        }

    def generate_from_files(
        self,
        file_paths: List[Path],
        target_samples: Optional[int] = None,
    ) -> List[DatasetSample]:
        if target_samples is not None and target_samples <= 0:
            return []

        # Reset RNG with seed for reproducibility
        self.rng = random.Random(self.seed)
        samples: List[DatasetSample] = []
        seen_sample_ids = set()

        # Step 1: Read and validate all input source programs
        valid_sources: List[Tuple[str, str]] = []  # (source_name, source_code)
        for path in file_paths:
            content = path.read_text(encoding="utf-8")
            if is_valid_neurolang(content):
                valid_sources.append((path.name, content))
            else:
                self.rejection_stats["invalid_base_source"] += 1

        if not valid_sources:
            return []

        # Step 2: Iterate across corruption strategies and sources
        for strat_name, strategy in self.strategies.items():
            for source_name, source_text in valid_sources:
                tokens = get_tokens(source_text)
                if not tokens:
                    continue

                candidates = strategy.find_candidates(tokens, source_text)
                # Shuffle candidates deterministically
                self.rng.shuffle(candidates)

                for cand in candidates:
                    corrupted_source, repair = strategy.apply(cand, tokens, source_text)
                    if corrupted_source == source_text:
                        continue

                    # Confirm corrupted program produces syntax error
                    if is_valid_neurolang(corrupted_source):
                        self.rejection_stats["corruption_did_not_error"] += 1
                        continue

                    # Extract context & diagnostic
                    errors, contexts = get_diagnostics_and_contexts(corrupted_source)
                    if not errors or not contexts:
                        self.rejection_stats["no_context_extracted"] += 1
                        continue

                    err_dict = errors[0]
                    ctx_dict = contexts[0]

                    # Round-trip repair verification
                    reconstructed = strategy.apply_repair(corrupted_source, repair)
                    if not is_valid_neurolang(reconstructed):
                        self.rejection_stats["roundtrip_repair_failed"] += 1
                        continue

                    # Deterministic sample ID
                    sample_id_data = f"{source_name}:{strat_name}:{repair.position}:{repair.token}:{corrupted_source}"
                    sample_id = hashlib.sha256(sample_id_data.encode("utf-8")).hexdigest()[:16]

                    if sample_id in seen_sample_ids:
                        self.rejection_stats["duplicate_sample"] += 1
                        continue
                    seen_sample_ids.add(sample_id)

                    sample = DatasetSample(
                        sample_id=sample_id,
                        source_file=source_name,
                        original_source=source_text,
                        corrupted_source=corrupted_source,
                        corruption_type=strat_name,
                        repair=repair,
                        error=ErrorRecord(
                            token_index=err_dict["token_index"],
                            line=err_dict["line"],
                            column=err_dict["column"],
                            message=err_dict["message"],
                            expected_tokens=err_dict.get("expected_tokens", []),
                            construct=err_dict.get("construct"),
                        ),
                        context=ContextRecord(
                            tokens_before=ctx_dict.get("tokens_before", []),
                            current_token=ctx_dict.get("current_token", {}),
                            tokens_after=ctx_dict.get("tokens_after", []),
                            expected_tokens=ctx_dict.get("expected_tokens", []),
                            construct=ctx_dict.get("construct"),
                        ),
                    )

                    samples.append(sample)
                    if target_samples is not None and len(samples) >= target_samples:
                        return samples

        return samples
