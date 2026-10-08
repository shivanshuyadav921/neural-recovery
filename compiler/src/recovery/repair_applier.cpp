#include "neuroparse/recovery/repair_applier.hpp"

#include <string>

namespace neuroparse::recovery {
namespace {

std::string default_lexeme_for_token(lexer::TokenType type) {
  switch (type) {
    case lexer::TokenType::Semicolon:    return ";";
    case lexer::TokenType::LeftParen:    return "(";
    case lexer::TokenType::RightParen:   return ")";
    case lexer::TokenType::LeftBrace:    return "{";
    case lexer::TokenType::RightBrace:   return "}";
    case lexer::TokenType::LeftBracket:  return "[";
    case lexer::TokenType::RightBracket: return "]";
    case lexer::TokenType::Comma:        return ",";
    case lexer::TokenType::Colon:        return ":";
    case lexer::TokenType::Dot:          return ".";
    case lexer::TokenType::Equal:        return "=";
    case lexer::TokenType::EqualEqual:   return "==";
    case lexer::TokenType::BangEqual:    return "!=";
    case lexer::TokenType::Less:         return "<";
    case lexer::TokenType::LessEqual:    return "<=";
    case lexer::TokenType::Greater:      return ">";
    case lexer::TokenType::GreaterEqual: return ">=";
    case lexer::TokenType::Plus:         return "+";
    case lexer::TokenType::Minus:        return "-";
    case lexer::TokenType::Star:         return "*";
    case lexer::TokenType::Slash:        return "/";
    case lexer::TokenType::Percent:      return "%";
    case lexer::TokenType::Bang:         return "!";
    case lexer::TokenType::AmpAmp:       return "&&";
    case lexer::TokenType::PipePipe:     return "||";
    case lexer::TokenType::KwFunction:   return "function";
    case lexer::TokenType::KwLet:        return "let";
    case lexer::TokenType::KwReturn:     return "return";
    case lexer::TokenType::KwIf:         return "if";
    case lexer::TokenType::KwElse:       return "else";
    case lexer::TokenType::KwWhile:      return "while";
    case lexer::TokenType::KwTrue:       return "true";
    case lexer::TokenType::KwFalse:      return "false";
    case lexer::TokenType::Identifier:   return "synthetic_id";
    default:                             return "";
  }
}

}  // namespace

RepairApplyResult RepairApplier::apply(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const RepairPrediction& prediction) const {
  switch (prediction.operation) {
    case RepairOperation::InsertToken:
      return apply_insert(tokens, cursor, prediction);
    case RepairOperation::DeleteToken:
      return apply_delete(tokens, cursor, prediction);
    case RepairOperation::ReplaceToken:
      return apply_replace(tokens, cursor, prediction);
    case RepairOperation::NoRepair:
      return apply_no_repair(tokens, cursor, prediction);
    case RepairOperation::Unknown:
    default: {
      RepairApplyResult res;
      res.applied = false;
      res.status = RepairApplyStatus::InvalidState;
      res.operation = prediction.operation;
      res.position = cursor;
      res.new_parser_position = cursor;
      res.message = "Cannot apply unknown or unsupported repair operation.";
      return res;
    }
  }
}

RepairApplyResult RepairApplier::apply_insert(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const RepairPrediction& prediction) const {
  RepairApplyResult result;
  result.operation = RepairOperation::InsertToken;

  if (prediction.token_type == lexer::TokenType::Invalid ||
      prediction.token_type == lexer::TokenType::Eof) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = cursor;
    result.new_parser_position = cursor;
    result.message = "Cannot insert Invalid or EOF token.";
    return result;
  }

  std::size_t target_pos = prediction.position_offset;
  if (target_pos > tokens.size()) {
    target_pos = tokens.size();
  }

  // Anchor source location to context
  lexer::SourceLocation loc;
  if (target_pos > 0 && target_pos - 1 < tokens.size()) {
    loc = tokens[target_pos - 1].location;
    loc.column += tokens[target_pos - 1].length;
    loc.offset += tokens[target_pos - 1].length;
  } else if (target_pos < tokens.size()) {
    loc = tokens[target_pos].location;
  } else {
    loc = lexer::SourceLocation{.offset = 0, .line = 1, .column = 1};
  }

  std::string lexeme = prediction.token_lexeme;
  if (lexeme.empty()) {
    lexeme = default_lexeme_for_token(prediction.token_type);
  }

