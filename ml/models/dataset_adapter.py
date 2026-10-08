import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import torch
from torch.utils.data import DataLoader, Dataset

from ml.dataset.schema import DatasetSample
from ml.models.config import ModelConfig
from ml.models.encoder import ContextEncoder, EncodedContext
from ml.models.vocabulary import Vocabulary


class NeuroLangDataset(Dataset):
    """PyTorch Dataset adapter for Phase 8 generated JSONL datasets."""

    def __init__(
        self,
        samples_or_path: Union[List[DatasetSample], str, Path],
        config: Optional[ModelConfig] = None,
        vocabulary: Optional[Vocabulary] = None,
    ):
        self.config = config if config is not None else ModelConfig()
        self.vocabulary = vocabulary if vocabulary is not None else Vocabulary()
        self.encoder = ContextEncoder(self.config, self.vocabulary)

        if isinstance(samples_or_path, (str, Path)):
            self.samples = self._load_jsonl(Path(samples_or_path))
        else:
            self.samples = list(samples_or_path)

    def _load_jsonl(self, path: Path) -> List[DatasetSample]:
        samples: List[DatasetSample] = []
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                d = json.loads(line)
                samples.append(DatasetSample.from_dict(d))
        return samples

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int) -> EncodedContext:
        sample = self.samples[idx]
        return self.encoder.encode_sample(sample)

    @staticmethod
    def collate_fn(batch: List[EncodedContext]) -> Dict[str, Any]:
        """Collate batch of EncodedContext items into batched tensors."""
        input_ids = torch.stack([item.input_ids for item in batch], dim=0)
        attention_mask = torch.stack([item.attention_mask for item in batch], dim=0)
        segment_ids = torch.stack([item.segment_ids for item in batch], dim=0)
        error_positions = torch.tensor(
            [item.error_position for item in batch], dtype=torch.long
        )

        has_target_op = batch[0].target_operation is not None
        target_operations = (
            torch.tensor([item.target_operation for item in batch], dtype=torch.long)
            if has_target_op
            else None
        )

        has_target_tok = batch[0].target_token is not None
        target_tokens = (
            torch.tensor([item.target_token for item in batch], dtype=torch.long)
            if has_target_tok
            else None
        )

        has_target_pos = batch[0].target_position is not None
        target_positions = (
            torch.tensor([item.target_position for item in batch], dtype=torch.long)
            if has_target_pos
            else None
        )

        return {
            "input_ids": input_ids,
            "attention_mask": attention_mask,
            "segment_ids": segment_ids,
            "error_positions": error_positions,
            "target_operations": target_operations,
            "target_tokens": target_tokens,
            "target_positions": target_positions,
        }

    def create_dataloader(
        self,
        batch_size: int = 16,
        shuffle: bool = False,
        num_workers: int = 0,
    ) -> DataLoader:
        return DataLoader(
            self,
            batch_size=batch_size,
            shuffle=shuffle,
            num_workers=num_workers,
            collate_fn=self.collate_fn,
        )
