#include "neuroparse/ast/ast_printer.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"
#include "neuroparse/recovery/error_recovery_controller.hpp"
#include "neuroparse/recovery/neural_recovery.hpp"
#include "test_harness.hpp"

#include <string>
#include <vector>

namespace {

using namespace neuroparse;
using namespace neuroparse::recovery;
using namespace neuroparse::lexer;
using namespace neuroparse::parser;

#define ASSERT_TRUE(c) NP_EXPECT(c)
#define ASSERT_FALSE(c) NP_EXPECT(!(c))
#define ASSERT_EQ(a, b) NP_EXPECT((a) == (b))

void test_neural_recovery_missing_semicolon() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  Parser parser(std::move(lex_res.tokens), RecoveryMode::Neural);
  auto parse_res = parser.parse();

  // Neural recovery should have recovered the missing semicolon
  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_EQ(parse_res.errors.size(), 1);
  ASSERT_EQ(parse_res.metrics.recovered_errors, 1);
  ASSERT_EQ(parse_res.program->declarations().size(), 2);

  const auto& rec = parse_res.recoveries.front();
  ASSERT_TRUE(rec.success);
  ASSERT_EQ(rec.strategy, RecoveryStrategyType::Neural);
  ASSERT_EQ(rec.action, RecoveryActionType::InsertToken);
}

void test_neural_recovery_missing_closing_paren() {
  std::string source = "function test() {\n  let x = add(1, 2;\n  return x;\n}";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  Parser parser(std::move(lex_res.tokens), RecoveryMode::Neural);
  auto parse_res = parser.parse();

  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_TRUE(parse_res.has_recovered());
  ASSERT_EQ(parse_res.program->declarations().size(), 1);
}

void test_neural_recovery_missing_closing_brace() {
  std::string source = "function compute() {\n  let a = 5;\n  return a;\n";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  Parser parser(std::move(lex_res.tokens), RecoveryMode::Neural);
  auto parse_res = parser.parse();

  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_TRUE(parse_res.has_recovered());
}

void test_neural_recovery_rejected_candidate_fallback() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  // Configure a MockInferenceEngine that proposes an invalid token type (e.g. KwIf instead of Semicolon)
  RepairPrediction bad_pred;
  bad_pred.operation = RepairOperation::InsertToken;
  bad_pred.token_type = TokenType::KwIf;
  bad_pred.position_offset = 4;
  bad_pred.confidence = 0.99;

  auto mock_engine = std::make_shared<MockInferenceEngine>(bad_pred);
  auto controller = std::make_shared<ErrorRecoveryController>();
  controller->clear_strategies();
  controller->add_strategy(std::make_unique<NeuralRecoveryStrategy>(mock_engine));
  controller->add_strategy(std::make_unique<HeuristicRecovery>(/*enable_panic_fallback=*/false));
  controller->add_strategy(std::make_unique<PanicModeRecovery>());

  Parser parser(std::move(lex_res.tokens), controller);
  auto parse_res = parser.parse();

  // The invalid neural candidate was rejected by RepairValidator, so controller cleanly fell back to Heuristic recovery
  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_EQ(parse_res.metrics.recovered_errors, 1);
  ASSERT_EQ(parse_res.recoveries.front().strategy, RecoveryStrategyType::Heuristic);
}

void test_neural_recovery_engine_unavailable_fallback() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  auto mock_engine = std::make_shared<MockInferenceEngine>();
  mock_engine->set_ready(false); // Model not loaded

  auto controller = std::make_shared<ErrorRecoveryController>();
  controller->clear_strategies();
  controller->add_strategy(std::make_unique<NeuralRecoveryStrategy>(mock_engine));
  controller->add_strategy(std::make_unique<HeuristicRecovery>(/*enable_panic_fallback=*/false));
  controller->add_strategy(std::make_unique<PanicModeRecovery>());

  Parser parser(std::move(lex_res.tokens), controller);
  auto parse_res = parser.parse();

  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_EQ(parse_res.metrics.recovered_errors, 1);
  ASSERT_EQ(parse_res.recoveries.front().strategy, RecoveryStrategyType::Heuristic);
}

void test_neural_recovery_multiple_errors() {
  std::string source = "let a = 1\nlet b = 2\nlet c = 3;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  Parser parser(std::move(lex_res.tokens), RecoveryMode::Neural);
  auto parse_res = parser.parse();

  // Two missing semicolons recovered sequentially
  ASSERT_TRUE(parse_res.program != nullptr);
  ASSERT_EQ(parse_res.errors.size(), 2);
  ASSERT_EQ(parse_res.metrics.recovered_errors, 2);
  ASSERT_EQ(parse_res.program->declarations().size(), 3);
}