  lexer::Token synth_token{
      .type = prediction.token_type,
      .lexeme = lexeme,
      .location = loc,
      .length = lexeme.size(),
  };

  if (target_pos <= tokens.size()) {
    tokens.insert(tokens.begin() + target_pos, synth_token);
  } else {
    tokens.push_back(synth_token);
  }

  // When inserting at or before cursor, cursor now points to target_pos
  cursor = target_pos;

  result.applied = true;
  result.status = RepairApplyStatus::Applied;
  result.position = target_pos;
  result.new_parser_position = cursor;
  result.tokens_changed = {synth_token};
  result.message = "Successfully inserted synthetic token '" + lexeme + "'";
  return result;
}

RepairApplyResult RepairApplier::apply_delete(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const RepairPrediction& prediction) const {
  RepairApplyResult result;
  result.operation = RepairOperation::DeleteToken;

  std::size_t target_pos = prediction.position_offset;
  if (target_pos >= tokens.size()) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = cursor;
    result.new_parser_position = cursor;
    result.message = "Delete position out of bounds.";
    return result;
  }

  if (tokens[target_pos].type == lexer::TokenType::Eof) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = target_pos;
    result.new_parser_position = cursor;
    result.message = "Cannot delete EOF token.";
    return result;
  }

  lexer::Token deleted_tok = tokens[target_pos];
  tokens.erase(tokens.begin() + target_pos);

  // If we deleted before cursor, decrement cursor; if deleted at cursor, cursor now indexes next token
  if (cursor > target_pos) {
    --cursor;
  }
  if (cursor >= tokens.size()) {
    cursor = tokens.empty() ? 0 : tokens.size() - 1;
  }

  result.applied = true;
  result.status = RepairApplyStatus::Applied;
  result.position = target_pos;
  result.new_parser_position = cursor;
  result.tokens_changed = {deleted_tok};
  result.message = "Successfully deleted token '" + deleted_tok.lexeme + "'";
  return result;
}

RepairApplyResult RepairApplier::apply_replace(
    std::vector<lexer::Token>& tokens,
    std::size_t& cursor,
    const RepairPrediction& prediction) const {
  RepairApplyResult result;
  result.operation = RepairOperation::ReplaceToken;

  std::size_t target_pos = prediction.position_offset;
  if (target_pos >= tokens.size()) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = cursor;
    result.new_parser_position = cursor;
    result.message = "Replace position out of bounds.";
    return result;
  }

  if (tokens[target_pos].type == lexer::TokenType::Eof) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = target_pos;
    result.new_parser_position = cursor;
    result.message = "Cannot replace EOF token.";
    return result;
  }

  if (prediction.token_type == lexer::TokenType::Invalid ||
      prediction.token_type == lexer::TokenType::Eof) {
    result.applied = false;
    result.status = RepairApplyStatus::InvalidState;
    result.position = target_pos;
    result.new_parser_position = cursor;
    result.message = "Cannot replace with Invalid or EOF token.";
    return result;
  }

  lexer::Token old_token = tokens[target_pos];

  std::string lexeme = prediction.token_lexeme;
  if (lexeme.empty()) {
    lexeme = default_lexeme_for_token(prediction.token_type);
  }

  lexer::Token synth_token{
      .type = prediction.token_type,
      .lexeme = lexeme,
      .location = old_token.location,
      .length = lexeme.size(),
  };

  tokens[target_pos] = synth_token;
  cursor = target_pos;

  result.applied = true;
  result.status = RepairApplyStatus::Applied;
  result.position = target_pos;
  result.new_parser_position = cursor;
  result.tokens_changed = {old_token, synth_token};
  result.message = "Successfully replaced token '" + old_token.lexeme + "' with '" + lexeme + "'";
  return result;
}

RepairApplyResult RepairApplier::apply_no_repair(
    std::vector<lexer::Token>& /*tokens*/,
    std::size_t& cursor,
    const RepairPrediction& prediction) const {
  RepairApplyResult result;
  result.applied = false;
  result.status = RepairApplyStatus::NoRepair;
  result.operation = RepairOperation::NoRepair;
  result.position = prediction.position_offset;
  result.new_parser_position = cursor;
  result.message = "NO_REPAIR operation requested; stream left unmodified.";
  return result;
}

}  // namespace neuroparse::recovery
