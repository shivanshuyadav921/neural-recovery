# Chapter 8: Classical Fallback Strategies

This chapter details the two deterministic baseline error recovery strategies implemented in **NeuroParse**: **`PanicModeRecovery`** and **`HeuristicRecovery`**, and compares them empirically to the neural recovery engine.

---

## 1. Role of Classical Fallbacks

While neural inference provides high accuracy on complex or localized errors, compilers require **hard 100% termination guarantees**. If a neural prediction is unavailable, invalid, rejected by the validator, or fails to advance parser state, the compiler immediately and deterministically transitions to classical recovery.

---

## 2. Panic-Mode Recovery (`PanicModeRecovery`)

Implemented in [`compiler/src/recovery/panic_mode.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/panic_mode.cpp), panic-mode recovery discards tokens until a known synchronization boundary is reached.

```cpp
class PanicModeRecovery final : public ErrorRecoveryStrategy {
public:
  RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context) override;

  static bool is_synchronization_token(lexer::TokenType type) noexcept;
  static bool is_statement_starter(lexer::TokenType type) noexcept;
};
```

### Synchronization Logic:
1. **Synchronization Tokens (Delimiters):**
   - `;` (`TokenType::Semicolon`)
   - `}` (`TokenType::RightBrace`)
2. **Statement Starters (Keywords):**
   - `let`, `if`, `while`, `return`, `function`
3. **Skipping Loop:**
   - Advances `cursor` while skipping invalid/unexpected tokens until reaching a synchronization token or statement starter.
   - Preserves `TokenType::Eof`.
   - Records skipped tokens in `RecoveryResult.tokens_skipped`.

---

## 3. Heuristic Recovery (`HeuristicRecovery`)

Implemented in [`compiler/src/recovery/heuristic_recovery.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/heuristic_recovery.cpp), heuristic recovery executes deterministic rule-based insertions for common programmer errors:

```cpp
class HeuristicRecovery final : public ErrorRecoveryStrategy {
  bool try_missing_semicolon(...);
  bool try_missing_closing_paren(...);
  bool try_missing_closing_brace(...);
  bool try_missing_comma(...);
  bool try_unexpected_extraneous_token(...);
};
```

### Rule Implementations:
1. **`try_missing_semicolon`:** If the error expected `;` and the current token is a statement starter (`let`, `return`, `if`), inserts `;` before the current token.
2. **`try_missing_closing_paren`:** If `)` was expected and the current token is `{` or `;`, inserts `)`.
3. **`try_missing_closing_brace`:** If `}` was expected and the parser is at `EOF` or next `function`, inserts `}`.
4. **`try_missing_comma`:** If inside a function parameter list or call argument list and an identifier follows another without separator, inserts `,`.
5. **`try_unexpected_extraneous_token`:** If a duplicate delimiter (e.g. `;;`) is detected, deletes the extraneous token.
6. **Panic Fallback:** If none of the 5 heuristic patterns match, delegates to `PanicModeRecovery`.

---

## 4. Empirical Performance Comparison

Measured on the 30 held-out test programs (`ml/dataset/generated/test.jsonl`):

| Metric | No Recovery | Panic-Mode | Heuristic Baseline | NeuroParse (Neural / Auto) |
|---|---|---|---|---|
| **Parse Success Rate** | 0.0% (0/30) | 60.0% (18/30) | 70.0% (21/30) | **96.7% (29/30)** |
| **Remaining Syntax Errors**| 35 errors | 13 errors | 14 errors | **2 errors** |
| **Cascading Error Reduction**| 0.0% (ref) | -62.9% | 0.0% (baseline) | **-85.7%** |
| **Inference Latency** | N/A | < 1 µs | < 5 µs | **1.19 ms (CPU)** |
| **Robustness Fuzzing (1k runs)**| Passes | Passes | Passes | **0 crashes, 0 hangs** |

### Why Neural Outperforms Heuristics:
Heuristics fail when syntax errors are non-trivial (e.g. mismatched delimiters across nested expressions, misplaced operator sequences, or corrupted function headers). Neural recovery evaluates bidirectional context (50 tokens before, 10 tokens after) to predict the exact missing token and location, eliminating 85.7% of cascading errors.
