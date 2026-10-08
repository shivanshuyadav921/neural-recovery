#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace neuroparse::lexer {

enum class TokenType {
  // End of input / errors
  Eof,
  Invalid,

  // Identifiers & Literals
  Identifier,
  IntegerLiteral,
  FloatLiteral,
  StringLiteral,

  // Keywords
  KwFunction,
  KwLet,
  KwReturn,
  KwIf,
  KwElse,
  KwWhile,
  KwTrue,
  KwFalse,

  // Operators
  Plus,          // +
  Minus,         // -
  Star,          // *
  Slash,         // /
  Percent,       // %
  Equal,         // =
  EqualEqual,    // ==
  BangEqual,     // !=
  Less,          // <
  LessEqual,     // <=
  Greater,       // >
  GreaterEqual,  // >=
  Bang,          // !
  AmpAmp,        // &&
  PipePipe,      // ||

  // Delimiters
  LeftParen,     // (
  RightParen,    // )
  LeftBrace,     // {
  RightBrace,    // }
  LeftBracket,   // [
  RightBracket,  // ]

  // Punctuation
  Comma,         // ,
  Semicolon,     // ;
  Colon,         // :
  Dot,           // .
};

enum class TokenCategory {
  Identifier,
  Integer,
  Float,
  String,
  Keyword,
  Operator,
  Delimiter,
  Punctuation,
  Eof,
  Invalid,
};

constexpr TokenCategory token_category(TokenType type) noexcept {
  switch (type) {
    case TokenType::Identifier:
      return TokenCategory::Identifier;
    case TokenType::IntegerLiteral:
      return TokenCategory::Integer;
    case TokenType::FloatLiteral:
      return TokenCategory::Float;
    case TokenType::StringLiteral:
      return TokenCategory::String;

    case TokenType::KwFunction:
    case TokenType::KwLet:
    case TokenType::KwReturn:
    case TokenType::KwIf:
    case TokenType::KwElse:
    case TokenType::KwWhile:
    case TokenType::KwTrue:
    case TokenType::KwFalse:
      return TokenCategory::Keyword;

    case TokenType::Plus:
    case TokenType::Minus:
    case TokenType::Star:
    case TokenType::Slash:
    case TokenType::Percent:
    case TokenType::Equal:
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
    case TokenType::Less:
    case TokenType::LessEqual:
    case TokenType::Greater:
    case TokenType::GreaterEqual:
    case TokenType::Bang:
    case TokenType::AmpAmp:
    case TokenType::PipePipe:
      return TokenCategory::Operator;

    case TokenType::LeftParen:
    case TokenType::RightParen:
    case TokenType::LeftBrace:
    case TokenType::RightBrace:
    case TokenType::LeftBracket:
    case TokenType::RightBracket:
      return TokenCategory::Delimiter;

    case TokenType::Comma:
    case TokenType::Semicolon:
    case TokenType::Colon:
    case TokenType::Dot:
      return TokenCategory::Punctuation;

    case TokenType::Eof:
      return TokenCategory::Eof;

    case TokenType::Invalid:
    default:
      return TokenCategory::Invalid;
  }
}

constexpr std::string_view to_string(TokenType type) noexcept {
  switch (type) {
    case TokenType::Eof:          return "EOF";
    case TokenType::Invalid:      return "INVALID";
    case TokenType::Identifier:   return "IDENTIFIER";
    case TokenType::IntegerLiteral: return "INTEGER";
    case TokenType::FloatLiteral: return "FLOAT";
    case TokenType::StringLiteral: return "STRING";
    case TokenType::KwFunction:   return "function";
    case TokenType::KwLet:        return "let";
    case TokenType::KwReturn:     return "return";
    case TokenType::KwIf:         return "if";
    case TokenType::KwElse:       return "else";
    case TokenType::KwWhile:      return "while";
    case TokenType::KwTrue:       return "true";
    case TokenType::KwFalse:      return "false";
    case TokenType::Plus:         return "+";
    case TokenType::Minus:        return "-";
    case TokenType::Star:         return "*";
    case TokenType::Slash:        return "/";
    case TokenType::Percent:      return "%";
    case TokenType::Equal:        return "=";
    case TokenType::EqualEqual:   return "==";
    case TokenType::BangEqual:    return "!=";
    case TokenType::Less:         return "<";
    case TokenType::LessEqual:    return "<=";
    case TokenType::Greater:      return ">";
    case TokenType::GreaterEqual: return ">=";
    case TokenType::Bang:         return "!";
    case TokenType::AmpAmp:       return "&&";
    case TokenType::PipePipe:     return "||";
    case TokenType::LeftParen:    return "(";
    case TokenType::RightParen:   return ")";
    case TokenType::LeftBrace:    return "{";
    case TokenType::RightBrace:   return "}";
    case TokenType::LeftBracket:  return "[";
    case TokenType::RightBracket: return "]";
    case TokenType::Comma:        return ",";
    case TokenType::Semicolon:    return ";";
    case TokenType::Colon:        return ":";
    case TokenType::Dot:          return ".";
    default:                      return "UNKNOWN";
  }
}

constexpr std::string_view to_category_string(TokenCategory cat) noexcept {
  switch (cat) {
    case TokenCategory::Identifier:   return "IDENTIFIER";
    case TokenCategory::Integer:      return "INTEGER";
    case TokenCategory::Float:        return "FLOAT";
    case TokenCategory::String:       return "STRING";
    case TokenCategory::Keyword:      return "KEYWORD";
    case TokenCategory::Operator:     return "OPERATOR";
    case TokenCategory::Delimiter:    return "DELIMITER";
    case TokenCategory::Punctuation:  return "PUNCTUATION";
    case TokenCategory::Eof:          return "EOF";
    case TokenCategory::Invalid:      return "INVALID";
    default:                          return "UNKNOWN";
  }
}

struct SourceLocation {
  std::size_t offset = 0;
  std::size_t line = 1;
  std::size_t column = 1;

  constexpr bool operator==(const SourceLocation&) const = default;
};

struct Token {
  TokenType type = TokenType::Invalid;
  std::string lexeme;
  SourceLocation location;
  std::size_t length = 0;

  [[nodiscard]] std::size_t offset() const noexcept { return location.offset; }
  [[nodiscard]] std::size_t line() const noexcept { return location.line; }
  [[nodiscard]] std::size_t column() const noexcept { return location.column; }
  [[nodiscard]] TokenCategory category() const noexcept { return token_category(type); }

  [[nodiscard]] bool is(TokenType t) const noexcept { return type == t; }
  [[nodiscard]] bool is_keyword() const noexcept { return category() == TokenCategory::Keyword; }
  [[nodiscard]] bool is_operator() const noexcept { return category() == TokenCategory::Operator; }
  [[nodiscard]] bool is_delimiter() const noexcept { return category() == TokenCategory::Delimiter; }
  [[nodiscard]] bool is_punctuation() const noexcept { return category() == TokenCategory::Punctuation; }
  [[nodiscard]] bool is_literal() const noexcept {
    return type == TokenType::IntegerLiteral ||
           type == TokenType::FloatLiteral ||
           type == TokenType::StringLiteral ||
           type == TokenType::KwTrue ||
           type == TokenType::KwFalse;
  }

  bool operator==(const Token&) const = default;
};

}  // namespace neuroparse::lexer
