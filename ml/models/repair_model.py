from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple

import torch
import torch.nn as nn
import torch.nn.functional as F

from ml.models.config import ModelConfig
from ml.models.heads import (
    MultiTaskRepairLoss,
    OperationHead,
    PositionHead,
    TokenHead,
)
from ml.models.vocabulary import Vocabulary


@dataclass
class RepairModelOutput:
    operation_logits: torch.Tensor
    token_logits: torch.Tensor
    position_logits: torch.Tensor
    operation_probs: torch.Tensor
    token_probs: torch.Tensor
    position_probs: torch.Tensor
    confidence: torch.Tensor
    loss: Optional[torch.Tensor] = None
    loss_dict: Optional[Dict[str, torch.Tensor]] = None


class RepairModel(nn.Module):
    """Small local Transformer model for structured compiler syntax repair."""

    def __init__(self, config: ModelConfig, vocabulary: Optional[Vocabulary] = None):
        super().__init__()
        self.config = config
        self.config.validate()
        self.vocabulary = vocabulary if vocabulary is not None else Vocabulary()

        # Embeddings
        self.token_embeddings = nn.Embedding(
            self.config.vocab_size,
            self.config.embedding_dim,
            padding_idx=self.vocabulary.pad_id,
        )
        self.position_embeddings = nn.Embedding(
            self.config.max_sequence_length,
            self.config.embedding_dim,
        )
        # Segment roles: 0=PAD, 1=BEFORE, 2=CURRENT/ERROR, 3=AFTER
        self.segment_embeddings = nn.Embedding(
            4,
            self.config.embedding_dim,
        )

        self.embed_layer_norm = nn.LayerNorm(self.config.embedding_dim)
        self.embed_dropout = nn.Dropout(self.config.dropout)

        # Transformer Encoder
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=self.config.embedding_dim,
            nhead=self.config.num_heads,
            dim_feedforward=self.config.feed_forward_dim,
            dropout=self.config.dropout,
            activation="gelu",
            batch_first=True,
            norm_first=True,
        )
        self.transformer = nn.TransformerEncoder(
            encoder_layer,
            num_layers=self.config.num_layers,
            enable_nested_tensor=False,
        )

        # Prediction Heads
        self.operation_head = OperationHead(
            hidden_dim=self.config.embedding_dim,
            num_operations=self.config.num_operations,
            dropout=self.config.dropout,
        )
        self.token_head = TokenHead(
            hidden_dim=self.config.embedding_dim,
            num_tokens=self.config.num_token_types,
            dropout=self.config.dropout,
        )
        self.position_head = PositionHead(
            hidden_dim=self.config.embedding_dim,
            max_positions=self.config.max_sequence_length,
            dropout=self.config.dropout,
        )

        # Loss Function
        self.loss_fn = MultiTaskRepairLoss(
            weights=self.config.loss_weights,
            ignore_token_index=self.vocabulary.pad_id,
        )

    def forward(
        self,
        input_ids: torch.Tensor,
        attention_mask: Optional[torch.Tensor] = None,
        segment_ids: Optional[torch.Tensor] = None,
        error_positions: Optional[torch.Tensor] = None,
        target_operations: Optional[torch.Tensor] = None,
        target_tokens: Optional[torch.Tensor] = None,
        target_positions: Optional[torch.Tensor] = None,
    ) -> RepairModelOutput:
        """Forward pass for structured repair prediction.

        Args:
            input_ids: [batch_size, seq_len] Token indices
            attention_mask: [batch_size, seq_len] 1 for valid tokens, 0 for PAD
            segment_ids: [batch_size, seq_len] Role segment IDs (optional)
            error_positions: [batch_size] Index of the error token in the sequence (optional)
            target_operations: [batch_size] Ground truth operation IDs (optional)
            target_tokens: [batch_size] Ground truth token IDs (optional)
            target_positions: [batch_size] Ground truth position offsets (optional)

        Returns:
            RepairModelOutput with logits, probabilities, confidence, and optional loss
        """
        batch_size, seq_len = input_ids.shape
        device = input_ids.device

        # Default attention mask: 1 where input_ids != pad_id
        if attention_mask is None:
            attention_mask = (input_ids != self.vocabulary.pad_id).long()

        # Token embeddings
        tok_embeds = self.token_embeddings(input_ids)

        # Positional embeddings
        positions = torch.arange(seq_len, device=device).unsqueeze(0).expand(batch_size, -1)
        pos_embeds = self.position_embeddings(positions)

        hidden = tok_embeds + pos_embeds

        # Segment embeddings if provided
        if segment_ids is not None:
            seg_embeds = self.segment_embeddings(segment_ids)
            hidden = hidden + seg_embeds

        hidden = self.embed_layer_norm(hidden)
        hidden = self.embed_dropout(hidden)

        # In PyTorch TransformerEncoder, src_key_padding_mask has True for padded positions
        src_key_padding_mask = (attention_mask == 0)

        # Transformer encoding
        encoded = self.transformer(
            hidden,
            src_key_padding_mask=src_key_padding_mask,
        )

        # Context vector extraction:
        # If error_positions is given, gather encoded vectors at the error position;
        # otherwise, use masked mean pooling across non-padded tokens.
        if error_positions is not None:
            # error_positions: [batch_size] -> clamped to valid range [0, seq_len - 1]
            clamped_positions = error_positions.clamp(0, seq_len - 1).unsqueeze(-1).unsqueeze(-1)
            clamped_positions = clamped_positions.expand(-1, 1, self.config.embedding_dim)
            context_vector = encoded.gather(1, clamped_positions).squeeze(1)
        else:
            # Masked mean pooling
            mask_expanded = attention_mask.unsqueeze(-1).float()
            sum_encoded = torch.sum(encoded * mask_expanded, dim=1)
            lengths = torch.clamp(mask_expanded.sum(dim=1), min=1.0)
            context_vector = sum_encoded / lengths

        # Head outputs
        op_logits = self.operation_head(context_vector)
        tok_logits = self.token_head(context_vector)
        pos_logits = self.position_head(context_vector)

        # Probabilities
        op_probs = F.softmax(op_logits, dim=-1)
        tok_probs = F.softmax(tok_logits, dim=-1)
        pos_probs = F.softmax(pos_logits, dim=-1)

        # Confidence: product of max operation probability and max token probability
        max_op_prob, _ = torch.max(op_probs, dim=-1)
        max_tok_prob, _ = torch.max(tok_probs, dim=-1)
        confidence = max_op_prob * max_tok_prob

        # Loss calculation if targets provided
        loss = None
        loss_dict = None
        if target_operations is not None:
            loss, loss_dict = self.loss_fn(
                operation_logits=op_logits,
                token_logits=tok_logits,
                position_logits=pos_logits,
                target_operations=target_operations,
                target_tokens=target_tokens,
                target_positions=target_positions,
            )

        return RepairModelOutput(
            operation_logits=op_logits,
            token_logits=tok_logits,
            position_logits=pos_logits,
            operation_probs=op_probs,
            token_probs=tok_probs,
            position_probs=pos_probs,
            confidence=confidence,
            loss=loss,
            loss_dict=loss_dict,
        )

    def get_model_size(self) -> Dict[str, Any]:
        """Compute parameter counts and approximate memory footprints."""
        total_params = sum(p.numel() for p in self.parameters())
        trainable_params = sum(p.numel() for p in self.parameters() if p.requires_grad)

        # FP32: 4 bytes per param, FP16: 2 bytes per param
        fp32_memory_mb = (total_params * 4) / (1024 * 1024)
        fp16_memory_mb = (total_params * 2) / (1024 * 1024)

        return {
            "total_parameters": total_params,
            "trainable_parameters": trainable_params,
            "non_trainable_parameters": total_params - trainable_params,
            "fp32_memory_mb": round(fp32_memory_mb, 3),
            "fp16_memory_mb": round(fp16_memory_mb, 3),
        }
