# Error Recovery & Neural Repair Pipeline (Phase 14)

This document specifies the end-to-end neural error recovery pipeline, deterministic validation architecture, token mutation applier, and authoritative parser resumption in NeuroParse.

---

## 1. Core Principle: AI Proposes, Compiler Validates, Parser Applies, Parser Recovers

The neural repair model operates strictly as an untrusted proposal mechanism. It never directly mutates the token stream, AST, or compiler state. All proposed repairs must pass deterministic compiler-side validation before application. Once applied by `RepairApplier`, the parser deterministically resumes and acts as the authoritative final judge of syntactic validity.

```
                         ┌───────────────┐
                         │    Parser     │
                         └───────┬───────┘
                                 │
                            Syntax Error
                                 │
                                 ▼
                       ┌──────────────────┐
                       │ ContextExtractor │
                       └────────┬─────────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │ NeuralInference  │
                       │ Engine           │
                       └────────┬─────────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │ RepairPrediction │
                       └────────┬─────────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │ RepairValidator  │
                       └───────┬───┬──────┘
                               │   │
                         ACCEPT│   │REJECT
                               │   │
                               ▼   ▼
                     ┌────────────┐ ┌───────────────┐
                     │RepairApplier│ │ Deterministic │
                     └──────┬─────┘ │ Recovery      │
                            │       └───────────────┘
                            ▼
                       Parser Resume
                            │
                            ▼
                           AST
```

---

## 2. Pipeline Subsystems

### A. Context Extractor (`ContextExtractor`)
- Extracts structured lexical and syntactic context around the syntax error token.
- Produces `ErrorContext` including tokens before, error token, tokens after, expected tokens, and grammar construct.

### B. Neural Inference Engine (`NeuralInferenceEngine`)
- Evaluates `ErrorContext` to produce candidate `RepairPrediction` structs.
- Generates operation (`INSERT_TOKEN`, `DELETE_TOKEN`, `REPLACE_TOKEN`, `NO_REPAIR`), token type, lexeme, position offset, and confidence scores.

### C. Deterministic Repair Validator (`RepairValidator`)
- Non-mutating safety gate enforcing compiler grammar invariants across 6 verification levels:
  1. `operation_validity`
  2. `position_bounds`
  3. `token_validity`
  4. `parser_context_compatibility`
  5. `structural_safety`
  6. `forward_progress`
- Returns `RepairValidationResult` (`Accepted` vs rejection statuses).

### D. Deterministic Repair Applier (`RepairApplier`)
- Safely mutates the parser's `std::vector<lexer::Token>&` stream and `cursor`.
- Supports:
  - `INSERT_TOKEN`: Inserts synthetic token with properly anchored source location and advances/adjusts parser cursor.
  - `DELETE_TOKEN`: Erases extraneous token with strict EOF protection.
  - `REPLACE_TOKEN`: Replaces malformed token with synthetic token preserving source span.
  - `NO_REPAIR`: Leaves stream untouched.
- Returns structured `RepairApplyResult`.

### E. Authoritative Parser Resumption
- Following repair application, the parser resumes from the mutation location without restarting compilation from scratch.
- If the repaired token stream is valid, parsing succeeds and produces an AST.
- If the repair candidate fails to progress or still produces syntax errors, the `ErrorRecoveryController` rolls back and falls back to deterministic heuristic / panic-mode recovery.

---

## 3. Loop Protection & Multi-Error Safety

The controller enforces strict loop protection guarantees:
1. **Consecutive Location Tracking**: Repeated failures at the exact same token index trigger automatic fallback to synchronization.
2. **Global Parse Limit**: Limits total recovery attempts per parse pass (`kMaxRecoveriesPerParse = 50`) to mathematically prevent infinite recovery cycles.
3. **Sequential Multi-Error Processing**: Multiple errors in a single source file are repaired iteratively and sequentially, producing complete ASTs.

---

## 4. Diagnostics & Recovery Trace

NeuroParse provides two diagnostic display modes:

### Recovery Report (`--explain-recovery` / `--explain`)
Displays aggregate recovery statistics and per-error diagnostic breakdowns.

### Recovery Trace (`--trace-recovery`)
Provides step-by-step trace of each recovery event:
```
=== NeuroParse Recovery Trace ===
File: examples/syntax_errors/missing_semicolon.nl
[STEP 1]
  [ERROR] examples/syntax_errors/missing_semicolon.nl:6:1: syntax error: Expected ';' after return statement. (expected: ';')
  [STRATEGY] neural
  [ACTION] insert_token
  [STATUS] ACCEPTED & APPLIED
  [DESCRIPTION] Validated neural repair applied: INSERT_TOKEN token=';' pos=12 conf=0.95
  [LATENCY] 65 µs
  [PARSER RESUME] index 12 -> 12
```
