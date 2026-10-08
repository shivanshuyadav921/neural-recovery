#pragma once

#include "neuroparse/lexer/token.hpp"

#include <cstddef>

namespace neuroparse::ast {

struct SourceSpan {
  lexer::SourceLocation start;
  lexer::SourceLocation end;

  [[nodiscard]] std::size_t line() const noexcept { return start.line; }
  [[nodiscard]] std::size_t column() const noexcept { return start.column; }
  [[nodiscard]] std::size_t offset() const noexcept { return start.offset; }
  [[nodiscard]] std::size_t length() const noexcept {
    if (end.offset >= start.offset) {
      return end.offset - start.offset;
    }
    return 0;
  }

  [[nodiscard]] static SourceSpan from_location(const lexer::SourceLocation& loc, std::size_t len = 0) noexcept {
    return SourceSpan{
        .start = loc,
        .end = lexer::SourceLocation{
            .offset = loc.offset + len,
            .line = loc.line,
            .column = loc.column + len,
        },
    };
  }

  [[nodiscard]] static SourceSpan from_token(const lexer::Token& tok) noexcept {
    return from_location(tok.location, tok.length);
  }

  [[nodiscard]] static SourceSpan cover(const SourceSpan& a, const SourceSpan& b) noexcept {
    SourceSpan result;
    result.start = (a.start.offset <= b.start.offset) ? a.start : b.start;
    result.end = (a.end.offset >= b.end.offset) ? a.end : b.end;
    return result;
  }

  constexpr bool operator==(const SourceSpan&) const = default;
};

}  // namespace neuroparse::ast
