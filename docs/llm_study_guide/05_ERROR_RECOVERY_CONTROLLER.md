# Chapter 5: Error Recovery Controller & Orchestration

This chapter explores the central recovery coordinator, **`ErrorRecoveryController`**, its strategy dispatch algorithms, loop detection invariants, cascading error tracking, and telemetry infrastructure.

---

## 1. Responsibilities & Architecture

The `ErrorRecoveryController` ([`compiler/include/neuroparse/recovery/error_recovery_controller.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/error_recovery_controller.hpp)) manages the lifecycle of all error recovery operations.

### Key Responsibilities:
1. **Strategy Chain Management:** Maintains ordered recovery strategies (Neural $\to$ Heuristic $\to$ Panic).
2. **Infinite Loop Protection:** Enforces strict upper bounds on recovery attempts per token position and per file.
3. **Telemetry & Metrics:** Accurately measures recovery latency, success rates, and flags cascading errors.
4. **Deterministic Rollback:** If an advanced strategy fails, gracefully falls back to classical methods.

---

## 2. Recovery Modes

The compiler CLI and VS Code extension configure the recovery controller via `RecoveryMode`:

| Mode | Enumerator | Behavior |
|---|---|---|
| **Auto** | `RecoveryMode::Auto` | Default mode. Evaluates Neural first; falls back to Heuristic, then Panic Mode. |
| **Neural** | `RecoveryMode::Neural` | Strictly uses `NeuralRecoveryStrategy` (with `RepairValidator`). Rejects fallback. |
| **Heuristic** | `RecoveryMode::Heuristic` | Classical rule-based delimiter and delimiter replacement. |
| **Panic** | `RecoveryMode::Panic` | Synchronizes to statement boundaries (`;`) or starter keywords (`let`, `if`, etc.). |
| **None** | `RecoveryMode::None` | Disables recovery completely (fails fast upon first syntax error). |

---

## 3. The `attempt_recovery()` Orchestration Loop

```cpp
RecoveryResult ErrorRecoveryController::attempt_recovery(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context)
```

### Operational Steps:
1. **Loop Protection (Same-Position Bound):**
   ```cpp
   if (cursor == last_recovery_token_idx_) {
     ++consecutive_same_pos_attempts_;
     if (consecutive_same_pos_attempts_ > 3) {
       // Abort recovery at this position to prevent parser freeze!
       return fail_result("Exceeded maximum consecutive recovery attempts at same token index.");
     }
   } else {
     last_recovery_token_idx_ = cursor;
     consecutive_same_pos_attempts_ = 1;
   }
   ```
2. **Global Budget Guard:**
   Ensures total errors across the compilation unit do not exceed $K = 50$.
3. **Cascading Error Detection:**
   If an error occurs within $\le 3$ tokens of a previously applied recovery, it is flagged with `is_cascading = true`.
4. **Strategy Chain Execution:**
   Iterates through registered strategies. For each strategy:
   - Measures start time with high-resolution clock (`std::chrono::steady_clock`).
   - Invokes `strategy->recover(tokens, cursor, error, construct_context)`.
   - Records latency into `StrategyTelemetry`.
   - If `result.success == true`, appends to `history_`, increments `metrics_.recovered_errors`, and immediately returns.
5. **Final Fallback:**
   If all strategies in the chain fail, increments `metrics_.failed_recoveries` and returns a failed `RecoveryResult`.

---

## 4. Recovery Data Structures

### `RecoveryResult`
```cpp
struct RecoveryResult {
  bool attempted = false;
  bool success = false;
  RecoveryStrategyType strategy = RecoveryStrategyType::None; // Neural, Heuristic, Panic
  RecoveryActionType action = RecoveryActionType::None;       // InsertToken, DeleteToken, ReplaceToken, SkipTokens
  lexer::SourceLocation error_location;
  std::vector<lexer::Token> tokens_skipped;
  std::vector<lexer::Token> tokens_inserted;
  std::size_t original_token_index = 0;
  std::size_t resulting_token_index = 0;
  std::string description;
  double latency_us = 0.0;
  bool is_cascading = false;
};
```

### `RecoveryMetrics`
```cpp
struct RecoveryMetrics {
  std::size_t total_errors = 0;
  std::size_t recovered_errors = 0;
  std::size_t failed_recoveries = 0;
  std::size_t cascading_errors = 0;
  double total_latency_us = 0.0;

  [[nodiscard]] double recovery_rate() const noexcept {
    return total_errors == 0 ? 1.0 : (double)recovered_errors / total_errors;
  }
};
```
