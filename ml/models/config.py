import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional, Union


@dataclass
class ModelConfig:
    vocab_size: int = 42
    embedding_dim: int = 64
    num_layers: int = 2
    num_heads: int = 4
    feed_forward_dim: int = 256
    dropout: float = 0.1
    max_sequence_length: int = 128
    num_operations: int = 4
    num_token_types: int = 42
    learning_rate: float = 1e-3
    loss_weights: Dict[str, float] = field(
        default_factory=lambda: {
            "operation": 1.0,
            "token": 1.0,
            "position": 0.5,
        }
    )

    def __post_init__(self):
        self.validate()

    def validate(self) -> None:
        if self.vocab_size <= 0:
            raise ValueError(f"vocab_size must be positive, got {self.vocab_size}")
        if self.embedding_dim <= 0:
            raise ValueError(f"embedding_dim must be positive, got {self.embedding_dim}")
        if self.num_layers <= 0:
            raise ValueError(f"num_layers must be positive, got {self.num_layers}")
        if self.num_heads <= 0:
            raise ValueError(f"num_heads must be positive, got {self.num_heads}")
        if self.embedding_dim % self.num_heads != 0:
            raise ValueError(
                f"embedding_dim ({self.embedding_dim}) must be divisible by num_heads ({self.num_heads})"
            )
        if self.feed_forward_dim <= 0:
            raise ValueError(
                f"feed_forward_dim must be positive, got {self.feed_forward_dim}"
            )
        if not (0.0 <= self.dropout < 1.0):
            raise ValueError(
                f"dropout must be in [0.0, 1.0), got {self.dropout}"
            )
        if self.max_sequence_length <= 0:
            raise ValueError(
                f"max_sequence_length must be positive, got {self.max_sequence_length}"
            )
        if self.num_operations <= 0:
            raise ValueError(
                f"num_operations must be positive, got {self.num_operations}"
            )
        if self.num_token_types <= 0:
            raise ValueError(
                f"num_token_types must be positive, got {self.num_token_types}"
            )

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "ModelConfig":
        valid_keys = {
            "vocab_size",
            "embedding_dim",
            "num_layers",
            "num_heads",
            "feed_forward_dim",
            "dropout",
            "max_sequence_length",
            "num_operations",
            "num_token_types",
            "learning_rate",
            "loss_weights",
        }
        filtered = {k: v for k, v in d.items() if k in valid_keys}
        return cls(**filtered)

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent)

    @classmethod
    def from_json(cls, json_str: str) -> "ModelConfig":
        return cls.from_dict(json.loads(json_str))

    def save(self, path: Union[str, Path]) -> None:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.to_json())

    @classmethod
    def load(cls, path: Union[str, Path]) -> "ModelConfig":
        with open(path, "r", encoding="utf-8") as f:
            return cls.from_json(f.read())
