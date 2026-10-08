#include "neuroparse/recovery/panic_mode.hpp"

#include <chrono>
#include <sstream>

namespace neuroparse::recovery {

bool PanicModeRecovery::is_statement_starter(lexer::TokenType type) noexcept {
  switch (type) {
    case lexer::TokenType::KwLet:
    case lexer::TokenType::KwFunction:
    case lexer::TokenType::KwIf:
    case lexer::TokenType::KwWhile:
    case lexer::TokenType::KwReturn:
      return true;
    default:
      return false;
  }
}

bool PanicModeRecovery::is_synchronization_token(lexer::TokenType type) noexcept {
  if (is_statement_starter(type)) {
    return true;
  }
  switch (type) {
    case lexer::TokenType::Semicolon:
    case lexer::TokenType::RightBrace:
    case lexer::TokenType::Eof:
      return true;
    default:
      return false;
  }
}

RecoveryResult PanicModeRecovery::recover(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context) {
  auto start_time = std::chrono::steady_clock::now();

  RecoveryResult result;
  result.attempted = true;
  result.strategy = RecoveryStrategyType::Panic;
  result.original_token_index = cursor;
  result.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;

  if (cursor >= tokens.size() || tokens[cursor].type == lexer::TokenType::Eof) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.success = false;
    result.action = RecoveryActionType::None;
    result.description = "Already at EOF; cannot synchronize.";
    result.resulting_token_index = cursor;
    return result;
  }

  // To prevent infinite loops if the parser failed on a synchronization token itself
  // (or statement starter), advance at least one token if we're currently on a sync token.
  if (is_synchronization_token(tokens[cursor].type)) {
    result.tokens_skipped.push_back(tokens[cursor]);
    ++cursor;
  }

  // Skip tokens until we find a synchronization boundary
  while (cursor < tokens.size()) {
    const auto& tok = tokens[cursor];

    if (tok.type == lexer::TokenType::Eof) {
      break;
    }

    if (tok.type == lexer::TokenType::Semicolon) {
      // Consume the semicolon to synchronize past it
      result.tokens_skipped.push_back(tok);
      ++cursor;
      result.success = true;
      result.action = RecoveryActionType::SynchronizeStatement;
      break;
    }

    if (tok.type == lexer::TokenType::RightBrace) {
      // Do not consume closing brace so enclosing block can close
      result.success = true;
      result.action = RecoveryActionType::SynchronizeStatement;
      break;
    }

    if (is_statement_starter(tok.type)) {
      // Synchronize immediately before the next statement
      result.success = true;
      result.action = RecoveryActionType::SynchronizeStatement;
      break;
    }

    result.tokens_skipped.push_back(tok);
    ++cursor;
  }

  if (cursor < tokens.size() && tokens[cursor].type == lexer::TokenType::Eof) {
    result.success = (cursor > result.original_token_index);
    result.action = result.success ? RecoveryActionType::SkipTokens : RecoveryActionType::None;
  }

  result.resulting_token_index = cursor;

  std::ostringstream desc;
  desc << "Panic-mode synchronized on construct '" << construct_context << "'; skipped "
       << result.tokens_skipped.size() << " token(s).";
  result.description = desc.str();

  auto end_time = std::chrono::steady_clock::now();
  result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();

  return result;
}

}  // namespace neuroparse::recovery
