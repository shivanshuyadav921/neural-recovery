#pragma once

#include "neuroparse/lexer/token.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::parser {

struct ParserError {
  std::string message;
  lexer::Token token;
  std::size_t token_index = 0;
  lexer::SourceLocation location;
  std::vector<lexer::TokenType> expected_tokens;
  std::string construct;

  [[nodiscard]] std::size_t offset() const noexcept { return location.offset; }
  [[nodiscard]] std::size_t line() const noexcept { return location.line; }
  [[nodiscard]] std::size_t column() const noexcept { return location.column; }

  [[nodiscard]] std::string format(std::string_view filename = "") const {
    std::ostringstream ss;
    if (!filename.empty()) {
      ss << filename << ":";
    }
    ss << line() << ":" << column() << ": syntax error: " << message;
    if (!expected_tokens.empty()) {
      ss << " (expected: ";
      for (std::size_t i = 0; i < expected_tokens.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "'" << lexer::to_string(expected_tokens[i]) << "'";
      }
      ss << ")";
    }
    return ss.str();
  }

  bool operator==(const ParserError&) const = default;
};

}  // namespace neuroparse::parser
