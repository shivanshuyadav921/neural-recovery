#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/context_window_config.hpp"
#include "neuroparse/recovery/error_context.hpp"

#include <span>
#include <vector>

namespace neuroparse::recovery {

class ContextExtractor {
public:
  explicit ContextExtractor(ContextWindowConfig config = ContextWindowConfig{});

  [[nodiscard]] const ContextWindowConfig& config() const noexcept { return config_; }
  void set_config(ContextWindowConfig config) noexcept { config_ = config; }

  // Extract ErrorContext for a single ParserError
  [[nodiscard]] ErrorContext extract(
      std::span<const lexer::Token> tokens,
      const parser::ParserError& error) const;

  [[nodiscard]] ErrorContext extract(
      const std::vector<lexer::Token>& tokens,
      const parser::ParserError& error) const {
    return extract(std::span<const lexer::Token>(tokens.data(), tokens.size()), error);
  }

  // Extract ErrorContext for all errors in a parse result
  [[nodiscard]] std::vector<ErrorContext> extract_all(
      std::span<const lexer::Token> tokens,
      const std::vector<parser::ParserError>& errors) const;

  [[nodiscard]] std::vector<ErrorContext> extract_all(
      const std::vector<lexer::Token>& tokens,
      const std::vector<parser::ParserError>& errors) const {
    return extract_all(std::span<const lexer::Token>(tokens.data(), tokens.size()), errors);
  }

private:
  ContextWindowConfig config_;
};

}  // namespace neuroparse::recovery
