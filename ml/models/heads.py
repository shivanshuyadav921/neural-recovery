from typing import Dict, Optional, Tuple

import torch
import torch.nn as nn
import torch.nn.functional as F


class OperationHead(nn.Module):
    """Predicts repair operation: INSERT_TOKEN, DELETE_TOKEN, REPLACE_TOKEN, NO_REPAIR."""

    def __init__(self, hidden_dim: int, num_operations: int, dropout: float = 0.1):
        super().__init__()
        self.classifier = nn.Sequential(
            nn.Dropout(dropout),
            nn.Linear(hidden_dim, hidden_dim // 2 if hidden_dim >= 32 else hidden_dim),
            nn.GELU(),
            nn.Linear(hidden_dim // 2 if hidden_dim >= 32 else hidden_dim, num_operations),
        )

    def forward(self, context_vector: torch.Tensor) -> torch.Tensor:
        """Args: context_vector: [batch_size, hidden_dim]

        Returns: logits: [batch_size, num_operations]
        """
        return self.classifier(context_vector)


class TokenHead(nn.Module):
    """Predicts token to insert or replace from vocabulary."""

    def __init__(self, hidden_dim: int, num_tokens: int, dropout: float = 0.1):
        super().__init__()
        self.classifier = nn.Sequential(
            nn.Dropout(dropout),
            nn.Linear(hidden_dim, hidden_dim // 2 if hidden_dim >= 32 else hidden_dim),
            nn.GELU(),
            nn.Linear(hidden_dim // 2 if hidden_dim >= 32 else hidden_dim, num_tokens),
        )

    def forward(self, context_vector: torch.Tensor) -> torch.Tensor:
        """Args: context_vector: [batch_size, hidden_dim]

        Returns: logits: [batch_size, num_tokens]
        """
        return self.classifier(context_vector)


class PositionHead(nn.Module):
    """Predicts relative repair position within the sequence length."""

    def __init__(self, hidden_dim: int, max_positions: int, dropout: float = 0.1):
        super().__init__()
        self.classifier = nn.Sequential(
            nn.Dropout(dropout),
            nn.Linear(hidden_dim, hidden_dim // 2 if hidden_dim >= 32 else hidden_dim),
            nn.GELU(),
            nn.Linear(hidden_dim // 2 if hidden_dim >= 32 else hidden_dim, max_positions),
        )

    def forward(self, context_vector: torch.Tensor) -> torch.Tensor:
        """Args: context_vector: [batch_size, hidden_dim]

        Returns: logits: [batch_size, max_positions]
        """
        return self.classifier(context_vector)


class MultiTaskRepairLoss(nn.Module):
    """Configurable weighted multi-task loss for structured syntax repair."""

    def __init__(
        self,
        weights: Optional[Dict[str, float]] = None,
        ignore_token_index: int = -100,
        ignore_position_index: int = -100,
    ):
        super().__init__()
        default_weights = {"operation": 1.0, "token": 1.0, "position": 0.5}
        self.weights = weights if weights is not None else default_weights
        self.ignore_token_index = ignore_token_index
        self.ignore_position_index = ignore_position_index

        self.op_loss_fn = nn.CrossEntropyLoss()
        self.token_loss_fn = nn.CrossEntropyLoss(ignore_index=self.ignore_token_index)
        self.pos_loss_fn = nn.CrossEntropyLoss(ignore_index=self.ignore_position_index)

    def forward(
        self,
        operation_logits: torch.Tensor,
        token_logits: torch.Tensor,
        position_logits: torch.Tensor,
        target_operations: torch.Tensor,
        target_tokens: Optional[torch.Tensor] = None,
        target_positions: Optional[torch.Tensor] = None,
    ) -> Tuple[torch.Tensor, Dict[str, torch.Tensor]]:
        """Compute weighted multi-task loss.

        Args:
            operation_logits: [batch_size, num_operations]
            token_logits: [batch_size, num_token_types]
            position_logits: [batch_size, max_positions]
            target_operations: [batch_size]
            target_tokens: [batch_size] (optional)
            target_positions: [batch_size] (optional)

        Returns:
            (total_loss, loss_dict)
        """
        w_op = self.weights.get("operation", 1.0)
        w_tok = self.weights.get("token", 1.0)
        w_pos = self.weights.get("position", 0.5)

        op_loss = self.op_loss_fn(operation_logits, target_operations)
        total_loss = w_op * op_loss
        loss_dict = {"operation_loss": op_loss.detach()}

        if target_tokens is not None and w_tok > 0:
            tok_loss = self.token_loss_fn(token_logits, target_tokens)
            if not torch.isnan(tok_loss):
                total_loss = total_loss + w_tok * tok_loss
                loss_dict["token_loss"] = tok_loss.detach()

        if target_positions is not None and w_pos > 0:
            pos_loss = self.pos_loss_fn(position_logits, target_positions)
            if not torch.isnan(pos_loss):
                total_loss = total_loss + w_pos * pos_loss
                loss_dict["position_loss"] = pos_loss.detach()

        loss_dict["total_loss"] = total_loss.detach()
        return total_loss, loss_dict
