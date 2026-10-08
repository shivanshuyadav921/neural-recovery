from dataclasses import asdict, dataclass, field
from typing import Any, Dict, List, Optional


@dataclass
class TokenRecord:
    type: str
    lexeme: str
    line: int
    column: int
    category: Optional[str] = None
    offset: Optional[int] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class RepairLabel:
    operation: str  # INSERT_TOKEN, DELETE_TOKEN, REPLACE_TOKEN
    token: str
    position: int
    replaced_token: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class ErrorRecord:
    token_index: int
    line: int
    column: int
    message: str
    expected_tokens: List[str] = field(default_factory=list)
    construct: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class ContextRecord:
    tokens_before: List[Dict[str, Any]]
    current_token: Dict[str, Any]
    tokens_after: List[Dict[str, Any]]
    expected_tokens: List[str] = field(default_factory=list)
    construct: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class DatasetSample:
    sample_id: str
    source_file: str
    original_source: str
    corrupted_source: str
    corruption_type: str
    repair: RepairLabel
    error: ErrorRecord
    context: ContextRecord

    def to_dict(self) -> Dict[str, Any]:
        return {
            "sample_id": self.sample_id,
            "source_file": self.source_file,
            "original_source": self.original_source,
            "corrupted_source": self.corrupted_source,
            "corruption_type": self.corruption_type,
            "repair": self.repair.to_dict(),
            "error": self.error.to_dict(),
            "context": self.context.to_dict(),
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "DatasetSample":
        return cls(
            sample_id=data["sample_id"],
            source_file=data.get("source_file", ""),
            original_source=data["original_source"],
            corrupted_source=data["corrupted_source"],
            corruption_type=data["corruption_type"],
            repair=RepairLabel(**data["repair"]),
            error=ErrorRecord(**data["error"]),
            context=ContextRecord(**data["context"]),
        )


@dataclass
class DatasetMetadata:
    dataset_version: str
    language: str
    generator_version: str
    random_seed: int
    context_before: int
    context_after: int
    corruption_types: List[str]
    split_ratios: Dict[str, float]

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class GenerationReport:
    total_samples_generated: int
    samples_per_corruption: Dict[str, int]
    train_count: int
    validation_count: int
    test_count: int
    rejected_count: int
    rejection_reasons: Dict[str, int]

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)
