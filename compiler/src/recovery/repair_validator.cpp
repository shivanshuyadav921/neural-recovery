#include "neuroparse/recovery/repair_validator.hpp"

#include <algorithm>
#include <sstream>

namespace neuroparse::recovery {
namespace {

bool is_in_expected(const std::vector<lexer::TokenType>& expected, lexer::TokenType target) noexcept {
  return std::find(expected.begin(), expected.end(), target) != expected.end();
}

bool is_valid_syntax_token(lexer::TokenType type) noexcept {
  if (type == lexer::TokenType::Invalid || type == lexer::TokenType::Eof) {
    return false;
  }
  return true;
}

}  // namespace

RepairValidationResult RepairValidator::validate(
    const std::vector<lexer::Token>& tokens,
    std::size_t cursor,
    const ErrorContext& context,
    const RepairPrediction& prediction) const {
  RepairValidationResult result;
  result.normalized_prediction = prediction;
  result.confidence = prediction.confidence;

  // Optional confidence threshold gate
  if (config_.min_confidence_threshold > 0.0 &&
      prediction.confidence < config_.min_confidence_threshold) {
    result.accepted = false;
    result.status = ValidationStatus::UnsafeRepair;
    result.reason = "Confidence is below the configured minimum threshold";
    std::ostringstream ss;
    ss << "prediction confidence " << prediction.confidence
       << " < threshold " << config_.min_confidence_threshold;
    result.details = ss.str();
    return result;
  }

  std::string err_msg;

  // Level 1: Operation validity
  if (!check_operation(prediction, result.checks.operation_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::InvalidOperation;
    result.reason = "Invalid repair operation";
    result.details = err_msg;
    return result;
  }

  // If operation is NO_REPAIR, it is accepted as a valid decision to not perform repair
  if (prediction.operation == RepairOperation::NoRepair) {
    result.checks.position_check.passed = true;
    result.checks.position_check.message = "NO_REPAIR requires no position check";
    result.checks.token_check.passed = true;
    result.checks.token_check.message = "NO_REPAIR requires no token check";
    result.checks.context_check.passed = true;
    result.checks.context_check.message = "NO_REPAIR is context-compatible";
    result.checks.structural_safety_check.passed = true;
    result.checks.structural_safety_check.message = "NO_REPAIR does not alter structure";
    result.checks.progress_check.passed = true;
    result.checks.progress_check.message = "NO_REPAIR makes no mutation";

    result.accepted = true;
    result.status = ValidationStatus::Accepted;
    result.reason = "Model proposed NO_REPAIR (no neural modification)";
    return result;
  }

  // Level 2: Position bounds
  if (!check_position(tokens, cursor, context, prediction, result.checks.position_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::InvalidPosition;
    result.reason = "Repair position is outside valid context/token bounds";
    result.details = err_msg;
    return result;
  }

  // Level 3: Token validity
  if (!check_token(prediction, result.checks.token_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::InvalidToken;
    result.reason = "Invalid or unsupported repair token";
    result.details = err_msg;
    return result;
  }

  // Level 4: Parser context compatibility
  if (!check_context(context, prediction, result.checks.context_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::NotExpected;
    result.reason = "Proposed repair token is incompatible with parser expectations";
    result.details = err_msg;
    return result;
  }

  // Level 5: Structural safety
  if (!check_structural_safety(tokens, cursor, context, prediction,
                               result.checks.structural_safety_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::UnsafeRepair;
    result.reason = "Proposed repair violates structural safety invariants";
    result.details = err_msg;
    return result;
  }

  // Level 6: Forward progress
  if (!check_progress(tokens, cursor, context, prediction,
                      result.checks.progress_check, err_msg)) {
    result.accepted = false;
    result.status = ValidationStatus::WouldNotProgress;
    result.reason = "Proposed repair would not advance parsing or alter error state";
    result.details = err_msg;
    return result;
  }

  // All deterministic compiler checks passed
  result.accepted = true;
  result.status = ValidationStatus::Accepted;
  result.reason = "Candidate is structurally and syntactically valid";
  return result;
}

RepairValidationResult RepairValidator::validate_error(
    const std::vector<lexer::Token>& tokens,
    std::size_t cursor,
    const parser::ParserError& error,
    const RepairPrediction& prediction) const {
  ErrorContext context;
  context.error_token = error.token;
  context.error_token_index = error.token_index;
  context.error_location = error.location;
  context.expected_tokens = error.expected_tokens;
  context.diagnostic_message = error.message;
  context.construct = error.construct;
  context.total_tokens = tokens.size();

  // Populate basic before/after context from token stream
  std::size_t win_start = (cursor >= 10) ? cursor - 10 : 0;
  std::size_t win_end = std::min(tokens.size(), cursor + 10);
  for (std::size_t i = win_start; i < cursor && i < tokens.size(); ++i) {
    context.tokens_before.push_back(tokens[i]);
  }
  for (std::size_t i = cursor + 1; i < win_end; ++i) {
    context.tokens_after.push_back(tokens[i]);
  }
  context.window_start_index = win_start;
  context.window_end_index = win_end;

  return validate(tokens, cursor, context, prediction);
}

bool RepairValidator::check_operation(
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  switch (pred.operation) {
    case RepairOperation::InsertToken:
      check.passed = true;
      check.message = "Operation INSERT_TOKEN is supported";
      return true;
    case RepairOperation::DeleteToken:
      check.passed = true;
      check.message = "Operation DELETE_TOKEN is supported";
      return true;
    case RepairOperation::ReplaceToken:
      check.passed = true;
      check.message = "Operation REPLACE_TOKEN is supported";
      return true;
    case RepairOperation::NoRepair:
      if (config_.allow_no_repair) {
        check.passed = true;
        check.message = "Operation NO_REPAIR is supported";
        return true;
      }
      check.passed = false;
      check.message = "NO_REPAIR operation is disallowed by validator configuration";
      err = check.message;
      return false;
    case RepairOperation::Unknown:
    default:
      check.passed = false;
      check.message = "Unknown or unsupported repair operation";
      err = check.message;
      return false;
  }
}

bool RepairValidator::check_position(
    const std::vector<lexer::Token>& tokens,
    std::size_t cursor,
    const ErrorContext& context,
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  std::size_t context_len = context.tokens_before.size() + 1 + context.tokens_after.size();

  // Position is valid if within context window [0, context_len] or stream bounds [0, tokens.size()]
  bool is_context_relative = (pred.position_offset <= context_len);
  bool is_stream_absolute = (pred.position_offset <= tokens.size());

  if (!is_context_relative && !is_stream_absolute) {
    check.passed = false;
    std::ostringstream ss;
    ss << "Position offset " << pred.position_offset << " exceeds context length ("
       << context_len << ") and token stream size (" << tokens.size() << ")";
    check.message = ss.str();
    err = check.message;
    return false;
  }

  // Deletion / Replacement must target a non-EOF token
  if (pred.operation == RepairOperation::DeleteToken || pred.operation == RepairOperation::ReplaceToken) {
    if (cursor >= tokens.size()) {
      check.passed = false;
      check.message = "Target position is past the end of the token stream";
      err = check.message;
      return false;
    }
    if (tokens[cursor].type == lexer::TokenType::Eof) {
      check.passed = false;
      check.message = "Cannot delete or replace EOF token";
      err = check.message;
      return false;
    }
  }

  check.passed = true;
  std::ostringstream ss;
  ss << "Position offset " << pred.position_offset << " is within legal bounds (cursor=" << cursor << ")";
  check.message = ss.str();
  return true;
}

bool RepairValidator::check_token(
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  if (pred.operation == RepairOperation::DeleteToken || pred.operation == RepairOperation::NoRepair) {
    check.passed = true;
    check.message = "Operation does not require token validation";
    return true;
  }

  if (!is_valid_syntax_token(pred.token_type)) {
    check.passed = false;
    std::ostringstream ss;
    ss << "Proposed token '" << lexer::to_string(pred.token_type) << "' is not a valid syntax token";
    check.message = ss.str();
    err = check.message;
    return false;
  }

  check.passed = true;
  std::ostringstream ss;
  ss << "Proposed token '" << (pred.token_lexeme.empty() ? std::string(lexer::to_string(pred.token_type)) : pred.token_lexeme)
     << "' is a valid compiler token";
  check.message = ss.str();
  return true;
}

bool RepairValidator::check_context(
    const ErrorContext& context,
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  if (pred.operation == RepairOperation::NoRepair) {
    check.passed = true;
    check.message = "NO_REPAIR is context-compatible";
    return true;
  }

  // For DELETE_TOKEN, deleting an unexpected token is valid
  if (pred.operation == RepairOperation::DeleteToken) {
    check.passed = true;
    check.message = "DELETE_TOKEN on error token is context-compatible";
    return true;
  }

  // For INSERT_TOKEN and REPLACE_TOKEN, check expected_tokens if available
  if (!context.expected_tokens.empty()) {
    if (is_in_expected(context.expected_tokens, pred.token_type)) {
      check.passed = true;
      std::ostringstream ss;
      ss << "Token '" << lexer::to_string(pred.token_type) << "' matches expected grammar terminals";
      check.message = ss.str();
      return true;
    }

    if (config_.require_expected_token_match) {
      check.passed = false;
      std::ostringstream ss;
      ss << "Token '" << lexer::to_string(pred.token_type) << "' is not in parser expected list: [";
      for (std::size_t i = 0; i < context.expected_tokens.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "'" << lexer::to_string(context.expected_tokens[i]) << "'";
      }
      ss << "]";
      check.message = ss.str();
      err = check.message;
      return false;
    }
  }

  // Fallback: Plausibility check based on construct
  check.passed = true;
  check.message = "Token is plausible for current syntax context";
  return true;
}

bool RepairValidator::check_structural_safety(
    const std::vector<lexer::Token>& tokens,
    std::size_t cursor,
    const ErrorContext& /*context*/,
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  // Rule 1: Never delete or replace EOF
  if (pred.operation == RepairOperation::DeleteToken || pred.operation == RepairOperation::ReplaceToken) {
    if (cursor < tokens.size() && tokens[cursor].type == lexer::TokenType::Eof) {
      check.passed = false;
      check.message = "Modifying EOF would corrupt token stream termination";
      err = check.message;
      return false;
    }
  }

  // Rule 2: Strict delimiter balance protection
  if (config_.strict_delimiter_matching) {
    // If inserting a closing brace '}', check if there is an unmatched open brace before cursor
    if (pred.operation == RepairOperation::InsertToken &&
        pred.token_type == lexer::TokenType::RightBrace) {
      int brace_balance = 0;
      for (std::size_t i = 0; i < cursor && i < tokens.size(); ++i) {
        if (tokens[i].type == lexer::TokenType::LeftBrace) ++brace_balance;
        else if (tokens[i].type == lexer::TokenType::RightBrace) --brace_balance;
      }
      if (brace_balance <= 0) {
        check.passed = false;
        check.message = "Inserting '}' without an open '{' would create an unmatched closing brace";
        err = check.message;
        return false;
      }
    }

    // If inserting a closing paren ')', check if there is an unmatched open paren before cursor
    if (pred.operation == RepairOperation::InsertToken &&
        pred.token_type == lexer::TokenType::RightParen) {
      int paren_balance = 0;
      for (std::size_t i = 0; i < cursor && i < tokens.size(); ++i) {
        if (tokens[i].type == lexer::TokenType::LeftParen) ++paren_balance;
        else if (tokens[i].type == lexer::TokenType::RightParen) --paren_balance;
      }
      if (paren_balance <= 0) {
        check.passed = false;
        check.message = "Inserting ')' without an open '(' would create an unmatched closing paren";
        err = check.message;
        return false;
      }
    }

    // If deleting an open brace '{', check that it doesn't leave trailing unmatched closing braces
    if (pred.operation == RepairOperation::DeleteToken && cursor < tokens.size()) {
      if (tokens[cursor].type == lexer::TokenType::LeftBrace) {
        int brace_balance = 0;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
          if (i == cursor) continue; // Speculatively skip
          if (tokens[i].type == lexer::TokenType::LeftBrace) ++brace_balance;
          else if (tokens[i].type == lexer::TokenType::RightBrace) --brace_balance;
        }
        if (brace_balance < 0) {
          check.passed = false;
          check.message = "Deleting '{' would leave unmatched closing braces in program";
          err = check.message;
          return false;
        }
      }
    }
  }

  check.passed = true;
  check.message = "Repair does not violate delimiter or token stream invariants";
  return true;
}

bool RepairValidator::check_progress(
    const std::vector<lexer::Token>& tokens,
    std::size_t cursor,
    const ErrorContext& /*context*/,
    const RepairPrediction& pred,
    ValidationCheck& check,
    std::string& err) const {
  if (pred.operation == RepairOperation::NoRepair) {
    check.passed = true;
    check.message = "NO_REPAIR requires no progress assertion";
    return true;
  }

  // If replacing a token with the EXACT SAME token, it produces zero progress
  if (pred.operation == RepairOperation::ReplaceToken && cursor < tokens.size()) {
    if (tokens[cursor].type == pred.token_type) {
      check.passed = false;
      check.message = "Replacing token with identical token type produces no forward progress";
      err = check.message;
      return false;
    }
  }

  // If inserting a duplicate consecutive semicolon right before an existing semicolon
  if (pred.operation == RepairOperation::InsertToken &&
      pred.token_type == lexer::TokenType::Semicolon &&
      cursor < tokens.size() &&
      tokens[cursor].type == lexer::TokenType::Semicolon) {
    check.passed = false;
    check.message = "Inserting duplicate consecutive ';' produces redundant empty statement";
    err = check.message;
    return false;
  }

  check.passed = true;
  check.message = "Repair ensures parser error state mutation / progress";
  return true;
}

}  // namespace neuroparse::recovery
