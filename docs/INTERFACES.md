# NeuroParse Interface Contracts & System Invariants (Phase 18)

This document formally specifies the frozen contracts, data structures, invariants, error handling, and ownership models across all internal and boundary interfaces in the NeuroParse compiler.

---

## 1. Architectural Pipeline Overview

```
Source Code (.nl)
       │
       ▼
   [ Lexer ] ────────────► Tokens + LexerErrors
       │
       ▼
   [ Parser ] ───────────► Syntax Error Event
       │                         │
       │                         ▼
       │             [ ErrorRecoveryController ]
       │                         │
       │                         ▼
       │               [ ContextExtractor ] ──► ErrorContext
       │                         │
       │                         ▼
       │            [ NeuralInferenceEngine ] ──► RepairPrediction
       │                         │
       │                         ▼
       │                [ RepairValidator ] ──► RepairValidationResult
       │                    │          │
       │              ACCEPT│          │REJECT
       │                    ▼          ▼
       │             [RepairApplier] [Deterministic Recovery]
       │                    │          │
       │                    └────┬─────┘
       │                         │
       ▼                         ▼
   [ Parser Resumption ] ◄───────┘
       │
       ▼
  ParseResult (AST + Diagnostics + RecoveryMetrics)
```

---

## 2. Core Compiler Component Interfaces

### 2.1 Lexer $\longrightarrow$ Parser
- **Producer:** `neuroparse::lexer::Lexer`
- **Consumer:** `neuroparse::parser::Parser`
- **Data Contract:** `std::vector<neuroparse::lexer::Token>` and `std::vector<neuroparse::lexer::LexerError>`
- **Token Struct Invariants:**
  - `type`: Strongly typed `TokenType` enum.
  - `lexeme`: String slice or owned token text.
  - `location`: Monotonic `SourceLocation` (`line >= 1`, `column >= 1`).
  - `stream`: Exactly one `TokenType::Eof` as the terminal token (`tokens.back()`). No interior EOFs.
- **Ownership:** `Parser` takes ownership of the token stream via `std::vector<lexer::Token>` (move semantics).
- **Error Behavior:** If lexical errors exist, `Lexer::tokenize()` records them in `LexResult::errors`; the compiler reports lexical diagnostics and aborts before parsing.

---

### 2.2 Parser $\longrightarrow$ ErrorRecoveryController
- **Caller:** `neuroparse::parser::Parser`
- **Callee:** `neuroparse::recovery::ErrorRecoveryController`
- **Trigger:** Encountering an unexpected token or unfulfilled grammar expectation during recursive descent or Pratt expression parsing.
- **Data Passed:**
  - `tokens`: Mutable reference `std::vector<lexer::Token>&`
  - `cursor`: Current token stream index `std::size_t&`
  - `error`: `parser::ParserError` containing message, location, construct, and expected terminals.
- **Invariants Enforced:**
  - **Loop Prevention:** Tracks consecutive recovery attempts at the same token index. If `consecutive_same_pos_attempts_ >= 2`, neural recovery is bypassed and deterministic fallback is triggered.
  - **Global Budget:** Limits total recovery attempts to `kMaxRecoveriesPerParse = 50` per translation unit.
- **Return Contract:** `recovery::RecoveryResult` containing `success`, `action`, `strategy`, `tokens_skipped`, `tokens_inserted`, `resulting_token_index`, and `latency_us`.

---

### 2.3 ErrorRecoveryController $\longrightarrow$ ContextExtractor
- **Caller:** `ErrorRecoveryController`
- **Callee:** `neuroparse::recovery::ContextExtractor`
- **Input:**
  - `tokens`: `const std::vector<lexer::Token>&`
  - `error`: `const parser::ParserError&`
  - `config`: `recovery::ContextWindowConfig` (default: `tokens_before = 50`, `tokens_after = 10`, `max_window_size = 64`).
- **Output:** Immutable `recovery::ErrorContext` struct:
  - `tokens_before`: Sliced left context tokens.
  - `error_token`: The token where syntax failure was detected.
  - `tokens_after`: Sliced right context tokens.
  - `expected_tokens`: Set of expected `TokenType` terminals.
  - `construct`: String hint of the active syntactic construct (e.g. `"let binding"`).
- **Invariants:**
  - Slices never exceed `tokens` buffer boundaries.
  - Does NOT mutate compiler state or token streams.

---

### 2.4 ContextExtractor $\longrightarrow$ NeuralInferenceEngine
- **Caller:** `ErrorRecoveryController`
- **Callee:** `neuroparse::recovery::NeuralInferenceEngine`
- **Input:** `const recovery::ErrorContext&`
- **Output:** `recovery::RepairPrediction` struct:
  - `operation`: `RecoveryOperation` (`InsertToken`, `DeleteToken`, `ReplaceToken`, `NoRepair`).
  - `token_type`: Predicted `lexer::TokenType`.
  - `lexeme`: Synthetic token text (e.g. `";"`).
  - `position_offset`: Token stream offset relative to error token.
  - `confidence`: Normalized probability score $[0.0, 1.0]$.
- **Error & Failure Contract:**
  - If inference fails (model file missing, runtime exception, low confidence $< \text{threshold}$), the engine returns `RecoveryOperation::NoRepair` with `confidence = 0.0`.
  - The compiler catches all inference exceptions and safely routes to fallback.

---

