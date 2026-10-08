import json
import random
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader

from ml.models.config import ModelConfig
from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary
from ml.training.checkpoint import CheckpointManager
from ml.training.metrics import EvaluationResults, evaluate_predictions


class Trainer:
    """Reproducible trainer and evaluator for NeuroParse neural repair models."""

    def __init__(
        self,
        model: RepairModel,
        train_dataset: NeuroLangDataset,
        val_dataset: Optional[NeuroLangDataset] = None,
        output_dir: Union[str, Path] = "artifacts/training",
        learning_rate: float = 1e-3,
        weight_decay: float = 1e-4,
        epochs: int = 20,
        batch_size: int = 16,
        gradient_clip: float = 1.0,
        early_stopping_patience: int = 5,
        device: Optional[torch.device] = None,
        seed: int = 42,
    ):
        self.seed = seed
        self._set_seed(seed)

        self.model = model
        self.train_dataset = train_dataset
        self.val_dataset = val_dataset
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.learning_rate = learning_rate
        self.weight_decay = weight_decay
        self.epochs = epochs
        self.batch_size = batch_size
        self.gradient_clip = gradient_clip
        self.patience = early_stopping_patience

        self.device = (
            device
            if device is not None
            else torch.device("cuda" if torch.cuda.is_available() else "cpu")
        )
        self.model.to(self.device)

        self.optimizer = optim.AdamW(
            self.model.parameters(),
            lr=self.learning_rate,
            weight_decay=self.weight_decay,
        )
        self.checkpoint_manager = CheckpointManager(self.output_dir / "checkpoints")

        self.history: List[Dict[str, Any]] = []

    def _set_seed(self, seed: int) -> None:
        random.seed(seed)
        np.random.seed(seed)
        torch.manual_seed(seed)
        if torch.cuda.is_available():
            torch.cuda.manual_seed_all(seed)

    def train_epoch(self, dataloader: DataLoader) -> float:
        self.model.train()
        total_loss = 0.0
        num_batches = 0

        for batch in dataloader:
            input_ids = batch["input_ids"].to(self.device)
            attention_mask = batch["attention_mask"].to(self.device)
            segment_ids = batch["segment_ids"].to(self.device)
            error_positions = batch["error_positions"].to(self.device)
            target_ops = batch["target_operations"].to(self.device)
            target_toks = (
                batch["target_tokens"].to(self.device)
                if batch["target_tokens"] is not None
                else None
            )
            target_pos = (
                batch["target_positions"].to(self.device)
                if batch["target_positions"] is not None
                else None
            )

            self.optimizer.zero_grad()
            output = self.model(
                input_ids=input_ids,
                attention_mask=attention_mask,
                segment_ids=segment_ids,
                error_positions=error_positions,
                target_operations=target_ops,
                target_tokens=target_toks,
                target_positions=target_pos,
            )

            loss = output.loss
            assert loss is not None, "Model forward pass produced no loss"

            loss.backward()

            if self.gradient_clip > 0:
                nn.utils.clip_grad_norm_(self.model.parameters(), self.gradient_clip)

            self.optimizer.step()

            total_loss += loss.item()
            num_batches += 1

        return total_loss / num_batches if num_batches > 0 else 0.0

    @torch.no_grad()
    def evaluate(self, dataset: NeuroLangDataset) -> EvaluationResults:
        self.model.eval()
        dataloader = dataset.create_dataloader(
            batch_size=self.batch_size, shuffle=False
        )

        all_target_ops: List[int] = []
        all_pred_ops: List[int] = []
        all_target_toks: List[int] = []
        all_pred_toks: List[int] = []
        all_target_pos: List[int] = []
        all_pred_pos: List[int] = []
        all_confidences: List[float] = []
        all_corruptions: List[str] = []
        all_sample_ids: List[str] = []

        sample_offset = 0
        for batch in dataloader:
            b_size = batch["input_ids"].shape[0]
            batch_samples = dataset.samples[sample_offset : sample_offset + b_size]
            sample_offset += b_size

            input_ids = batch["input_ids"].to(self.device)
            attention_mask = batch["attention_mask"].to(self.device)
            segment_ids = batch["segment_ids"].to(self.device)
            error_positions = batch["error_positions"].to(self.device)

            output = self.model(
                input_ids=input_ids,
                attention_mask=attention_mask,
                segment_ids=segment_ids,
                error_positions=error_positions,
            )

            pred_ops = torch.argmax(output.operation_logits, dim=-1).cpu().tolist()
            pred_toks = torch.argmax(output.token_logits, dim=-1).cpu().tolist()
            pred_pos = torch.argmax(output.position_logits, dim=-1).cpu().tolist()
            confs = output.confidence.cpu().tolist()

            target_ops = batch["target_operations"].tolist()
            target_toks = (
                batch["target_tokens"].tolist()
                if batch["target_tokens"] is not None
                else [0] * b_size
            )
            target_pos = (
                batch["target_positions"].tolist()
                if batch["target_positions"] is not None
                else [0] * b_size
            )

            all_pred_ops.extend(pred_ops)
            all_target_ops.extend(target_ops)
            all_pred_toks.extend(pred_toks)
            all_target_toks.extend(target_toks)
            all_pred_pos.extend(pred_pos)
            all_target_pos.extend(target_pos)
            all_confidences.extend(confs)

            for s in batch_samples:
                all_corruptions.append(s.corruption_type)
                all_sample_ids.append(s.sample_id)

        return evaluate_predictions(
            target_ops=all_target_ops,
            pred_ops=all_pred_ops,
            target_tokens=all_target_toks,
            pred_tokens=all_pred_toks,
            target_positions=all_target_pos,
            pred_positions=all_pred_pos,
            confidences=all_confidences,
            corruptions=all_corruptions,
            sample_ids=all_sample_ids,
            vocabulary=self.model.vocabulary,
        )

    def train(self) -> Dict[str, Any]:
        train_loader = self.train_dataset.create_dataloader(
            batch_size=self.batch_size, shuffle=True
        )

        best_val_exact = -1.0
        epochs_without_improvement = 0
        best_epoch = 0

        print("=" * 60)
        print(f" Starting NeuroParse Training on {self.device.type.upper()}")
        print(f" Train Samples: {len(self.train_dataset)} | Val Samples: {len(self.val_dataset) if self.val_dataset else 0}")
        print(f" Epochs: {self.epochs} | Batch Size: {self.batch_size} | LR: {self.learning_rate} | Seed: {self.seed}")
        print("=" * 60)

        for epoch in range(1, self.epochs + 1):
            train_loss = self.train_epoch(train_loader)

            val_metrics = None
            if self.val_dataset is not None:
                val_metrics = self.evaluate(self.val_dataset)
                val_exact = val_metrics.exact_repair_accuracy
                val_op_acc = val_metrics.operation_report.accuracy
                val_tok_acc = val_metrics.token_accuracy

                print(
                    f"Epoch {epoch:02d}/{self.epochs:02d} | "
                    f"Train Loss: {train_loss:.4f} | "
                    f"Val Exact Acc: {val_exact*100:.2f}% | "
                    f"Val Op Acc: {val_op_acc*100:.2f}% | "
                    f"Val Tok Acc: {val_tok_acc*100:.2f}%"
                )

                is_best = val_exact > best_val_exact
                if is_best:
                    best_val_exact = val_exact
                    best_epoch = epoch
                    epochs_without_improvement = 0
                else:
                    epochs_without_improvement += 1

                epoch_record = {
                    "epoch": epoch,
                    "train_loss": round(train_loss, 4),
                    "val_exact_repair_acc": val_exact,
                    "val_op_acc": val_op_acc,
                    "val_tok_acc": val_tok_acc,
                    "is_best": is_best,
                }
                self.history.append(epoch_record)

                # Save checkpoint
                self.checkpoint_manager.save_checkpoint(
                    model=self.model,
                    optimizer=self.optimizer,
                    epoch=epoch,
                    best_metric=best_val_exact,
                    metrics_history=self.history,
                    is_best=is_best,
                    seed=self.seed,
                )

                if self.patience > 0 and epochs_without_improvement >= self.patience:
                    print(f"\n[EARLY STOPPING] No improvement for {self.patience} consecutive epochs. Stopping at epoch {epoch}.")
                    break
            else:
                print(f"Epoch {epoch:02d}/{self.epochs:02d} | Train Loss: {train_loss:.4f}")
                self.history.append({"epoch": epoch, "train_loss": round(train_loss, 4)})

        # Save training history
        history_path = self.output_dir / "training_history.json"
        with open(history_path, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "seed": self.seed,
                    "epochs_trained": epoch,
                    "best_epoch": best_epoch,
                    "best_val_exact_accuracy": best_val_exact,
                    "history": self.history,
                },
                f,
                indent=2,
            )

        print("-" * 60)
        print(f"Training completed. Best Val Exact Acc: {best_val_exact*100:.2f}% (Epoch {best_epoch})")
        print(f"Artifacts and checkpoints saved to '{self.output_dir}'.")
        print("=" * 60)

        return {
            "best_epoch": best_epoch,
            "best_val_exact_accuracy": best_val_exact,
            "history": self.history,
        }
