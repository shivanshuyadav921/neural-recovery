from __future__ import annotations

import abc
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple

if TYPE_CHECKING:
    from ml.dataset.schema import RepairLabel


def _make_repair_label(
    operation: str,
    token: str,
    position: int,
    replaced_token: Optional[str] = None,
) -> RepairLabel:
    from ml.dataset.schema import RepairLabel

    return RepairLabel(
        operation=operation,
        token=token,
        position=position,
        replaced_token=replaced_token,
    )


class CorruptionStrategy(abc.ABC):
    @property
    @abc.abstractmethod
    def name(self) -> str:
        """Name of the corruption strategy."""
        pass

    @abc.abstractmethod
    def find_candidates(self, tokens: List[dict], source: str) -> List[Any]:
        """Find all applicable token indices or positions for corruption."""
        pass

    @abc.abstractmethod
    def apply(
        self, candidate: Any, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        """Apply corruption and return (corrupted_source, ground_truth_repair)."""
        pass

    @abc.abstractmethod
    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        """Apply the ground truth repair to restore the original program."""
        pass


class MissingSemicolonCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "missing_semicolon"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        # Semicolons that terminate statements
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] == ";"
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        target_tok = tokens[candidate]
        # Reconstruct source without this semicolon token
        corrupted_tokens = [
            tok["lexeme"] for i, tok in enumerate(tokens) if i != candidate and tok["type"] != "EOF"
        ]
        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="INSERT_TOKEN",
            token=";",
            position=candidate,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        tokens.insert(repair.position, repair.token)
        return " ".join(tokens)


class MissingClosingParenCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "missing_closing_paren"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] == ")"
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        corrupted_tokens = [
            tok["lexeme"] for i, tok in enumerate(tokens) if i != candidate and tok["type"] != "EOF"
        ]
        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="INSERT_TOKEN",
            token=")",
            position=candidate,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        tokens.insert(repair.position, repair.token)
        return " ".join(tokens)


class MissingClosingBraceCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "missing_closing_brace"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] == "}"
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        corrupted_tokens = [
            tok["lexeme"] for i, tok in enumerate(tokens) if i != candidate and tok["type"] != "EOF"
        ]
        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="INSERT_TOKEN",
            token="}",
            position=candidate,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        tokens.insert(repair.position, repair.token)
        return " ".join(tokens)


class MissingCommaCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "missing_comma"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] == ","
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        corrupted_tokens = [
            tok["lexeme"] for i, tok in enumerate(tokens) if i != candidate and tok["type"] != "EOF"
        ]
        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="INSERT_TOKEN",
            token=",",
            position=candidate,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        tokens.insert(repair.position, repair.token)
        return " ".join(tokens)


class ReplaceDelimiterCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "replace_delimiter"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] in (")", "}")
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        original_tok = tokens[candidate]
        replacement = "]"
        corrupted_tokens = []
        for i, tok in enumerate(tokens):
            if tok["type"] == "EOF":
                continue
            if i == candidate:
                corrupted_tokens.append(replacement)
            else:
                corrupted_tokens.append(tok["lexeme"])

        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="REPLACE_TOKEN",
            token=original_tok["lexeme"],
            position=candidate,
            replaced_token=replacement,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        if repair.position < len(tokens):
            tokens[repair.position] = repair.token
        return " ".join(tokens)


class UnexpectedTokenCorruption(CorruptionStrategy):
    @property
    def name(self) -> str:
        return "unexpected_token"

    def find_candidates(self, tokens: List[dict], source: str) -> List[int]:
        return [
            i
            for i, tok in enumerate(tokens)
            if tok["lexeme"] == ";"
        ]

    def apply(
        self, candidate: int, tokens: List[dict], source: str
    ) -> Tuple[str, RepairLabel]:
        corrupted_tokens = []
        for i, tok in enumerate(tokens):
            if tok["type"] == "EOF":
                continue
            corrupted_tokens.append(tok["lexeme"])
            if i == candidate:
                corrupted_tokens.append(";")  # Extra semicolon

        corrupted_source = " ".join(corrupted_tokens)

        repair = _make_repair_label(
            operation="DELETE_TOKEN",
            token=";",
            position=candidate + 1,
        )
        return corrupted_source, repair

    def apply_repair(self, corrupted_source: str, repair: RepairLabel) -> str:
        tokens = corrupted_source.split()
        if repair.position < len(tokens):
            tokens.pop(repair.position)
        return " ".join(tokens)


CORRUPTION_REGISTRY: Dict[str, CorruptionStrategy] = {
    "missing_semicolon": MissingSemicolonCorruption(),
    "missing_closing_paren": MissingClosingParenCorruption(),
    "missing_closing_brace": MissingClosingBraceCorruption(),
    "missing_comma": MissingCommaCorruption(),
    "replace_delimiter": ReplaceDelimiterCorruption(),
    "unexpected_token": UnexpectedTokenCorruption(),
}
