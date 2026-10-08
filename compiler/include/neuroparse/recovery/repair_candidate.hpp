#pragma once

#include "neuroparse/lexer/token.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace neuroparse::recovery {

enum class RepairOperation {
  InsertToken,
  DeleteToken,
  ReplaceToken,
  NoRepair,
  Unknown,
};

[[nodiscard]] constexpr std::string_view to_string(RepairOperation op) noexcept {
  switch (op) {
    case RepairOperation::InsertToken:  return "INSERT_TOKEN";
    case RepairOperation::DeleteToken:  return "DELETE_TOKEN";
    case RepairOperation::ReplaceToken: return "REPLACE_TOKEN";
    case RepairOperation::NoRepair:     return "NO_REPAIR";
    case RepairOperation::Unknown:
    default:                            return "UNKNOWN";
  }
}

[[nodiscard]] inline RepairOperation repair_operation_from_string(std::string_view str) noexcept {
  if (str == "INSERT_TOKEN" || str == "insert" || str == "INSERT") {
    return RepairOperation::InsertToken;
  }
  if (str == "DELETE_TOKEN" || str == "delete" || str == "DELETE") {
    return RepairOperation::DeleteToken;
  }
  if (str == "REPLACE_TOKEN" || str == "replace" || str == "REPLACE") {
    return RepairOperation::ReplaceToken;
  }
  if (str == "NO_REPAIR" || str == "none" || str == "NOOP") {
    return RepairOperation::NoRepair;
  }
  return RepairOperation::Unknown;
}

struct RepairPrediction {
  RepairOperation operation = RepairOperation::Unknown;
  lexer::TokenType token_type = lexer::TokenType::Invalid;
  std::string token_lexeme;
  std::size_t position_offset = 0;

  double confidence = 1.0;
  double operation_confidence = 1.0;
  double token_confidence = 1.0;
  double position_confidence = 1.0;

  [[nodiscard]] std::string format() const {
    std::ostringstream ss;
    ss << to_string(operation);
    if (operation == RepairOperation::InsertToken || operation == RepairOperation::ReplaceToken) {
      ss << " token='" << (token_lexeme.empty() ? lexer::to_string(token_type) : token_lexeme) << "'";
    }
    ss << " pos=" << position_offset;
    ss << " conf=" << confidence;
    return ss.str();
  }

  bool operator==(const RepairPrediction&) const = default;
};

// Backward alias
using RepairCandidate = RepairPrediction;

}  // namespace neuroparse::recovery
