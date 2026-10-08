#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/error_context.hpp"
#include "neuroparse/recovery/repair_candidate.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

enum class ValidationStatus {
  Accepted,
  InvalidOperation,
  InvalidPosition,
  InvalidToken,
  NotExpected,
  ContextMismatch,
  UnsafeRepair,
  WouldNotProgress,
  WouldCorruptStructure,
  ValidationError,
};

[[nodiscard]] constexpr std::string_view to_string(ValidationStatus status) noexcept {
  switch (status) {
    case ValidationStatus::Accepted:               return "ACCEPTED";
    case ValidationStatus::InvalidOperation:       return "INVALID_OPERATION";
    case ValidationStatus::InvalidPosition:        return "INVALID_POSITION";
    case ValidationStatus::InvalidToken:           return "INVALID_TOKEN";
    case ValidationStatus::NotExpected:            return "NOT_EXPECTED";
    case ValidationStatus::ContextMismatch:        return "CONTEXT_MISMATCH";
    case ValidationStatus::UnsafeRepair:           return "UNSAFE_REPAIR";
    case ValidationStatus::WouldNotProgress:       return "WOULD_NOT_PROGRESS";
    case ValidationStatus::WouldCorruptStructure:  return "WOULD_CORRUPT_STRUCTURE";
    case ValidationStatus::ValidationError:
    default:                                       return "VALIDATION_ERROR";
  }
}

struct ValidationCheck {
  std::string name;
  bool passed = false;
  std::string message;
};

struct ValidationChecks {
  ValidationCheck operation_check{"operation_validity", false, ""};
  ValidationCheck position_check{"position_bounds", false, ""};
  ValidationCheck token_check{"token_validity", false, ""};
  ValidationCheck context_check{"parser_context_compatibility", false, ""};
  ValidationCheck structural_safety_check{"structural_safety", false, ""};
  ValidationCheck progress_check{"forward_progress", false, ""};

  [[nodiscard]] bool all_passed() const noexcept {
    return operation_check.passed &&
           position_check.passed &&
           token_check.passed &&
           context_check.passed &&
           structural_safety_check.passed &&
           progress_check.passed;
  }

  [[nodiscard]] std::vector<ValidationCheck> list() const {
    return {operation_check, position_check, token_check,
            context_check, structural_safety_check, progress_check};
  }
};

struct RepairValidationResult {
  bool accepted = false;
  ValidationStatus status = ValidationStatus::ValidationError;
  std::string reason;
  std::string details;
  ValidationChecks checks;
  RepairPrediction normalized_prediction;
  double confidence = 0.0;

  [[nodiscard]] bool is_accepted() const noexcept { return accepted; }

  // Formats a structured, human-readable explainability report
  [[nodiscard]] std::string explain() const {
    std::ostringstream ss;
    ss << "Proposed repair:\n";
    ss << "  operation = " << to_string(normalized_prediction.operation) << "\n";
    if (normalized_prediction.operation == RepairOperation::InsertToken ||
        normalized_prediction.operation == RepairOperation::ReplaceToken) {
      ss << "  token     = " << (normalized_prediction.token_lexeme.empty()
                                  ? std::string(lexer::to_string(normalized_prediction.token_type))
                                  : normalized_prediction.token_lexeme) << "\n";
    }
    ss << "  position  = " << normalized_prediction.position_offset << "\n";
    ss << "  conf      = " << confidence << "\n";
    ss << "\nValidation checks:\n";
    for (const auto& check : checks.list()) {
      ss << "  [" << (check.passed ? "PASS" : "FAIL") << "] " << check.name;
      if (!check.message.empty()) {
        ss << " - " << check.message;
      }
      ss << "\n";
    }
    ss << "\nRESULT: " << to_string(status) << "\n";
    if (!reason.empty()) {
      ss << "Reason: " << reason << "\n";
    }
    if (!details.empty()) {
      ss << "Details: " << details << "\n";
    }
    return ss.str();
  }
};

class RepairValidator {
public:
  struct Config {
    bool require_expected_token_match = true;
    bool allow_no_repair = true;
    bool strict_delimiter_matching = true;
    double min_confidence_threshold = 0.0;
  };

  RepairValidator() = default;
  explicit RepairValidator(Config config) : config_(std::move(config)) {}

  [[nodiscard]] const Config& config() const noexcept { return config_; }
  void set_config(Config config) noexcept { config_ = std::move(config); }

  // Primary non-mutating validation methods
  [[nodiscard]] RepairValidationResult validate(
      const std::vector<lexer::Token>& tokens,
      std::size_t cursor,
      const ErrorContext& context,
      const RepairPrediction& prediction) const;

  [[nodiscard]] RepairValidationResult validate_error(
      const std::vector<lexer::Token>& tokens,
      std::size_t cursor,
      const parser::ParserError& error,
      const RepairPrediction& prediction) const;

private:
  Config config_;

  bool check_operation(
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;

  bool check_position(
      const std::vector<lexer::Token>& tokens,
      std::size_t cursor,
      const ErrorContext& context,
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;

  bool check_token(
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;

  bool check_context(
      const ErrorContext& context,
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;

  bool check_structural_safety(
      const std::vector<lexer::Token>& tokens,
      std::size_t cursor,
      const ErrorContext& context,
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;

  bool check_progress(
      const std::vector<lexer::Token>& tokens,
      std::size_t cursor,
      const ErrorContext& context,
      const RepairPrediction& pred,
      ValidationCheck& check,
      std::string& err) const;
};

}  // namespace neuroparse::recovery
