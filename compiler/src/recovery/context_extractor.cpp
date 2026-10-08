#include "neuroparse/recovery/context_extractor.hpp"

#include <algorithm>

namespace neuroparse::recovery {

ContextExtractor::ContextExtractor(ContextWindowConfig config)
    : config_(config) {}

ErrorContext ContextExtractor::extract(
    std::span<const lexer::Token> tokens,
    const parser::ParserError& error) const {
  ErrorContext ctx;
  ctx.config = config_;
  ctx.diagnostic_message = error.message;
  ctx.expected_tokens = error.expected_tokens;
  ctx.construct = error.construct;
  ctx.error_location = error.location;
  ctx.total_tokens = tokens.size();

  if (tokens.empty()) {
    ctx.error_token = error.token;
    ctx.error_token_index = 0;
    ctx.window_start_index = 0;
    ctx.window_end_index = 0;
    return ctx;
  }

  std::size_t error_idx = error.token_index;
  if (error_idx >= tokens.size()) {
    error_idx = tokens.size() - 1;
  }

  ctx.error_token = tokens[error_idx];
  ctx.error_token_index = error_idx;

  // Compute token window before error
  std::size_t start_idx = (error_idx >= config_.tokens_before)
                              ? (error_idx - config_.tokens_before)
                              : 0;

  // Compute token window after error
  std::size_t end_idx = (error_idx + 1 + config_.tokens_after <= tokens.size())
                            ? (error_idx + 1 + config_.tokens_after)
                            : tokens.size();

  ctx.window_start_index = start_idx;
  ctx.window_end_index = end_idx;

  // Populate tokens_before (strictly before error_idx)
  ctx.tokens_before.reserve(error_idx - start_idx);
  for (std::size_t i = start_idx; i < error_idx; ++i) {
    ctx.tokens_before.push_back(tokens[i]);
  }

  // Populate tokens_after (strictly after error_idx)
  // For errors at EOF, tokens_after must be empty
  if (ctx.error_token.type != lexer::TokenType::Eof && error_idx + 1 < end_idx) {
    ctx.tokens_after.reserve(end_idx - (error_idx + 1));
    for (std::size_t i = error_idx + 1; i < end_idx; ++i) {
      ctx.tokens_after.push_back(tokens[i]);
    }
  }

  return ctx;
}

std::vector<ErrorContext> ContextExtractor::extract_all(
    std::span<const lexer::Token> tokens,
    const std::vector<parser::ParserError>& errors) const {
  std::vector<ErrorContext> contexts;
  contexts.reserve(errors.size());
  for (const auto& err : errors) {
    contexts.push_back(extract(tokens, err));
  }
  return contexts;
}

}  // namespace neuroparse::recovery
