from pathlib import Path
from typing import Any, Dict, Optional, Tuple, Union

import torch
import torch.nn as nn
import torch.optim as optim

from ml.models.config import ModelConfig
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary


class CheckpointManager:
    """Manages saving, loading, and resuming model training checkpoints."""

    def __init__(self, checkpoint_dir: Union[str, Path]):
        self.checkpoint_dir = Path(checkpoint_dir)
        self.checkpoint_dir.mkdir(parents=True, exist_ok=True)

    def save_checkpoint(
        self,
        model: RepairModel,
        optimizer: Optional[optim.Optimizer],
        epoch: int,
        best_metric: float,
        metrics_history: Optional[list] = None,
        scheduler: Optional[Any] = None,
        is_best: bool = False,
        filename: Optional[str] = None,
        seed: int = 42,
    ) -> Path:
        checkpoint_data = {
            "epoch": epoch,
            "best_metric": best_metric,
            "seed": seed,
            "config": model.config.to_dict(),
            "model_state_dict": model.state_dict(),
            "optimizer_state_dict": optimizer.state_dict() if optimizer else None,
            "scheduler_state_dict": scheduler.state_dict() if scheduler else None,
            "metrics_history": metrics_history or [],
        }

        if filename is None:
            filename = f"checkpoint_epoch_{epoch:03d}.pt"

        target_path = self.checkpoint_dir / filename
        torch.save(checkpoint_data, target_path)

        # Always update latest_checkpoint.pt
        latest_path = self.checkpoint_dir / "latest_checkpoint.pt"
        torch.save(checkpoint_data, latest_path)

        if is_best:
            best_path = self.checkpoint_dir / "best_checkpoint.pt"
            torch.save(checkpoint_data, best_path)

        return target_path

    @staticmethod
    def load_checkpoint(
        checkpoint_path: Union[str, Path],
        device: torch.device = torch.device("cpu"),
        model: Optional[RepairModel] = None,
        optimizer: Optional[optim.Optimizer] = None,
        scheduler: Optional[Any] = None,
    ) -> Tuple[RepairModel, Dict[str, Any]]:
        path = Path(checkpoint_path)
        if not path.exists():
            raise FileNotFoundError(f"Checkpoint file not found: '{path}'")

        checkpoint = torch.load(path, map_location=device)
        config = ModelConfig.from_dict(checkpoint["config"])

        if model is None:
            vocab = Vocabulary()
            model = RepairModel(config, vocabulary=vocab)

        model.load_state_dict(checkpoint["model_state_dict"])
        model.to(device)

        if optimizer is not None and checkpoint.get("optimizer_state_dict"):
            optimizer.load_state_dict(checkpoint["optimizer_state_dict"])

        if scheduler is not None and checkpoint.get("scheduler_state_dict"):
            scheduler.load_state_dict(checkpoint["scheduler_state_dict"])

        return model, checkpoint
