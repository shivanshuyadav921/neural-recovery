import argparse
import json
from pathlib import Path
from typing import Optional

import torch

from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.repair_model import RepairModel
from ml.training.checkpoint import CheckpointManager
from ml.training.metrics import format_evaluation_summary
from ml.training.trainer import Trainer


def run_evaluation(
    checkpoint_path: Path,
    dataset_dir: Path,
    split: str = "test",
    batch_size: int = 16,
    show_errors: int = 10,
    output_json: Optional[Path] = None,
    device_name: Optional[str] = None,
) -> dict:
    device = (
        torch.device(device_name)
        if device_name
        else torch.device("cuda" if torch.cuda.is_available() else "cpu")
    )

    print(f"Loading checkpoint from '{checkpoint_path}'...")
    model, chk_info = CheckpointManager.load_checkpoint(checkpoint_path, device=device)

    splits_to_eval = [split] if split != "all" else ["train", "validation", "test"]
    all_results = {}

    for s in splits_to_eval:
        split_file = dataset_dir / f"{s}.jsonl"
        if not split_file.exists():
            print(f"[WARNING] Split file '{split_file}' does not exist. Skipping.")
            continue

        dataset = NeuroLangDataset(split_file, config=model.config, vocabulary=model.vocabulary)
        trainer = Trainer(
            model=model,
            train_dataset=dataset,
            batch_size=batch_size,
            device=device,
        )

        print(f"\nEvaluating on '{s}' split ({len(dataset)} samples)...")
        results = trainer.evaluate(dataset)
        print(format_evaluation_summary(results))

        if show_errors > 0 and results.failure_examples:
            print(f"\n--- Representative Failure Cases (showing up to {show_errors}) ---")
            for idx, fail in enumerate(results.failure_examples[:show_errors]):
                print(f"\n[Failure #{idx + 1}] Sample: {fail['sample_id']} | Type: {fail['corruption_type']}")
                print(f"  Expected: Op={fail['expected_operation']}, Tok={fail['expected_token']}, Pos={fail['expected_position']}")
                print(f"  Predicted: Op={fail['predicted_operation']}, Tok={fail['predicted_token']}, Pos={fail['predicted_position']} (Conf: {fail['confidence']:.4f})")
            print("-" * 60)

        all_results[s] = results.to_dict()

    if output_json is not None:
        output_json = Path(output_json)
        output_json.parent.mkdir(parents=True, exist_ok=True)
        with open(output_json, "w", encoding="utf-8") as f:
            json.dump(all_results, f, indent=2)
        print(f"\nEvaluation results saved to '{output_json}'.")

    return all_results


def main():
    parser = argparse.ArgumentParser(description="Evaluate NeuroParse Neural Repair Model")
    parser.add_argument(
        "--checkpoint",
        type=str,
        required=True,
        help="Path to trained .pt checkpoint file",
    )
    parser.add_argument(
        "--dataset",
        type=str,
        default="ml/dataset/generated",
        help="Directory containing JSONL dataset splits",
    )
    parser.add_argument(
        "--split",
        type=str,
        default="test",
        choices=["train", "validation", "test", "all"],
        help="Dataset split to evaluate",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=16,
        help="Evaluation batch size",
    )
    parser.add_argument(
        "--show-errors",
        type=int,
        default=10,
        help="Number of representative error cases to display",
    )
    parser.add_argument(
        "--output-json",
        type=str,
        default=None,
        help="Path to save evaluation JSON results",
    )
    parser.add_argument(
        "--device",
        type=str,
        default=None,
        help="Device to run on (cpu/cuda)",
    )

    args = parser.parse_args()
    run_evaluation(
        checkpoint_path=Path(args.checkpoint),
        dataset_dir=Path(args.dataset),
        split=args.split,
        batch_size=args.batch_size,
        show_errors=args.show_errors,
        output_json=Path(args.output_json) if args.output_json else None,
        device_name=args.device,
    )


if __name__ == "__main__":
    main()
