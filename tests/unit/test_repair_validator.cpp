#include "neuroparse/recovery/repair_validator.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/context_extractor.hpp"
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

void test_valid_insert_semicolon() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.message = "expected ';'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};
  err.construct = "let binding";

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Semicolon;
  prediction.token_lexeme = ";";
  prediction.position_offset = context.tokens_before.size();
  prediction.confidence = 0.95;

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);

  ASSERT_TRUE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::Accepted);
  ASSERT_TRUE(result.checks.all_passed());
  ASSERT_TRUE(result.checks.operation_check.passed);
  ASSERT_TRUE(result.checks.position_check.passed);
  ASSERT_TRUE(result.checks.token_check.passed);
  ASSERT_TRUE(result.checks.context_check.passed);
  ASSERT_TRUE(result.checks.structural_safety_check.passed);
  ASSERT_TRUE(result.checks.progress_check.passed);
}

void test_valid_insert_closing_paren() {
  std::string source = "if (x > 0 { let y = 1; }";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.message = "expected ')'";
  err.token = tokens[5];
  err.token_index = 5;
  err.expected_tokens = {TokenType::RightParen};
  err.construct = "if statement";

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::RightParen;
  prediction.token_lexeme = ")";
  prediction.position_offset = context.tokens_before.size();
  prediction.confidence = 0.92;

  RepairValidator validator;
  auto result = validator.validate(tokens, 5, context, prediction);

  ASSERT_TRUE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::Accepted);
}

void test_valid_delete_unexpected_token() {
  std::string source = "let x = 10 let ;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.message = "unexpected token 'let'";
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};
  err.construct = "let binding";

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::DeleteToken;
  prediction.position_offset = context.tokens_before.size();
  prediction.confidence = 0.88;

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);

  ASSERT_TRUE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::Accepted);
}

void test_valid_replace_token() {
  std::string source = "let x = (10 + 20];";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.message = "expected ')'";
  err.token = tokens[7];
  err.token_index = 7;
  err.expected_tokens = {TokenType::RightParen};
  err.construct = "parenthesized expression";

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::ReplaceToken;
  prediction.token_type = TokenType::RightParen;
  prediction.token_lexeme = ")";
  prediction.position_offset = context.tokens_before.size();
  prediction.confidence = 0.90;

  RepairValidator validator;
  auto result = validator.validate(tokens, 7, context, prediction);

  ASSERT_TRUE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::Accepted);
}

void test_valid_no_repair() {
  std::string source = "let x = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[0];
  err.token_index = 0;
  err.construct = "program";

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::NoRepair;
  prediction.confidence = 0.99;

  RepairValidator validator;
  auto result = validator.validate(tokens, 0, context, prediction);

  ASSERT_TRUE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::Accepted);
}

void test_invalid_unknown_operation() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::Unknown;
  prediction.position_offset = 0;

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::InvalidOperation);
  ASSERT_FALSE(result.checks.operation_check.passed);
}

void test_invalid_insert_eof_token() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Eof;
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::InvalidToken);
  ASSERT_FALSE(result.checks.token_check.passed);
}

void test_invalid_out_of_bounds_position() {
  std::string source = "let x = 42;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[2];
  err.token_index = 2;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Semicolon;
  prediction.position_offset = 999999;

  RepairValidator validator;
  auto result = validator.validate(tokens, 2, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::InvalidPosition);
  ASSERT_FALSE(result.checks.position_check.passed);
}

void test_invalid_context_mismatch_token() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  // Proposing Star '*' when Semicolon is required
  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Star;
  prediction.token_lexeme = "*";
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::NotExpected);
  ASSERT_FALSE(result.checks.context_check.passed);
}

void test_invalid_delete_eof() {
  std::string source = "let x = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  std::size_t eof_index = tokens.size() - 1;
  ParserError err;
  err.token = tokens[eof_index];
  err.token_index = eof_index;

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::DeleteToken;
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto result = validator.validate(tokens, eof_index, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::InvalidPosition);
}

