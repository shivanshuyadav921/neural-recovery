#pragma once

#include "neuroparse/lexer/lexer_error.hpp"
#include "neuroparse/lexer/token.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace neuroparse::lexer {

struct LexResult {
  std::vector<Token> tokens;
  std::vector<LexerError> errors;

  [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
  [[nodiscard]] bool has_errors() const noexcept { return !errors.empty(); }
};

class Lexer {
public:
  explicit Lexer(std::string_view source);

  // Tokenize the entire source text and return the token stream along with any lexical errors.
  [[nodiscard]] LexResult tokenize();

  // Scan and return the next token in the stream.
  Token next_token();

  // Keyword lookup helper
  [[nodiscard]] static std::optional<TokenType> lookup_keyword(std::string_view ident) noexcept;

  // Accessors
  [[nodiscard]] const std::vector<LexerError>& errors() const noexcept { return errors_; }
  [[nodiscard]] bool has_errors() const noexcept { return !errors_.empty(); }
  [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
  [[nodiscard]] std::size_t line() const noexcept { return line_; }
  [[nodiscard]] std::size_t column() const noexcept { return column_; }

private:
  [[nodiscard]] bool is_at_end() const noexcept;
  [[nodiscard]] char peek() const noexcept;
  [[nodiscard]] char peek_next() const noexcept;
  char advance() noexcept;
  bool match(char expected) noexcept;

  void skip_whitespace_and_comments();
  void add_error(std::string message, std::size_t start_offset, std::size_t start_line, std::size_t start_col, std::size_t len);

  Token make_token(TokenType type, std::size_t start_offset, std::size_t start_line, std::size_t start_col);
  Token scan_identifier_or_keyword(std::size_t start_offset, std::size_t start_line, std::size_t start_col);
  Token scan_number(std::size_t start_offset, std::size_t start_line, std::size_t start_col);
  Token scan_string(std::size_t start_offset, std::size_t start_line, std::size_t start_col);

  std::string_view source_;
  std::size_t cursor_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  std::vector<LexerError> errors_;
};

}  // namespace neuroparse::lexer