### 2.5 NeuralInferenceEngine $\longrightarrow$ RepairValidator
- **Caller:** `ErrorRecoveryController`
- **Callee:** `neuroparse::recovery::RepairValidator`
- **Input:** `const recovery::ErrorContext&`, `const recovery::RepairPrediction&`, `std::size_t token_stream_size`
- **Validation Pipeline (6 Stages):**
  1. `Operation Validity`: Asserts operation is a recognized enum.
  2. `Position Bounds`: Asserts target index is within $[0, \text{token\_stream\_size}]$.
  3. `Token Validity`: Asserts predicted token is legal for the operation (cannot insert `Invalid` or `Eof`).
  4. `Parser Context Compatibility`: Asserts predicted token matches the parser's expectation set when applicable.
  5. `Structural Safety`: Asserts EOF cannot be deleted or relocated.
  6. `Forward Progress`: Asserts proposed edit changes stream state.
- **Output:** `recovery::RepairValidationResult` (`is_accepted: bool`, `status: ValidationStatus`, `rejection_reason: string`).

---

### 2.6 RepairValidator $\longrightarrow$ RepairApplier
- **Caller:** `ErrorRecoveryController`
- **Callee:** `neuroparse::recovery::RepairApplier`
- **Precondition:** `validation_result.is_accepted == true`
- **Input:** `std::vector<lexer::Token>& tokens`, `std::size_t& cursor`, `const recovery::RepairPrediction&`
- **Mutations Supported:**
  - `InsertToken`: Inserts synthetic token at target index, preserves source line/column anchoring, increments cursor if insertion is before or at current position.
  - `DeleteToken`: Erases target token, clamps cursor to valid stream bounds.
  - `ReplaceToken`: Overwrites token at target index with synthetic replacement token.
- **Output:** `recovery::RepairApplyResult` (`success: bool`, `resulting_cursor: size_t`, `tokens_mutated: vector<Token>`).

---

### 2.7 RepairApplier $\longrightarrow$ Parser Resumption $\longrightarrow$ AST & Diagnostics
- **Resumption Protocol:**
  - Parser updates internal stream and cursor to `resulting_cursor`.
  - Parser continues standard recursive descent / Pratt parsing loop.
- **Final Result Struct (`parser::ParseResult`):**
  - `program`: `std::unique_ptr<ast::Program>` (Root node if parse completed; `nullptr` if unrecoverable).
  - `errors`: `std::vector<parser::ParserError>` (List of all diagnosed errors).
  - `recoveries`: `std::vector<recovery::RecoveryResult>` (Trace of all recovery attempts and outcomes).
  - `metrics`: `recovery::RecoveryMetrics` (Total errors, recovered count, failed count, cascading count, latency).

---

## 3. Machine Learning & Artifact Contracts

### 3.1 Dataset Schema (`ml/dataset/schema.py`)
Each sample in `ml/dataset/generated/*.jsonl` adheres strictly to:
```json
{
  "sample_id": "string",
  "source_file": "string",
  "corruption_type": "string",
  "corrupted_source": "string",
  "context": {
    "tokens_before": [{"type": "string", "lexeme": "string", "line": 0, "column": 0}],
    "current_token": {"type": "string", "lexeme": "string", "line": 0, "column": 0},
    "tokens_after": [{"type": "string", "lexeme": "string", "line": 0, "column": 0}],
    "expected_tokens": ["string"],
    "construct": "string"
  },
  "ground_truth_repair": {
    "operation": "INSERT_TOKEN | DELETE_TOKEN | REPLACE_TOKEN | NO_REPAIR",
    "token": "string",
    "position": 0
  }
}
```

---

### 3.2 ONNX Model Contract (`models/repair_model.onnx`)
- **Opset Version:** 18
- **Input Tensors (INT64):**
  - `input_ids`: `[batch_size, sequence_length]` (Dynamic axes: 0, 1)
  - `attention_mask`: `[batch_size, sequence_length]` (Dynamic axes: 0, 1)
  - `segment_ids`: `[batch_size, sequence_length]` (Dynamic axes: 0, 1)
  - `error_positions`: `[batch_size]` (Dynamic axis: 0)
- **Output Tensors (FLOAT32):**
  - `operation_logits`: `[batch_size, 4]` (Dynamic axis: 0)
  - `token_logits`: `[batch_size, 42]` (Dynamic axis: 0)
  - `position_logits`: `[batch_size, 128]` (Dynamic axis: 0)

---

## 4. Compiler CLI $\longleftrightarrow$ VS Code Extension Contract

The VS Code extension communicates with the `neuroparse` binary exclusively via standard process invocation (`stdout` / `stderr` / `exit_code`).

### Invocation Command:
```bash
neuroparse <file.nl> --diagnostics=json --recovery=<mode>
```

### JSON Diagnostics Schema:
```json
{
  "schema_version": "1.0.0",
  "file": "string",
  "success": true,
  "metrics": {
    "total_errors": 0,
    "recovered_errors": 0,
    "failed_recoveries": 0,
    "cascading_errors": 0,
    "recovery_rate": 1.0,
    "total_latency_us": 0.0
  },
  "diagnostics": [
    {
      "source": "lexer | parser | cli",
      "severity": "error | warning | info",
      "message": "string",
      "line": 1,
      "column": 1,
      "end_line": 1,
      "end_column": 2,
      "code": "SYN001",
      "construct": "string",
      "expected_tokens": ["string"],
      "recovery": {
        "strategy": "neural | heuristic | panic | none",
        "action": "insert_token | delete_token | replace_token | skip_tokens",
        "status": "RECOVERED | FAILED",
        "description": "string",
        "latency_us": 45.0,
        "is_cascading": false,
        "suggested_edit": {
          "operation": "insert | delete | replace",
          "line": 1,
          "column": 1,
          "end_line": 1,
          "end_column": 1,
          "new_text": ";"
        }
      }
    }
  ]
}
```

### Extension Safety & Authority Invariant:
The VS Code extension **never** invents repairs, guesses syntax fixes, or executes arbitrary code. It renders diagnostics and Quick Fixes strictly produced and validated by the compiler executable.
