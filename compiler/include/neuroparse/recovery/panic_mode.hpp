#pragma once

#include "neuroparse/recovery/recovery_strategy.hpp"

namespace neuroparse::recovery {

class PanicModeRecovery final : public ErrorRecoveryStrategy {
public:
  PanicModeRecovery() = default;
  ~PanicModeRecovery() override = default;

  [[nodiscard]] RecoveryStrategyType strategy_type() const noexcept override {
    return RecoveryStrategyType::Panic;
  }

  [[nodiscard]] std::string_view name() const noexcept override {
    return "panic_mode";
  }

  RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context) override;

  [[nodiscard]] static bool is_synchronization_token(lexer::TokenType type) noexcept;
  [[nodiscard]] static bool is_statement_starter(lexer::TokenType type) noexcept;
};

}  // namespace neuroparse::recovery
