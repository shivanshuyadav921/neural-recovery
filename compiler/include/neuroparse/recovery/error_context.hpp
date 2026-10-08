#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/recovery/context_window_config.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

struct ErrorContext {
  std::vector<lexer::Token> tokens_before;
  lexer::Token error_token;
  std::vector<lexer::Token> tokens_after;

  std::size_t error_token_index = 0;
  std::size_t total_tokens = 0;
  lexer::SourceLocation error_location;
  std::vector<lexer::TokenType> expected_tokens;
  std::string diagnostic_message;
  std::string construct;

  // Exact index range [start_index, end_index) in original token stream
  std::size_t window_start_index = 0;
  std::size_t window_end_index = 0;
  ContextWindowConfig config;

  [[nodiscard]] bool is_eof_error() const noexcept {
    return error_token.type == lexer::TokenType::Eof;
  }

  [[nodiscard]] std::string format(std::string_view filename = "") const;

  bool operator==(const ErrorContext&) const = default;
};

}  // namespace neuroparse::recovery
