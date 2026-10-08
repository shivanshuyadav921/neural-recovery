#pragma once

#include "neuroparse/recovery/panic_mode.hpp"
#include "neuroparse/recovery/recovery_strategy.hpp"

namespace neuroparse::recovery {

class HeuristicRecovery final : public ErrorRecoveryStrategy {
public:
  explicit HeuristicRecovery(bool enable_panic_fallback = true);
  ~HeuristicRecovery() override = default;

  [[nodiscard]] RecoveryStrategyType strategy_type() const noexcept override {
    return RecoveryStrategyType::Heuristic;
  }

  [[nodiscard]] std::string_view name() const noexcept override {
    return "heuristic";
  }

  RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context) override;

  [[nodiscard]] bool panic_fallback_enabled() const noexcept { return enable_panic_fallback_; }
  void set_panic_fallback(bool enable) noexcept { enable_panic_fallback_ = enable; }

private:
  bool try_missing_semicolon(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      RecoveryResult& result);

  bool try_missing_closing_paren(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      RecoveryResult& result);

  bool try_missing_closing_brace(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      RecoveryResult& result);

  bool try_missing_comma(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context,
      RecoveryResult& result);

  bool try_unexpected_extraneous_token(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      RecoveryResult& result);

  bool enable_panic_fallback_ = true;
  PanicModeRecovery panic_fallback_;
};

}  // namespace neuroparse::recovery
