import argparse
import sys
from pathlib import Path

from ml.dataset.validator import DatasetValidator


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate generated NeuroParse synthetic dataset."
    )
    parser.add_argument(
        "--dataset",
        type=str,
        default="ml/dataset/generated",
        help="Path to dataset directory containing jsonl files.",
    )
    parser.add_argument(
        "--no-roundtrip",
        action="store_true",
        help="Skip executing compiler validation for round-trip repair.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    dataset_dir = Path(args.dataset)

    if not dataset_dir.exists():
        print(f"Error: Dataset directory '{dataset_dir}' does not exist.", file=sys.stderr)
        sys.exit(1)

    print(f"Validating dataset at '{dataset_dir}'...")
    validator = DatasetValidator(check_compiler_roundtrip=not args.no_roundtrip)
    result = validator.validate_directory(dataset_dir)

    print(f"Total samples: {result['total_samples']}")
    print(f"Samples per split: {result['samples_per_split']}")

    if result["warnings"]:
        print("\nWarnings:")
        for w in result["warnings"]:
            print(f"  [WARN] {w}")

    if not result["valid"]:
        print("\nValidation FAILED with errors:", file=sys.stderr)
        for e in result["errors"]:
            print(f"  [ERROR] {e}", file=sys.stderr)
        sys.exit(1)

    print("\n[SUCCESS] Dataset validation passed with 0 errors!")


if __name__ == "__main__":
    main()
