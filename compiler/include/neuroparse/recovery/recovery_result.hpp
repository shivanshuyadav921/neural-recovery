#pragma once

#include "neuroparse/lexer/token.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

enum class RecoveryStrategyType {
  None,
  Panic,
  Heuristic,
  Neural,
};

[[nodiscard]] constexpr std::string_view to_string(RecoveryStrategyType type) noexcept {
  switch (type) {
    case RecoveryStrategyType::None:      return "none";
    case RecoveryStrategyType::Panic:     return "panic";
    case RecoveryStrategyType::Heuristic: return "heuristic";
    case RecoveryStrategyType::Neural:    return "neural";
    default:                              return "unknown";
  }
}

enum class RecoveryActionType {
  None,
  SkipTokens,
  InsertToken,
  DeleteToken,
  ReplaceToken,
  SynchronizeStatement,
};

[[nodiscard]] constexpr std::string_view to_string(RecoveryActionType action) noexcept {
  switch (action) {
    case RecoveryActionType::None:                 return "none";
    case RecoveryActionType::SkipTokens:           return "skip_tokens";
    case RecoveryActionType::InsertToken:          return "insert_token";
    case RecoveryActionType::DeleteToken:          return "delete_token";
    case RecoveryActionType::ReplaceToken:         return "replace_token";
    case RecoveryActionType::SynchronizeStatement: return "synchronize_statement";
    default:                                       return "unknown";
  }
}

enum class RecoveryMode {
  Auto,
  None,
  Panic,
  Heuristic,
  Neural,
};

[[nodiscard]] constexpr std::string_view to_string(RecoveryMode mode) noexcept {
  switch (mode) {
    case RecoveryMode::Auto:      return "auto";
    case RecoveryMode::None:      return "none";
    case RecoveryMode::Panic:     return "panic";
    case RecoveryMode::Heuristic: return "heuristic";
    case RecoveryMode::Neural:    return "neural";
    default:                      return "unknown";
  }
}

struct RecoveryResult {
  bool attempted = false;
  bool success = false;
  RecoveryStrategyType strategy = RecoveryStrategyType::None;
  RecoveryActionType action = RecoveryActionType::None;
  lexer::SourceLocation error_location;
  std::vector<lexer::Token> tokens_skipped;
  std::vector<lexer::Token> tokens_inserted;
  std::size_t original_token_index = 0;
  std::size_t resulting_token_index = 0;
  std::string description;
  double latency_us = 0.0;
  bool is_cascading = false;
};

struct RecoveryMetrics {
  std::size_t total_errors = 0;
  std::size_t recovered_errors = 0;
  std::size_t failed_recoveries = 0;
  std::size_t cascading_errors = 0;
  double total_latency_us = 0.0;

  [[nodiscard]] double recovery_rate() const noexcept {
    if (total_errors == 0) return 1.0;
    return static_cast<double>(recovered_errors) / static_cast<double>(total_errors);
  }

  [[nodiscard]] double average_latency_us() const noexcept {
    std::size_t attempts = recovered_errors + failed_recoveries;
    if (attempts == 0) return 0.0;
    return total_latency_us / static_cast<double>(attempts);
  }
};

}  // namespace neuroparse::recovery
