import argparse
import json
import sys
from pathlib import Path
from typing import List

from ml.dataset.generator import DatasetGenerator
from ml.dataset.schema import DatasetMetadata, GenerationReport
from ml.dataset.splitter import DatasetSplitter


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate synthetic corrupted training dataset for NeuroParse."
    )
    parser.add_argument(
        "--input",
        type=str,
        default="examples/valid",
        help="Input directory containing valid NeuroLang (.nl) source files.",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="ml/dataset/generated",
        help="Output directory to save dataset artifacts.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducible generation and splitting.",
    )
    parser.add_argument(
        "--samples",
        type=int,
        default=None,
        help="Optional maximum number of samples to generate.",
    )
    parser.add_argument(
        "--train-ratio",
        type=float,
        default=0.8,
        help="Proportion of source programs allocated to training set.",
    )
    parser.add_argument(
        "--validation-ratio",
        type=float,
        default=0.1,
        help="Proportion of source programs allocated to validation set.",
    )
    parser.add_argument(
        "--test-ratio",
        type=float,
        default=0.1,
        help="Proportion of source programs allocated to test set.",
    )
    parser.add_argument(
        "--context-before",
        type=int,
        default=50,
        help="Number of tokens before error in ErrorContext.",
    )
    parser.add_argument(
        "--context-after",
        type=int,
        default=10,
        help="Number of tokens after error in ErrorContext.",
    )
    parser.add_argument(
        "--corruptions",
        type=str,
        default=None,
        help="Comma-separated list of corruption strategy names to apply.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    input_dir = Path(args.input)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not input_dir.exists():
        print(f"Error: Input directory '{input_dir}' does not exist.", file=sys.stderr)
        sys.exit(1)

    file_paths: List[Path] = sorted(list(input_dir.glob("*.nl")))
    if not file_paths:
        print(f"Error: No .nl files found in '{input_dir}'.", file=sys.stderr)
        sys.exit(1)

    corruption_types = (
        [c.strip() for c in args.corruptions.split(",") if c.strip()]
        if args.corruptions
        else None
    )

    print(f"Generating synthetic dataset from {len(file_paths)} source file(s)...")
    print(f"Random seed: {args.seed}")

    generator = DatasetGenerator(
        seed=args.seed,
        corruption_types=corruption_types,
        context_before=args.context_before,
        context_after=args.context_after,
    )

    samples = generator.generate_from_files(file_paths, target_samples=args.samples)
    print(f"Generated {len(samples)} valid corrupted training sample(s).")

    # Split dataset
    splitter = DatasetSplitter(
        train_ratio=args.train_ratio,
        validation_ratio=args.validation_ratio,
        test_ratio=args.test_ratio,
        seed=args.seed,
    )
    train_samples, val_samples, test_samples = splitter.split(samples)

    # Write splits to JSONL
    for split_name, split_data in [
        ("train", train_samples),
        ("validation", val_samples),
        ("test", test_samples),
    ]:
        out_file = output_dir / f"{split_name}.jsonl"
        with open(out_file, "w", encoding="utf-8") as f:
            for s in split_data:
                f.write(json.dumps(s.to_dict()) + "\n")
        print(f"  Saved {len(split_data)} sample(s) -> {out_file}")

    # Track distribution per corruption type
    counts_by_type: dict = {}
    for s in samples:
        counts_by_type[s.corruption_type] = counts_by_type.get(s.corruption_type, 0) + 1

    # Write metadata.json
    metadata = DatasetMetadata(
        dataset_version="0.1.0",
        language="NeuroLang",
        generator_version="0.1.0",
        random_seed=args.seed,
        context_before=args.context_before,
        context_after=args.context_after,
        corruption_types=list(generator.strategies.keys()),
        split_ratios={
            "train": args.train_ratio,
            "validation": args.validation_ratio,
            "test": args.test_ratio,
        },
    )
    with open(output_dir / "metadata.json", "w", encoding="utf-8") as f:
        json.dump(metadata.to_dict(), f, indent=2)

    # Write report.json
    report = GenerationReport(
        total_samples_generated=len(samples),
        samples_per_corruption=counts_by_type,
        train_count=len(train_samples),
        validation_count=len(val_samples),
        test_count=len(test_samples),
        rejected_count=sum(generator.rejection_stats.values()),
        rejection_reasons=generator.rejection_stats,
    )
    with open(output_dir / "report.json", "w", encoding="utf-8") as f:
        json.dump(report.to_dict(), f, indent=2)

    print("\nGeneration Report:")
    print(f"  Total samples: {len(samples)}")
    print(f"  Samples per corruption: {counts_by_type}")
    print(f"  Rejections: {generator.rejection_stats}")
    print(f"Dataset successfully generated at '{output_dir}'.")


if __name__ == "__main__":
    main()
