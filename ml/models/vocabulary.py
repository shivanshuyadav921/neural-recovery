from typing import Dict, List, Optional, Set, Tuple

# Special tokens
PAD_TOKEN = "<PAD>"
UNK_TOKEN = "<UNK>"
BOS_TOKEN = "<BOS>"
EOS_TOKEN = "<EOS>"
ERROR_TOKEN = "<ERROR>"

SPECIAL_TOKENS = [PAD_TOKEN, UNK_TOKEN, BOS_TOKEN, EOS_TOKEN, ERROR_TOKEN]

# NeuroLang structural token types and delimiters
NEUROLANG_KEYWORDS = [
    "function",
    "let",
    "if",
    "else",
    "while",
    "return",
    "true",
    "false",
    "null",
]

NEUROLANG_DELIMITERS = [
    "(",
    ")",
    "{",
    "}",
    ";",
    ",",
    "[",
    "]",
]

NEUROLANG_OPERATORS = [
    "+",
    "-",
    "*",
    "/",
    "%",
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",
    "=",
    "!",
    "&&",
    "||",
]

NEUROLANG_LITERAL_CATEGORIES = [
    "IDENTIFIER",
    "INTEGER",
    "FLOAT",
    "STRING",
    "EOF",
]

# Supported repair operations
REPAIR_OPERATIONS = [
    "INSERT_TOKEN",
    "DELETE_TOKEN",
    "REPLACE_TOKEN",
    "NO_REPAIR",
]


class Vocabulary:
    def __init__(self):
        self.special_tokens = list(SPECIAL_TOKENS)
        self.keywords = list(NEUROLANG_KEYWORDS)
        self.delimiters = list(NEUROLANG_DELIMITERS)
        self.operators = list(NEUROLANG_OPERATORS)
        self.literal_categories = list(NEUROLANG_LITERAL_CATEGORIES)

        # Build token-to-id mapping deterministically
        self.tokens: List[str] = (
            self.special_tokens
            + self.keywords
            + self.delimiters
            + self.operators
            + self.literal_categories
        )

        self.token_to_id: Dict[str, int] = {tok: idx for idx, tok in enumerate(self.tokens)}
        self.id_to_token: Dict[int, str] = {idx: tok for idx, tok in enumerate(self.tokens)}

        self.pad_id = self.token_to_id[PAD_TOKEN]
        self.unk_id = self.token_to_id[UNK_TOKEN]
        self.bos_id = self.token_to_id[BOS_TOKEN]
        self.eos_id = self.token_to_id[EOS_TOKEN]
        self.error_id = self.token_to_id[ERROR_TOKEN]

        # Operations mapping
        self.operations: List[str] = list(REPAIR_OPERATIONS)
        self.op_to_id: Dict[str, int] = {op: idx for idx, op in enumerate(self.operations)}
        self.id_to_op: Dict[int, str] = {idx: op for idx, op in enumerate(self.operations)}

    @property
    def size(self) -> int:
        return len(self.tokens)

    @property
    def num_operations(self) -> int:
        return len(self.operations)

    def normalize_token(self, token_info: any) -> str:
        """Normalize token record from lexer/ContextExtractor to vocabulary token."""
        if not isinstance(token_info, dict):
            return str(token_info) if str(token_info) in self.token_to_id else UNK_TOKEN

        tok_type = token_info.get("type", "")
        lexeme = token_info.get("lexeme", "")
        category = token_info.get("category", "")

        # Exact match on lexeme (e.g. delimiters, operators, keywords)
        if lexeme in self.token_to_id:
            return lexeme

        # Exact match on type
        if tok_type in self.token_to_id:
            return tok_type

        # Literal categories
        if category in self.token_to_id:
            return category

        if tok_type in ("IDENTIFIER", "INTEGER", "FLOAT", "STRING", "EOF"):
            return tok_type

        return UNK_TOKEN

    def encode_token(self, token_info: any) -> int:
        norm = self.normalize_token(token_info)
        return self.token_to_id.get(norm, self.unk_id)

    def decode_token(self, token_id: int) -> str:
        return self.id_to_token.get(token_id, UNK_TOKEN)

    def encode_operation(self, op_name: str) -> int:
        return self.op_to_id.get(op_name, self.op_to_id["NO_REPAIR"])

    def decode_operation(self, op_id: int) -> str:
        return self.id_to_op.get(op_id, "NO_REPAIR")
