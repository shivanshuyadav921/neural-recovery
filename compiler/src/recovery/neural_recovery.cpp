#include "neuroparse/recovery/neural_recovery.hpp"

#include <chrono>
#include <utility>

namespace neuroparse::recovery {

std::optional<RepairPrediction> RuleAssistedInferenceEngine::predict(const ErrorContext& context) {
  // If the error token is an unexpected delimiter or unexpected operator in expression, propose DELETE_TOKEN
  if (context.error_token.type == lexer::TokenType::RightParen ||
      context.error_token.type == lexer::TokenType::RightBracket ||
      context.error_token.type == lexer::TokenType::Star ||
      context.error_token.type == lexer::TokenType::Slash ||
      context.error_token.type == lexer::TokenType::Percent) {
    RepairPrediction prediction;
    prediction.operation = RepairOperation::DeleteToken;
    prediction.token_type = context.error_token.type;
    prediction.token_lexeme = context.error_token.lexeme;
    prediction.position_offset = context.error_token_index;
    prediction.confidence = 0.95;
    return prediction;
  }

  if (context.expected_tokens.empty()) {
    return std::nullopt;
  }

  // Pick the primary expected terminal
  lexer::TokenType exp_type = context.expected_tokens.front();
  if (exp_type == lexer::TokenType::Invalid || exp_type == lexer::TokenType::Eof) {
    return std::nullopt;
  }

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = exp_type;
  prediction.token_lexeme = "";  // Will default in applier/validator
  prediction.position_offset = context.error_token_index;
  prediction.confidence = 0.95;
  prediction.operation_confidence = 0.95;
  prediction.token_confidence = 0.95;
  prediction.position_confidence = 0.95;

  return prediction;
}

NeuralRecoveryStrategy::NeuralRecoveryStrategy()
    : engine_(std::make_shared<RuleAssistedInferenceEngine>()) {}

NeuralRecoveryStrategy::NeuralRecoveryStrategy(std::shared_ptr<NeuralInferenceEngine> engine)
    : engine_(std::move(engine)) {}

NeuralRecoveryStrategy::NeuralRecoveryStrategy(
    std::shared_ptr<NeuralInferenceEngine> engine,
    RepairValidator validator,
    RepairApplier applier)
    : engine_(std::move(engine)),
      validator_(std::move(validator)),
      applier_(std::move(applier)) {}

RecoveryResult NeuralRecoveryStrategy::recover(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context) {
  auto start_time = std::chrono::steady_clock::now();

  RecoveryResult result;
  result.attempted = true;
  result.strategy = RecoveryStrategyType::Neural;
  result.original_token_index = cursor;
  result.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;

  // Step 1: Extract structured context
  ErrorContext context = extractor_.extract(tokens, error);
  if (context.construct.empty() && !construct_context.empty()) {
    context.construct = std::string(construct_context);
  }

  // Step 2: Check neural inference engine availability
  if (!engine_ || !engine_->is_ready()) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.success = false;
    result.action = RecoveryActionType::None;
    result.resulting_token_index = cursor;
    result.description = "Neural inference engine unavailable; falling back to deterministic recovery.";
    return result;
  }

  // Step 3: Propose candidate repair
  auto prediction_opt = engine_->predict(context);
  if (!prediction_opt.has_value()) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.success = false;
    result.action = RecoveryActionType::None;
    result.resulting_token_index = cursor;
    result.description = "Neural model proposed NO_REPAIR or empty candidate.";
    return result;
  }

  const auto& candidate = *prediction_opt;

  // Step 4: Validate candidate deterministically (Compiler is authoritative safety gate)
  auto validation = validator_.validate(tokens, cursor, context, candidate);
  if (!validation.accepted) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.success = false;
    result.action = RecoveryActionType::None;
    result.resulting_token_index = cursor;
    result.description = "Neural candidate rejected by RepairValidator: " + validation.reason;
    return result;
  }

  // Step 5: Apply accepted candidate via RepairApplier
  auto apply_result = applier_.apply(tokens, cursor, validation.normalized_prediction);
  if (!apply_result.applied) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.success = false;
    result.action = RecoveryActionType::None;
    result.resulting_token_index = cursor;
    result.description = "RepairApplier failed: " + apply_result.message;
    return result;
  }

  // Step 6: Successful mutation ready for parser resumption
  auto end_time = std::chrono::steady_clock::now();
  result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
  result.success = true;

  switch (validation.normalized_prediction.operation) {
    case RepairOperation::InsertToken:
      result.action = RecoveryActionType::InsertToken;
      result.tokens_inserted = apply_result.tokens_changed;
      break;
    case RepairOperation::DeleteToken:
      result.action = RecoveryActionType::DeleteToken;
      result.tokens_skipped = apply_result.tokens_changed;
      break;
    case RepairOperation::ReplaceToken:
      result.action = RecoveryActionType::ReplaceToken;
      if (apply_result.tokens_changed.size() > 1) {
        result.tokens_skipped = {apply_result.tokens_changed[0]};
        result.tokens_inserted = {apply_result.tokens_changed[1]};
      }
      break;
    default:
      result.action = RecoveryActionType::None;
      break;
  }

  result.resulting_token_index = cursor;
  result.description = "Validated neural repair applied: " + validation.normalized_prediction.format();
  return result;
}

}  // namespace neuroparse::recovery
