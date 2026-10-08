#pragma once

#include "neuroparse/lexer/token.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace neuroparse::lexer {

struct LexerError {
  std::string message;
  SourceLocation location;
  std::size_t length = 1;

  [[nodiscard]] std::size_t offset() const noexcept { return location.offset; }
  [[nodiscard]] std::size_t line() const noexcept { return location.line; }
  [[nodiscard]] std::size_t column() const noexcept { return location.column; }

  [[nodiscard]] std::string format(std::string_view filename = "") const {
    std::ostringstream ss;
    if (!filename.empty()) {
      ss << filename << ":";
    }
    ss << line() << ":" << column() << ": error: " << message;
    return ss.str();
  }

  bool operator==(const LexerError&) const = default;
};

}  // namespace neuroparse::lexer
