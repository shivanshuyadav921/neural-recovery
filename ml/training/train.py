import argparse
import json
import sys
from pathlib import Path
from typing import Optional

import torch

from ml.models.config import ModelConfig
from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary
from ml.training.checkpoint import CheckpointManager
from ml.training.dataset_stats import DatasetAnalyzer
from ml.training.evaluate import run_evaluation
from ml.training.trainer import Trainer


def train_pipeline(
    dataset_dir: Path,
    output_dir: Path,
    config_path: Optional[Path] = None,
    epochs: int = 30,
    batch_size: int = 16,
    learning_rate: float = 1e-3,
    weight_decay: float = 1e-4,
    seed: int = 42,
    patience: int = 8,
    grad_clip: float = 1.0,
    resume_checkpoint: Optional[Path] = None,
    device_name: Optional[str] = None,
) -> dict:
    device = (
        torch.device(device_name)
        if device_name
        else torch.device("cuda" if torch.cuda.is_available() else "cpu")
    )

    print("=" * 60)
    print(" NeuroParse Model Training Pipeline (Phase 10)")
    print("=" * 60)
    print(f"Dataset Directory:     {dataset_dir}")
    print(f"Output Directory:      {output_dir}")
    print(f"Execution Device:      {device}")
    print(f"Deterministic Seed:    {seed}")
    print("-" * 60)

    # 1. Dataset verification & statistics
    stats_report = DatasetAnalyzer.analyze_dataset_dir(dataset_dir)
    print(DatasetAnalyzer.format_report_text(stats_report))

    if not stats_report.is_leakage_free:
        print("[ERROR] Dataset contains train/val/test leakage! Aborting.")
        sys.exit(1)

    # 2. Model & Config setup
    if config_path and Path(config_path).exists():
        config = ModelConfig.load(config_path)
    else:
        config = ModelConfig()

    vocab = Vocabulary()
    train_file = dataset_dir / "train.jsonl"
    val_file = dataset_dir / "validation.jsonl"
    test_file = dataset_dir / "test.jsonl"

    train_dataset = NeuroLangDataset(train_file, config=config, vocabulary=vocab)
    val_dataset = (
        NeuroLangDataset(val_file, config=config, vocabulary=vocab)
        if val_file.exists()
        else None
    )

    model = RepairModel(config, vocabulary=vocab)
    size_info = model.get_model_size()
    print(f"\nModel Parameters: {size_info['total_parameters']:,} (FP32: {size_info['fp32_memory_mb']} MB)")

    # 3. Initialize Trainer
    trainer = Trainer(
        model=model,
        train_dataset=train_dataset,
        val_dataset=val_dataset,
        output_dir=output_dir,
        learning_rate=learning_rate,
        weight_decay=weight_decay,
        epochs=epochs,
        batch_size=batch_size,
        gradient_clip=grad_clip,
        early_stopping_patience=patience,
        device=device,
        seed=seed,
    )

    # Resume from checkpoint if requested
    if resume_checkpoint and Path(resume_checkpoint).exists():
        print(f"Resuming training from checkpoint '{resume_checkpoint}'...")
        CheckpointManager.load_checkpoint(
            resume_checkpoint,
            device=device,
            model=model,
            optimizer=trainer.optimizer,
        )

    # 4. Train
    train_summary = trainer.train()

    # 5. Final Evaluation on Test Set using Best Checkpoint
    best_chk_path = output_dir / "checkpoints" / "best_checkpoint.pt"
    if best_chk_path.exists() and test_file.exists():
        print("\n" + "=" * 60)
        print(" FINAL TEST EVALUATION (Using Best Checkpoint)")
        print("=" * 60)
        test_results = run_evaluation(
            checkpoint_path=best_chk_path,
            dataset_dir=dataset_dir,
            split="test",
            batch_size=batch_size,
            show_errors=5,
            output_json=output_dir / "test_evaluation.json",
            device_name=str(device),
        )
        train_summary["test_results"] = test_results

    return train_summary


def main():
    parser = argparse.ArgumentParser(description="Train NeuroParse Neural Repair Model")
    parser.add_argument(
        "--dataset",
        type=str,
        default="ml/dataset/generated",
        help="Path to directory with train.jsonl, validation.jsonl, test.jsonl",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="artifacts/training",
        help="Directory to save checkpoints, logs, and evaluation reports",
    )
    parser.add_argument(
        "--config",
        type=str,
        default="ml/models/config.json",
        help="Path to model config JSON",
    )
    parser.add_argument(
        "--epochs",
        type=int,
        default=30,
        help="Maximum training epochs",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=16,
        help="Training batch size",
    )
    parser.add_argument(
        "--lr",
        type=float,
        default=1e-3,
        help="Learning rate",
    )
    parser.add_argument(
        "--weight-decay",
        type=float,
        default=1e-4,
        help="Weight decay for AdamW",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducibility",
    )
    parser.add_argument(
        "--patience",
        type=int,
        default=8,
        help="Early stopping patience in epochs",
    )
    parser.add_argument(
        "--grad-clip",
        type=float,
        default=1.0,
        help="Maximum gradient norm",
    )
    parser.add_argument(
        "--resume",
        type=str,
        default=None,
        help="Path to checkpoint to resume training from",
    )
    parser.add_argument(
        "--device",
        type=str,
        default=None,
        help="Device to train on (cpu/cuda)",
    )

    args = parser.parse_args()
    train_pipeline(
        dataset_dir=Path(args.dataset),
        output_dir=Path(args.output_dir),
        config_path=Path(args.config) if args.config else None,
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.lr,
        weight_decay=args.weight_decay,
        seed=args.seed,
        patience=args.patience,
        grad_clip=args.grad_clip,
        resume_checkpoint=Path(args.resume) if args.resume else None,
        device_name=args.device,
    )


if __name__ == "__main__":
    main()
