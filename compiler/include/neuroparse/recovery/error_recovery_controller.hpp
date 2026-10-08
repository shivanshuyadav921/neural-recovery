#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/heuristic_recovery.hpp"
#include "neuroparse/recovery/neural_recovery.hpp"
#include "neuroparse/recovery/panic_mode.hpp"
#include "neuroparse/recovery/recovery_result.hpp"
#include "neuroparse/recovery/recovery_strategy.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

struct StrategyTelemetry {
  std::size_t attempts = 0;
  std::size_t successes = 0;
  std::size_t failures = 0;
  double total_latency_us = 0.0;
};

class ErrorRecoveryController {
public:
  explicit ErrorRecoveryController(RecoveryMode mode = RecoveryMode::Auto);
  explicit ErrorRecoveryController(std::unique_ptr<ErrorRecoveryStrategy> custom_strategy);
  explicit ErrorRecoveryController(std::vector<std::unique_ptr<ErrorRecoveryStrategy>> strategies);

  [[nodiscard]] RecoveryMode mode() const noexcept { return mode_; }
  void set_mode(RecoveryMode mode);

  // Strategy management
  void add_strategy(std::unique_ptr<ErrorRecoveryStrategy> strategy);
  void set_strategies(std::vector<std::unique_ptr<ErrorRecoveryStrategy>> strategies);
  void clear_strategies() noexcept;
  [[nodiscard]] std::size_t strategy_count() const noexcept { return strategies_.size(); }
  [[nodiscard]] std::vector<std::string_view> strategy_names() const;

  [[nodiscard]] bool is_recovery_enabled() const noexcept {
    return mode_ != RecoveryMode::None && !strategies_.empty();
  }

  // Central orchestration point: attempts recovery through registered strategy chain
  RecoveryResult attempt_recovery(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context);

  [[nodiscard]] const RecoveryMetrics& metrics() const noexcept { return metrics_; }
  [[nodiscard]] const std::vector<RecoveryResult>& history() const noexcept { return history_; }
  [[nodiscard]] const std::map<std::string, StrategyTelemetry>& strategy_telemetry() const noexcept {
    return strategy_telemetry_;
  }

  void reset() noexcept;

private:
  RecoveryMode mode_ = RecoveryMode::Auto;
  std::vector<std::unique_ptr<ErrorRecoveryStrategy>> strategies_;
  RecoveryMetrics metrics_;
  std::vector<RecoveryResult> history_;
  std::map<std::string, StrategyTelemetry> strategy_telemetry_;
  std::size_t last_recovery_token_idx_ = static_cast<std::size_t>(-1);
  std::size_t consecutive_same_pos_attempts_ = 0;
};

// Typedef for backward compatibility with Phase 5 references
using RecoveryController = ErrorRecoveryController;

}  // namespace neuroparse::recovery
