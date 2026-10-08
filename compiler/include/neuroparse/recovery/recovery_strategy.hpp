#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/recovery_result.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

class ErrorRecoveryStrategy {
public:
  virtual ~ErrorRecoveryStrategy() = default;

  [[nodiscard]] virtual RecoveryStrategyType strategy_type() const noexcept = 0;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;

  // Attempt recovery given the current token stream, parser cursor, and parser error.
  // The strategy may modify the token vector (e.g. inserting synthetic tokens) or
  // adjust the cursor (e.g. skipping tokens to a synchronization boundary).
  virtual RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context) = 0;
};

}  // namespace neuroparse::recovery