void test_neural_recovery_ast_structural_integrity() {
  std::string valid_source = "let x = 10;\nlet y = 20;";
  Lexer lexer1(valid_source);
  auto lex_res1 = lexer1.tokenize();
  Parser parser1(std::move(lex_res1.tokens), RecoveryMode::None);
  auto parse_res1 = parser1.parse();
  ASSERT_TRUE(parse_res1.ok());

  std::string corrupted_source = "let x = 10\nlet y = 20;";
  Lexer lexer2(corrupted_source);
  auto lex_res2 = lexer2.tokenize();
  Parser parser2(std::move(lex_res2.tokens), RecoveryMode::Neural);
  auto parse_res2 = parser2.parse();
  ASSERT_TRUE(parse_res2.program != nullptr);

  // Both should have 2 VariableDeclaration nodes
  ASSERT_EQ(parse_res1.program->declarations().size(), parse_res2.program->declarations().size());
  ASSERT_EQ(parse_res2.program->declarations().size(), 2);

  auto* var1 = dynamic_cast<ast::VariableDeclaration*>(parse_res2.program->declarations()[0].get());
  ASSERT_TRUE(var1 != nullptr);
  ASSERT_EQ(var1->name(), "x");

  auto* var2 = dynamic_cast<ast::VariableDeclaration*>(parse_res2.program->declarations()[1].get());
  ASSERT_TRUE(var2 != nullptr);
  ASSERT_EQ(var2->name(), "y");
}

void test_neural_recovery_empty_construct_context() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  auto tokens = lex_res.tokens;
  std::size_t cursor = 4;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  NeuralRecoveryStrategy strategy;
  auto result = strategy.recover(tokens, cursor, err, "");
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.action, RecoveryActionType::InsertToken);
}

void test_neural_recovery_non_empty_construct_context() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  auto tokens = lex_res.tokens;
  std::size_t cursor = 4;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  NeuralRecoveryStrategy strategy;
  auto result = strategy.recover(tokens, cursor, err, "let_declaration");
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.action, RecoveryActionType::InsertToken);
}

void test_neural_recovery_malformed_construct_context() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  auto tokens = lex_res.tokens;
  std::size_t cursor = 4;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  NeuralRecoveryStrategy strategy;
  auto result = strategy.recover(tokens, cursor, err, "###_PATHOLOGICAL_CONTEXT_STRING_$$$");
  ASSERT_TRUE(result.success);
}

void test_neural_recovery_repeated_recoveries() {
  for (int iter = 0; iter < 10; ++iter) {
    std::string source = "let x = 42\nlet y = 10;";
    Lexer lexer(source);
    auto lex_res = lexer.tokenize();
    auto tokens = lex_res.tokens;
    std::size_t cursor = 4;

    ParserError err;
    err.message = "expected ';'";
    err.token = tokens[4];
    err.token_index = 4;
    err.expected_tokens = {TokenType::Semicolon};

    NeuralRecoveryStrategy strategy;
    auto result = strategy.recover(tokens, cursor, err, "statement");
    ASSERT_TRUE(result.success);
  }
}

void test_neural_recovery_inference_failure() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  auto tokens = lex_res.tokens;
  std::size_t cursor = 4;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {};

  // Mock engine returning std::nullopt (empty prediction)
  auto mock_engine = std::make_shared<MockInferenceEngine>(std::nullopt);
  NeuralRecoveryStrategy strategy(mock_engine);
  auto result = strategy.recover(tokens, cursor, err, "statement");
  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.action, RecoveryActionType::None);
}

void test_neural_recovery_invalid_model_output_bounds() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  auto tokens = lex_res.tokens;
  std::size_t cursor = 4;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  RepairPrediction bad_pred;
  bad_pred.operation = RepairOperation::InsertToken;
  bad_pred.token_type = TokenType::Semicolon;
  bad_pred.position_offset = 999999; // Out of bounds offset
  bad_pred.confidence = 0.99;

  auto mock_engine = std::make_shared<MockInferenceEngine>(bad_pred);
  NeuralRecoveryStrategy strategy(mock_engine);
  auto result = strategy.recover(tokens, cursor, err, "statement");
  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.action, RecoveryActionType::None);
}

}  // namespace

void run_neural_recovery_tests() {
  test_neural_recovery_missing_semicolon();
  test_neural_recovery_missing_closing_paren();
  test_neural_recovery_missing_closing_brace();
  test_neural_recovery_rejected_candidate_fallback();
  test_neural_recovery_engine_unavailable_fallback();
  test_neural_recovery_multiple_errors();
  test_neural_recovery_ast_structural_integrity();
  test_neural_recovery_empty_construct_context();
  test_neural_recovery_non_empty_construct_context();
  test_neural_recovery_malformed_construct_context();
  test_neural_recovery_repeated_recoveries();
  test_neural_recovery_inference_failure();
  test_neural_recovery_invalid_model_output_bounds();
}
