from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

import torch

from ml.dataset.schema import ContextRecord, DatasetSample, RepairLabel
from ml.models.config import ModelConfig
from ml.models.vocabulary import (
    BOS_TOKEN,
    EOS_TOKEN,
    ERROR_TOKEN,
    PAD_TOKEN,
    Vocabulary,
)


@dataclass
class EncodedContext:
    input_ids: torch.Tensor
    attention_mask: torch.Tensor
    segment_ids: torch.Tensor
    error_position: int
    expected_token_ids: List[int]
    target_operation: Optional[int] = None
    target_token: Optional[int] = None
    target_position: Optional[int] = None


class ContextEncoder:
    """Encodes localized ErrorContext and repair labels into model-ready tensors."""

    def __init__(
        self,
        config: ModelConfig,
        vocabulary: Optional[Vocabulary] = None,
    ):
        self.config = config
        self.vocab = vocabulary if vocabulary is not None else Vocabulary()
        self.max_length = config.max_sequence_length

    def encode_context(
        self,
        context: ContextRecord,
        repair: Optional[RepairLabel] = None,
    ) -> EncodedContext:
        """Encode a ContextRecord (or Phase 7 ErrorContext) into tensors."""
        tokens_before = context.tokens_before
        current_token = context.current_token
        tokens_after = context.tokens_after

        # Encode token lists
        before_ids = [self.vocab.encode_token(t) for t in tokens_before]
        curr_id = self.vocab.encode_token(current_token) if current_token else self.vocab.unk_id
        after_ids = [self.vocab.encode_token(t) for t in tokens_after]

        # Calculate max available tokens for before and after (reserving 3 slots for BOS, ERROR, EOS, and 1 for current)
        reserved_slots = 4  # BOS, ERROR, curr, EOS
        max_context_tokens = self.max_length - reserved_slots

        # Left-truncate before_tokens if too long
        if len(before_ids) + len(after_ids) > max_context_tokens:
            # Allocate proportional budget
            budget_after = min(len(after_ids), 10)
            budget_before = max_context_tokens - budget_after
            before_ids = before_ids[-budget_before:] if budget_before > 0 else []
            after_ids = after_ids[:budget_after]

        # Assemble sequence: [BOS] + before + [ERROR] + curr + after + [EOS]
        seq_tokens: List[int] = []
        seq_segments: List[int] = []

        # BOS
        seq_tokens.append(self.vocab.bos_id)
        seq_segments.append(1)  # BEFORE

        # Before tokens
        for tid in before_ids:
            seq_tokens.append(tid)
            seq_segments.append(1)  # BEFORE

        # ERROR marker & current token
        error_pos_in_seq = len(seq_tokens)
        seq_tokens.append(self.vocab.error_id)
        seq_segments.append(2)  # ERROR

        seq_tokens.append(curr_id)
        seq_segments.append(2)  # ERROR

        # After tokens
        for tid in after_ids:
            seq_tokens.append(tid)
            seq_segments.append(3)  # AFTER

        # EOS
        seq_tokens.append(self.vocab.eos_id)
        seq_segments.append(3)  # AFTER

        actual_len = len(seq_tokens)
        pad_len = self.max_length - actual_len

        # Pad sequence
        if pad_len > 0:
            seq_tokens.extend([self.vocab.pad_id] * pad_len)
            seq_segments.extend([0] * pad_len)
            attention_mask = [1] * actual_len + [0] * pad_len
        else:
            seq_tokens = seq_tokens[: self.max_length]
            seq_segments = seq_segments[: self.max_length]
            attention_mask = [1] * self.max_length

        # Encode expected tokens
        expected_token_ids = [
            self.vocab.encode_token({"lexeme": exp, "type": exp})
            for exp in context.expected_tokens
        ]

        # Target encoding if repair label is provided
        target_op = None
        target_tok = None
        target_pos = None

        if repair is not None:
            target_op = self.vocab.encode_operation(repair.operation)
            if repair.token:
                target_tok = self.vocab.encode_token(
                    {"lexeme": repair.token, "type": repair.token}
                )
            else:
                target_tok = self.vocab.pad_id

            # Map relative position to error_pos_in_seq if applicable
            target_pos = min(error_pos_in_seq, self.max_length - 1)

        return EncodedContext(
            input_ids=torch.tensor(seq_tokens, dtype=torch.long),
            attention_mask=torch.tensor(attention_mask, dtype=torch.long),
            segment_ids=torch.tensor(seq_segments, dtype=torch.long),
            error_position=error_pos_in_seq,
            expected_token_ids=expected_token_ids,
            target_operation=target_op,
            target_token=target_tok,
            target_position=target_pos,
        )

    def encode_sample(self, sample: DatasetSample) -> EncodedContext:
        """Encode a complete Phase 8 DatasetSample."""
        return self.encode_context(sample.context, sample.repair)