void test_invalid_unmatched_closing_brace_insertion() {
  std::string source = "let x = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[0];
  err.token_index = 0;

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::RightBrace;
  prediction.token_lexeme = "}";
  prediction.position_offset = 0;

  RepairValidator validator;
  auto result = validator.validate(tokens, 0, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::UnsafeRepair);
  ASSERT_FALSE(result.checks.structural_safety_check.passed);
}

void test_invalid_no_progress_identical_replace() {
  std::string source = "let x = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[2];
  err.token_index = 2;
  err.expected_tokens = {TokenType::Equal};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::ReplaceToken;
  prediction.token_type = TokenType::Equal;
  prediction.token_lexeme = "=";
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto result = validator.validate(tokens, 2, context, prediction);

  ASSERT_FALSE(result.is_accepted());
  ASSERT_EQ(result.status, ValidationStatus::WouldNotProgress);
  ASSERT_FALSE(result.checks.progress_check.passed);
}

void test_non_mutation_guarantee() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;
  auto tokens_copy = tokens;

  std::size_t cursor = 4;
  std::size_t cursor_copy = cursor;

  ParserError err;
  err.token = tokens[cursor];
  err.token_index = cursor;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);
  auto context_copy = context;

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Semicolon;
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto result = validator.validate(tokens, cursor, context, prediction);

  ASSERT_TRUE(result.is_accepted());

  // Verify non-mutation of inputs
  ASSERT_EQ(tokens.size(), tokens_copy.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    ASSERT_EQ(tokens[i], tokens_copy[i]);
  }
  ASSERT_EQ(cursor, cursor_copy);
  ASSERT_EQ(context, context_copy);
}

void test_deterministic_repeatability() {
  std::string source = "if (a > 0 { let b = 2; }";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[5];
  err.token_index = 5;
  err.expected_tokens = {TokenType::RightParen};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::RightParen;
  prediction.token_lexeme = ")";
  prediction.position_offset = context.tokens_before.size();

  RepairValidator validator;
  auto baseline = validator.validate(tokens, 5, context, prediction);

  for (int iter = 0; iter < 100; ++iter) {
    auto repeat = validator.validate(tokens, 5, context, prediction);
    ASSERT_EQ(baseline.accepted, repeat.accepted);
    ASSERT_EQ(baseline.status, repeat.status);
    ASSERT_EQ(baseline.reason, repeat.reason);
    ASSERT_EQ(baseline.checks.all_passed(), repeat.checks.all_passed());
  }
}

void test_explainability_report() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  ParserError err;
  err.token = tokens[4];
  err.token_index = 4;
  err.expected_tokens = {TokenType::Semicolon};

  ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  RepairPrediction prediction;
  prediction.operation = RepairOperation::InsertToken;
  prediction.token_type = TokenType::Semicolon;
  prediction.token_lexeme = ";";
  prediction.position_offset = context.tokens_before.size();
  prediction.confidence = 0.95;

  RepairValidator validator;
  auto result = validator.validate(tokens, 4, context, prediction);
  std::string explanation = result.explain();

  ASSERT_FALSE(explanation.empty());
  ASSERT_TRUE(explanation.find("Proposed repair:") != std::string::npos);
  ASSERT_TRUE(explanation.find("INSERT_TOKEN") != std::string::npos);
  ASSERT_TRUE(explanation.find("[PASS] operation_validity") != std::string::npos);
  ASSERT_TRUE(explanation.find("RESULT: ACCEPTED") != std::string::npos);
}

}  // namespace

void run_repair_validator_tests() {
  test_valid_insert_semicolon();
  test_valid_insert_closing_paren();
  test_valid_delete_unexpected_token();
  test_valid_replace_token();
  test_valid_no_repair();
  test_invalid_unknown_operation();
  test_invalid_insert_eof_token();
  test_invalid_out_of_bounds_position();
  test_invalid_context_mismatch_token();
  test_invalid_delete_eof();
  test_invalid_unmatched_closing_brace_insertion();
  test_invalid_no_progress_identical_replace();
  test_non_mutation_guarantee();
  test_deterministic_repeatability();
  test_explainability_report();
}
