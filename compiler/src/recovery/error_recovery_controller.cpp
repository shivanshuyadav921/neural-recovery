#include "neuroparse/recovery/error_recovery_controller.hpp"

#include <utility>

namespace neuroparse::recovery {

ErrorRecoveryController::ErrorRecoveryController(RecoveryMode mode)
    : mode_(mode) {
  set_mode(mode);
}

ErrorRecoveryController::ErrorRecoveryController(std::unique_ptr<ErrorRecoveryStrategy> custom_strategy)
    : mode_(RecoveryMode::Auto) {
  if (custom_strategy) {
    strategies_.push_back(std::move(custom_strategy));
  }
}

ErrorRecoveryController::ErrorRecoveryController(std::vector<std::unique_ptr<ErrorRecoveryStrategy>> strategies)
    : mode_(RecoveryMode::Auto), strategies_(std::move(strategies)) {}

void ErrorRecoveryController::set_mode(RecoveryMode mode) {
  mode_ = mode;
  strategies_.clear();

  switch (mode_) {
    case RecoveryMode::None:
      break;
    case RecoveryMode::Panic:
      strategies_.push_back(std::make_unique<PanicModeRecovery>());
      break;
    case RecoveryMode::Heuristic:
      strategies_.push_back(std::make_unique<HeuristicRecovery>(/*enable_panic_fallback=*/false));
      break;
    case RecoveryMode::Neural:
    case RecoveryMode::Auto:
      // In Auto/Neural mode, try Neural recovery first (validated by RepairValidator),
      // then fall back to Heuristic recovery, and finally Panic-mode synchronization.
      strategies_.push_back(std::make_unique<NeuralRecoveryStrategy>());
      strategies_.push_back(std::make_unique<HeuristicRecovery>(/*enable_panic_fallback=*/false));
      strategies_.push_back(std::make_unique<PanicModeRecovery>());
      break;
  }
}

void ErrorRecoveryController::add_strategy(std::unique_ptr<ErrorRecoveryStrategy> strategy) {
  if (strategy) {
    strategies_.push_back(std::move(strategy));
  }
}

void ErrorRecoveryController::set_strategies(std::vector<std::unique_ptr<ErrorRecoveryStrategy>> strategies) {
  strategies_ = std::move(strategies);
}

void ErrorRecoveryController::clear_strategies() noexcept {
  strategies_.clear();
}

std::vector<std::string_view> ErrorRecoveryController::strategy_names() const {
  std::vector<std::string_view> names;
  names.reserve(strategies_.size());
  for (const auto& strat : strategies_) {
    if (strat) {
      names.push_back(strat->name());
    }
  }
  return names;
}

RecoveryResult ErrorRecoveryController::attempt_recovery(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context) {
  metrics_.total_errors++;

  constexpr std::size_t kMaxRecoveriesPerParse = 50;
  if (metrics_.total_errors > kMaxRecoveriesPerParse) {
    RecoveryResult limit_res;
    limit_res.attempted = true;
    limit_res.success = false;
    limit_res.strategy = RecoveryStrategyType::None;
    limit_res.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;
    limit_res.original_token_index = cursor;
    limit_res.resulting_token_index = cursor;
    limit_res.description = "Maximum recovery attempt limit reached (loop protection).";
    metrics_.failed_recoveries++;
    history_.push_back(limit_res);
    return limit_res;
  }

  if (strategies_.empty() || mode_ == RecoveryMode::None) {
    RecoveryResult no_rec;
    no_rec.attempted = false;
    no_rec.success = false;
    no_rec.strategy = RecoveryStrategyType::None;
    no_rec.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;
    no_rec.original_token_index = cursor;
    no_rec.resulting_token_index = cursor;
    no_rec.description = "Recovery is disabled or no strategies registered.";
    metrics_.failed_recoveries++;
    history_.push_back(no_rec);
    return no_rec;
  }

  bool is_cascading = false;
  if (last_recovery_token_idx_ != static_cast<std::size_t>(-1)) {
    if (cursor == last_recovery_token_idx_) {
      consecutive_same_pos_attempts_++;
    } else {
      consecutive_same_pos_attempts_ = 0;
    }

    if (cursor >= last_recovery_token_idx_ && cursor <= last_recovery_token_idx_ + 2) {
      is_cascading = true;
      metrics_.cascading_errors++;
    }
  } else {
    consecutive_same_pos_attempts_ = 0;
  }

  // Save initial checkpoint for rollback if a strategy fails
  std::vector<lexer::Token> initial_tokens = tokens;
  std::size_t initial_cursor = cursor;
  double cumulative_latency_us = 0.0;

  for (auto& strategy : strategies_) {
    if (!strategy) continue;

    // Loop protection: If repeated recovery fails at the exact same location, skip local strategies and force panic/fallback
    if (consecutive_same_pos_attempts_ >= 2 &&
        strategy->strategy_type() != RecoveryStrategyType::Panic) {
      continue;
    }

    std::string strat_name = std::string(strategy->name());
    auto& telemetry = strategy_telemetry_[strat_name];
    telemetry.attempts++;

    // Execute strategy on token stream
    RecoveryResult result = strategy->recover(tokens, cursor, error, construct_context);
    cumulative_latency_us += result.latency_us;
    telemetry.total_latency_us += result.latency_us;

    if (result.success) {
      telemetry.successes++;
      result.is_cascading = is_cascading;
      result.latency_us = cumulative_latency_us;

      metrics_.recovered_errors++;
      metrics_.total_latency_us += cumulative_latency_us;
      last_recovery_token_idx_ = result.resulting_token_index;
      history_.push_back(result);
      return result;
    } else {
      telemetry.failures++;
      // Rollback token stream and cursor for the next strategy in chain
      tokens = initial_tokens;
      cursor = initial_cursor;
    }
  }

  // All strategies in the chain failed
  RecoveryResult failed_result;
  failed_result.attempted = true;
  failed_result.success = false;
  failed_result.strategy = RecoveryStrategyType::None;
  failed_result.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;
  failed_result.original_token_index = initial_cursor;
  failed_result.resulting_token_index = cursor;
  failed_result.latency_us = cumulative_latency_us;
  failed_result.is_cascading = is_cascading;
  failed_result.description = "All registered recovery strategies failed to recover.";

  metrics_.failed_recoveries++;
  metrics_.total_latency_us += cumulative_latency_us;
  history_.push_back(failed_result);
  return failed_result;
}

void ErrorRecoveryController::reset() noexcept {
  metrics_ = RecoveryMetrics{};
  history_.clear();
  strategy_telemetry_.clear();
  last_recovery_token_idx_ = static_cast<std::size_t>(-1);
  consecutive_same_pos_attempts_ = 0;
}

}  // namespace neuroparse::recovery
