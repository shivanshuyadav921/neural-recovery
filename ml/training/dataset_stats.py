import json
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from ml.dataset.schema import DatasetSample


@dataclass
class SplitStats:
    num_samples: int
    corruption_distribution: Dict[str, int]
    operation_distribution: Dict[str, int]
    token_distribution: Dict[str, int]
    position_min: int
    position_max: int
    context_length_min: int
    context_length_max: int
    context_length_mean: float
    source_files: List[str]


@dataclass
class DatasetStatsReport:
    train_stats: SplitStats
    validation_stats: SplitStats
    test_stats: SplitStats
    total_samples: int
    is_leakage_free: bool
    leakage_errors: List[str]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_samples": self.total_samples,
            "is_leakage_free": self.is_leakage_free,
            "leakage_errors": self.leakage_errors,
            "train": asdict(self.train_stats),
            "validation": asdict(self.validation_stats),
            "test": asdict(self.test_stats),
        }


class DatasetAnalyzer:
    """Analyzes Phase 8 JSONL datasets to compute verifiable statistics and check for leakage."""

    @staticmethod
    def analyze_split(jsonl_path: Path) -> SplitStats:
        samples: List[DatasetSample] = []
        if not jsonl_path.exists():
            return SplitStats(
                num_samples=0,
                corruption_distribution={},
                operation_distribution={},
                token_distribution={},
                position_min=0,
                position_max=0,
                context_length_min=0,
                context_length_max=0,
                context_length_mean=0.0,
                source_files=[],
            )

        with open(jsonl_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    samples.append(DatasetSample.from_dict(json.loads(line)))

        corruptions = Counter(s.corruption_type for s in samples)
        operations = Counter(s.repair.operation for s in samples)
        tokens = Counter(s.repair.token for s in samples if s.repair.token)

        positions = [s.repair.position for s in samples]
        pos_min = min(positions) if positions else 0
        pos_max = max(positions) if positions else 0

        context_lengths = [
            len(s.context.tokens_before) + len(s.context.tokens_after) + (1 if s.context.current_token else 0)
            for s in samples
        ]
        ctx_min = min(context_lengths) if context_lengths else 0
        ctx_max = max(context_lengths) if context_lengths else 0
        ctx_mean = sum(context_lengths) / len(context_lengths) if context_lengths else 0.0

        sources = sorted(list({s.source_file for s in samples}))

        return SplitStats(
            num_samples=len(samples),
            corruption_distribution=dict(corruptions),
            operation_distribution=dict(operations),
            token_distribution=dict(tokens),
            position_min=pos_min,
            position_max=pos_max,
            context_length_min=ctx_min,
            context_length_max=ctx_max,
            context_length_mean=round(ctx_mean, 2),
            source_files=sources,
        )

    @classmethod
    def analyze_dataset_dir(cls, dataset_dir: Path) -> DatasetStatsReport:
        dataset_dir = Path(dataset_dir)
        train_stats = cls.analyze_split(dataset_dir / "train.jsonl")
        val_stats = cls.analyze_split(dataset_dir / "validation.jsonl")
        test_stats = cls.analyze_split(dataset_dir / "test.jsonl")

        train_sources = set(train_stats.source_files)
        val_sources = set(val_stats.source_files)
        test_sources = set(test_stats.source_files)

        leakage_errors: List[str] = []
        train_val_overlap = train_sources.intersection(val_sources)
        if train_val_overlap:
            leakage_errors.append(f"Leakage between train and validation: {train_val_overlap}")

        train_test_overlap = train_sources.intersection(test_sources)
        if train_test_overlap:
            leakage_errors.append(f"Leakage between train and test: {train_test_overlap}")

        val_test_overlap = val_sources.intersection(test_sources)
        if val_test_overlap:
            leakage_errors.append(f"Leakage between validation and test: {val_test_overlap}")

        total_samples = train_stats.num_samples + val_stats.num_samples + test_stats.num_samples
        is_leakage_free = len(leakage_errors) == 0

        return DatasetStatsReport(
            train_stats=train_stats,
            validation_stats=val_stats,
            test_stats=test_stats,
            total_samples=total_samples,
            is_leakage_free=is_leakage_free,
            leakage_errors=leakage_errors,
        )

    @classmethod
    def format_report_text(cls, report: DatasetStatsReport) -> str:
        lines = [
            "=" * 60,
            " Dataset Statistics & Verification Report",
            "=" * 60,
            f"Total Samples:      {report.total_samples}",
            f"  - Train split:    {report.train_stats.num_samples}",
            f"  - Val split:      {report.validation_stats.num_samples}",
            f"  - Test split:     {report.test_stats.num_samples}",
            f"Leakage Free:       {report.is_leakage_free}",
        ]
        if report.leakage_errors:
            for err in report.leakage_errors:
                lines.append(f"  [ERROR] {err}")

        lines.extend([
            "-" * 60,
            "Corruption Type Distribution (Train):",
        ])
        for k, v in sorted(report.train_stats.corruption_distribution.items()):
            lines.append(f"  {k:25s}: {v}")

        lines.extend([
            "-" * 60,
            "Operation Distribution (Train):",
        ])
        for k, v in sorted(report.train_stats.operation_distribution.items()):
            lines.append(f"  {k:25s}: {v}")

        lines.extend([
            "-" * 60,
            "Context Length (Train):",
            f"  Min: {report.train_stats.context_length_min}, Max: {report.train_stats.context_length_max}, Mean: {report.train_stats.context_length_mean}",
            "=" * 60,
        ])
        return "\n".join(lines)
