#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/recovery/repair_candidate.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

enum class RepairApplyStatus {
  Applied,
  NotApplied,
  InvalidState,
  NoRepair,
  NoProgress,
  ApplicationError,
};

[[nodiscard]] constexpr std::string_view to_string(RepairApplyStatus status) noexcept {
  switch (status) {
    case RepairApplyStatus::Applied:          return "APPLIED";
    case RepairApplyStatus::NotApplied:       return "NOT_APPLIED";
    case RepairApplyStatus::InvalidState:     return "INVALID_STATE";
    case RepairApplyStatus::NoRepair:         return "NO_REPAIR";
    case RepairApplyStatus::NoProgress:       return "NO_PROGRESS";
    case RepairApplyStatus::ApplicationError:
    default:                                  return "APPLICATION_ERROR";
  }
}

struct RepairApplyResult {
  bool applied = false;
  RepairApplyStatus status = RepairApplyStatus::ApplicationError;
  RepairOperation operation = RepairOperation::Unknown;
  std::size_t position = 0;
  std::size_t new_parser_position = 0;
  std::vector<lexer::Token> tokens_changed;
  std::string message;
};

class RepairApplier {
public:
  RepairApplier() = default;

  // Primary deterministic mutation method
  // Safely mutates tokens and adjusts cursor according to the accepted RepairPrediction.
  [[nodiscard]] RepairApplyResult apply(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const RepairPrediction& prediction) const;

private:
  [[nodiscard]] RepairApplyResult apply_insert(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const RepairPrediction& prediction) const;

  [[nodiscard]] RepairApplyResult apply_delete(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const RepairPrediction& prediction) const;

  [[nodiscard]] RepairApplyResult apply_replace(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const RepairPrediction& prediction) const;

  [[nodiscard]] RepairApplyResult apply_no_repair(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const RepairPrediction& prediction) const;
};

}  // namespace neuroparse::recovery
