#include "test_harness.hpp"

#include "neuroparse/ast/ast.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"
#include "neuroparse/recovery/error_recovery_controller.hpp"
#include "neuroparse/recovery/heuristic_recovery.hpp"
#include "neuroparse/recovery/panic_mode.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace neuroparse;

parser::ParseResult parse_source(std::string_view source, recovery::RecoveryMode mode = recovery::RecoveryMode::Auto) {
  lexer::Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  parser::Parser parser(std::move(lex_res.tokens), mode);
  return parser.parse();
}

void test_missing_semicolon_var_decl() {
  const std::string source = "let x = 42\nlet y = 10;";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 2u);
  NP_EXPECT(res.has_recovered());
  NP_EXPECT(res.metrics.recovered_errors == 1u);
  NP_EXPECT(res.recoveries.size() == 1u);
  NP_EXPECT(res.recoveries[0].action == recovery::RecoveryActionType::InsertToken);
}

void test_missing_semicolon_return() {
  const std::string source = "function f() {\n  return 42\n  let y = 1;\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 1u);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_semicolon_assignment() {
  const std::string source = "function f() {\n  x = 5\n  y = 6;\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_semicolon_expr_stmt() {
  const std::string source = "function f() {\n  foo()\n  bar();\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_closing_paren_if() {
  const std::string source = "function f() {\n  if (x > 0 {\n    return 1;\n  }\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_closing_paren_while() {
  const std::string source = "function f() {\n  while (x < 10 {\n    x = x + 1;\n  }\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_closing_paren_call() {
  const std::string source = "let x = add(1, 2;\nlet y = 3;";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 2u);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_closing_paren_group() {
  const std::string source = "let x = (1 + 2 * 3;\nlet y = 4;";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 2u);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_closing_brace_function() {
  const std::string source = "function f() {\n  return 1;\n";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 1u);
  NP_EXPECT(res.metrics.recovered_errors == 1u);
}

void test_missing_comma_params() {
  const std::string source = "function add(a b) {\n  return a + b;\n}";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 1u);
  auto* fn = dynamic_cast<const ast::FunctionDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  if (fn) {
    NP_EXPECT(fn->parameters().size() == 2u);
  }
}

void test_missing_comma_call_args() {
  const std::string source = "let x = add(1 2);\nlet y = 3;";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 1u);
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 2u);
}

void test_unexpected_token_extraneous_semicolon() {
  const std::string source = "let x = 42;;\nlet y = 10;";
  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 2u);
}

void test_panic_mode_recovery() {
  const std::string source = "let x = + ;\nlet y = 10;";
  auto res = parse_source(source, recovery::RecoveryMode::Panic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
  // Panic mode skips to next statement 'let y = 10;'
  NP_EXPECT(res.program->declarations().size() == 1u);
  if (!res.program->declarations().empty()) {
    auto* var_decl = dynamic_cast<const ast::VariableDeclaration*>(res.program->declarations()[0].get());
    NP_EXPECT(var_decl != nullptr);
    if (var_decl) {
      NP_EXPECT(var_decl->name() == "y");
    }
  }
}

void test_multiple_syntax_errors() {
  const std::string source =
      "let a = 1\n"
      "function f(x y) {\n"
      "  if (x > 0 {\n"
      "    return x\n"
      "  }\n"
      "  return y;\n"
      "}\n"
      "let b = 2\n";

  auto res = parse_source(source, recovery::RecoveryMode::Heuristic);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 3u);
  NP_EXPECT(res.metrics.total_errors >= 3u);
  NP_EXPECT(res.metrics.recovered_errors >= 3u);
  NP_EXPECT(res.metrics.recovery_rate() > 0.8);
}

void test_error_near_eof() {
  const std::string source = "let x =";
  auto res = parse_source(source, recovery::RecoveryMode::Auto);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
}

void test_nested_errors() {
  const std::string source =
      "function outer() {\n"
      "  if (true) {\n"
      "    while (true {\n"
      "      let x = 1\n"
      "    }\n"
      "  }\n"
      "  return 0;\n"
      "}\n";

  auto res = parse_source(source, recovery::RecoveryMode::Auto);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 1u);
}

void test_recovery_disabled_mode() {
  const std::string source = "let x = 1\nlet y = 2;";
  auto res = parse_source(source, recovery::RecoveryMode::None);

  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
  // Without recovery, stops on first error, parses 0 declarations
  NP_EXPECT(res.program->declarations().size() == 0u);
  NP_EXPECT(res.metrics.recovered_errors == 0u);
}

void test_recovery_failure_garbage() {
  const std::string source = "% & ^ $ @ ! #";
  auto res = parse_source(source, recovery::RecoveryMode::Auto);

  // Must not crash or hang
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
}

void test_metrics_and_latency() {
  const std::string source = "let x = 1\nlet y = 2\nlet z = 3;";
  auto res = parse_source(source, recovery::RecoveryMode::Auto);

  NP_EXPECT(res.metrics.total_errors == 2u);
  NP_EXPECT(res.metrics.recovered_errors == 2u);
  NP_EXPECT(res.metrics.failed_recoveries == 0u);
  NP_EXPECT(res.metrics.recovery_rate() == 1.0);
  NP_EXPECT(res.metrics.total_latency_us >= 0.0);
}

