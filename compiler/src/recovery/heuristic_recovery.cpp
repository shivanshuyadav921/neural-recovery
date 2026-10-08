#include "neuroparse/recovery/heuristic_recovery.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace neuroparse::recovery {
namespace {

bool has_expected(const parser::ParserError& error, lexer::TokenType expected_type) noexcept {
  return std::find(error.expected_tokens.begin(), error.expected_tokens.end(), expected_type) !=
         error.expected_tokens.end();
}

}  // namespace

HeuristicRecovery::HeuristicRecovery(bool enable_panic_fallback)
    : enable_panic_fallback_(enable_panic_fallback) {}

RecoveryResult HeuristicRecovery::recover(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context) {
  auto start_time = std::chrono::steady_clock::now();

  RecoveryResult result;
  result.attempted = true;
  result.strategy = RecoveryStrategyType::Heuristic;
  result.original_token_index = cursor;
  result.error_location = (cursor < tokens.size()) ? tokens[cursor].location : error.location;

  // Try specific local deterministic heuristics
  bool recovered = try_missing_semicolon(tokens, cursor, error, result) ||
                   try_missing_closing_paren(tokens, cursor, error, result) ||
                   try_missing_closing_brace(tokens, cursor, error, result) ||
                   try_missing_comma(tokens, cursor, error, construct_context, result) ||
                   try_unexpected_extraneous_token(tokens, cursor, error, result);

  if (recovered) {
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    result.resulting_token_index = cursor;
    return result;
  }

  // If no local heuristic matched, fall back to panic mode if enabled
  if (enable_panic_fallback_) {
    result = panic_fallback_.recover(tokens, cursor, error, construct_context);
    result.strategy = RecoveryStrategyType::Heuristic;  // Keep overarching strategy tagged
    auto end_time = std::chrono::steady_clock::now();
    result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    return result;
  }

  auto end_time = std::chrono::steady_clock::now();
  result.latency_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
  result.success = false;
  result.action = RecoveryActionType::None;
  result.description = "No applicable heuristic repair found and panic fallback is disabled.";
  result.resulting_token_index = cursor;
  return result;
}

bool HeuristicRecovery::try_missing_semicolon(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    RecoveryResult& result) {
  if (!has_expected(error, lexer::TokenType::Semicolon)) {
    return false;
  }

  lexer::SourceLocation loc;
  if (cursor > 0 && cursor - 1 < tokens.size()) {
    loc = tokens[cursor - 1].location;
    loc.column += tokens[cursor - 1].length;
    loc.offset += tokens[cursor - 1].length;
  } else if (cursor < tokens.size()) {
    loc = tokens[cursor].location;
  } else {
    loc = error.location;
  }

  // Synthesize a ';' token
  lexer::Token synth_token{
      .type = lexer::TokenType::Semicolon,
      .lexeme = ";",
      .location = loc,
      .length = 1,
  };

  // Insert synthetic token at current cursor position
  if (cursor <= tokens.size()) {
    tokens.insert(tokens.begin() + cursor, synth_token);
  } else {
    tokens.push_back(synth_token);
  }

  result.success = true;
  result.action = RecoveryActionType::InsertToken;
  result.tokens_inserted.push_back(synth_token);
  result.description = "Heuristically inserted missing semicolon ';' before " +
                       (cursor + 1 < tokens.size() ? tokens[cursor + 1].lexeme : "EOF");
  return true;
}

bool HeuristicRecovery::try_missing_closing_paren(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    RecoveryResult& result) {
  if (!has_expected(error, lexer::TokenType::RightParen)) {
    return false;
  }

  lexer::SourceLocation loc;
  if (cursor > 0 && cursor - 1 < tokens.size()) {
    loc = tokens[cursor - 1].location;
    loc.column += tokens[cursor - 1].length;
    loc.offset += tokens[cursor - 1].length;
  } else if (cursor < tokens.size()) {
    loc = tokens[cursor].location;
  } else {
    loc = error.location;
  }

  // Synthesize a ')' token
  lexer::Token synth_token{
      .type = lexer::TokenType::RightParen,
      .lexeme = ")",
      .location = loc,
      .length = 1,
  };

  if (cursor <= tokens.size()) {
    tokens.insert(tokens.begin() + cursor, synth_token);
  } else {
    tokens.push_back(synth_token);
  }

  result.success = true;
  result.action = RecoveryActionType::InsertToken;
  result.tokens_inserted.push_back(synth_token);
  result.description = "Heuristically inserted missing closing parenthesis ')'.";
  return true;
}

bool HeuristicRecovery::try_missing_closing_brace(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    RecoveryResult& result) {
  if (!has_expected(error, lexer::TokenType::RightBrace)) {
    return false;
  }

  lexer::SourceLocation loc;
  if (cursor > 0 && cursor - 1 < tokens.size()) {
    loc = tokens[cursor - 1].location;
    loc.column += tokens[cursor - 1].length;
    loc.offset += tokens[cursor - 1].length;
  } else if (cursor < tokens.size()) {
    loc = tokens[cursor].location;
  } else {
    loc = error.location;
  }

  // Synthesize a '}' token
  lexer::Token synth_token{
      .type = lexer::TokenType::RightBrace,
      .lexeme = "}",
      .location = loc,
      .length = 1,
  };

  if (cursor <= tokens.size()) {
    tokens.insert(tokens.begin() + cursor, synth_token);
  } else {
    tokens.push_back(synth_token);
  }

  result.success = true;
  result.action = RecoveryActionType::InsertToken;
  result.tokens_inserted.push_back(synth_token);
  result.description = "Heuristically inserted missing closing brace '}'.";
  return true;
}

bool HeuristicRecovery::try_missing_comma(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& error,
    std::string_view construct_context,
    RecoveryResult& result) {
  bool is_list_context = construct_context.find("parameter") != std::string_view::npos ||
                         construct_context.find("call") != std::string_view::npos ||
                         construct_context.find("argument") != std::string_view::npos;

  if (!is_list_context && !has_expected(error, lexer::TokenType::Comma)) {
    return false;
  }

  // If current token is an identifier or literal (looks like next argument/parameter)
  if (cursor < tokens.size() &&
      (tokens[cursor].type == lexer::TokenType::Identifier || tokens[cursor].is_literal())) {
    lexer::SourceLocation loc = tokens[cursor].location;
    lexer::Token synth_token{
        .type = lexer::TokenType::Comma,
        .lexeme = ",",
        .location = loc,
        .length = 1,
    };

    tokens.insert(tokens.begin() + cursor, synth_token);
    result.success = true;
    result.action = RecoveryActionType::InsertToken;
    result.tokens_inserted.push_back(synth_token);
    result.description = "Heuristically inserted missing comma ',' before next argument/parameter.";
    return true;
  }

  return false;
}

bool HeuristicRecovery::try_unexpected_extraneous_token(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const parser::ParserError& /*error*/,
    RecoveryResult& result) {
  if (cursor >= tokens.size() || tokens[cursor].type == lexer::TokenType::Eof) {
    return false;
  }

  // Check for duplicate semicolon or stray delimiter
  const auto& cur_tok = tokens[cursor];
  if (cur_tok.type == lexer::TokenType::Semicolon) {
    // Redundant extra semicolon
    result.tokens_skipped.push_back(cur_tok);
    ++cursor;
    result.success = true;
    result.action = RecoveryActionType::DeleteToken;
    result.description = "Heuristically deleted redundant semicolon ';'.";
    return true;
  }

  return false;
}

}  // namespace neuroparse::recovery