// ============================================================
// PHASE 6: ErrorRecoveryController Specific Tests
// ============================================================

// Mock failing strategy to test controller chaining & rollback
class FailingMockStrategy : public recovery::ErrorRecoveryStrategy {
public:
  [[nodiscard]] recovery::RecoveryStrategyType strategy_type() const noexcept override {
    return recovery::RecoveryStrategyType::None;
  }
  [[nodiscard]] std::string_view name() const noexcept override {
    return "failing_mock";
  }
  recovery::RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view /*construct_context*/) override {
    // Mutate token vector and cursor to test rollback
    cursor += 1;
    tokens.push_back(lexer::Token{
        .type = lexer::TokenType::Invalid,
        .lexeme = "GARBAGE",
        .location = error.location,
        .length = 7,
    });
    recovery::RecoveryResult res;
    res.attempted = true;
    res.success = false;
    res.description = "Intentional mock failure.";
    return res;
  }
};

void test_controller_strategy_pipeline_and_names() {
  recovery::ErrorRecoveryController controller(recovery::RecoveryMode::Auto);
  NP_EXPECT(controller.is_recovery_enabled());
  NP_EXPECT(controller.strategy_count() == 3u);

  auto names = controller.strategy_names();
  NP_EXPECT(names.size() == 3u);
  NP_EXPECT(names[0] == "neural");
  NP_EXPECT(names[1] == "heuristic");
  NP_EXPECT(names[2] == "panic_mode");

  controller.clear_strategies();
  NP_EXPECT(controller.strategy_count() == 0u);
  NP_EXPECT(!controller.is_recovery_enabled());

  controller.add_strategy(std::make_unique<recovery::PanicModeRecovery>());
  NP_EXPECT(controller.strategy_count() == 1u);
  NP_EXPECT(controller.strategy_names()[0] == "panic_mode");
}

void test_controller_rollback_on_strategy_failure() {
  // Chain: [FailingMockStrategy, PanicModeRecovery]
  std::vector<std::unique_ptr<recovery::ErrorRecoveryStrategy>> chain;
  chain.push_back(std::make_unique<FailingMockStrategy>());
  chain.push_back(std::make_unique<recovery::PanicModeRecovery>());

  auto controller = std::make_shared<recovery::ErrorRecoveryController>(std::move(chain));

  const std::string source = "let x = + ;\nlet y = 10;";
  lexer::Lexer lexer(source);
  auto lex_res = lexer.tokenize();

  parser::Parser parser(std::move(lex_res.tokens), controller);
  auto res = parser.parse();

  // FailingMockStrategy failed and was rolled back; PanicModeRecovery then succeeded
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().size() == 1u);

  const auto& telemetry = controller->strategy_telemetry();
  NP_EXPECT(telemetry.count("failing_mock") == 1u);
  NP_EXPECT(telemetry.at("failing_mock").attempts == 1u);
  NP_EXPECT(telemetry.at("failing_mock").failures == 1u);
  NP_EXPECT(telemetry.at("failing_mock").successes == 0u);

  NP_EXPECT(telemetry.count("panic_mode") == 1u);
  NP_EXPECT(telemetry.at("panic_mode").attempts == 1u);
  NP_EXPECT(telemetry.at("panic_mode").successes == 1u);
}

void test_controller_telemetry_metrics() {
  recovery::ErrorRecoveryController controller(recovery::RecoveryMode::Auto);

  const std::string source = "let a = 1\nlet b = + ;\nlet c = 3;";
  lexer::Lexer lexer(source);
  auto lex_res = lexer.tokenize();

  parser::Parser parser(std::move(lex_res.tokens), std::make_shared<recovery::ErrorRecoveryController>(recovery::RecoveryMode::Auto));
  auto res = parser.parse();

  NP_EXPECT(res.metrics.total_errors >= 2u);
  NP_EXPECT(res.metrics.recovered_errors >= 2u);
}

}  // namespace

void run_recovery_tests() {
  test_missing_semicolon_var_decl();
  test_missing_semicolon_return();
  test_missing_semicolon_assignment();
  test_missing_semicolon_expr_stmt();
  test_missing_closing_paren_if();
  test_missing_closing_paren_while();
  test_missing_closing_paren_call();
  test_missing_closing_paren_group();
  test_missing_closing_brace_function();
  test_missing_comma_params();
  test_missing_comma_call_args();
  test_unexpected_token_extraneous_semicolon();
  test_panic_mode_recovery();
  test_multiple_syntax_errors();
  test_error_near_eof();
  test_nested_errors();
  test_recovery_disabled_mode();
  test_recovery_failure_garbage();
  test_metrics_and_latency();

  // Phase 6 tests
  test_controller_strategy_pipeline_and_names();
  test_controller_rollback_on_strategy_failure();
  test_controller_telemetry_metrics();
}
