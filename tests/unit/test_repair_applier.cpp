#include "neuroparse/recovery/repair_applier.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "test_harness.hpp"

#include <string>
#include <vector>

namespace {

using namespace neuroparse;
using namespace neuroparse::recovery;
using namespace neuroparse::lexer;

#define ASSERT_TRUE(c) NP_EXPECT(c)
#define ASSERT_FALSE(c) NP_EXPECT(!(c))
#define ASSERT_EQ(a, b) NP_EXPECT((a) == (b))

void test_repair_applier_insert_semicolon() {
  std::string source = "let x = 42\nlet y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  RepairApplier applier;
  RepairPrediction pred;
  pred.operation = RepairOperation::InsertToken;
  pred.token_type = TokenType::Semicolon;
  pred.token_lexeme = ";";
  pred.position_offset = 4; // After '42'

  std::size_t cursor = 4;
  auto result = applier.apply(lex_res.tokens, cursor, pred);

  ASSERT_TRUE(result.applied);
  ASSERT_EQ(result.status, RepairApplyStatus::Applied);
  ASSERT_EQ(result.operation, RepairOperation::InsertToken);
  ASSERT_EQ(result.position, 4);
  ASSERT_EQ(result.new_parser_position, 4);
  ASSERT_EQ(lex_res.tokens[4].type, TokenType::Semicolon);
  ASSERT_EQ(lex_res.tokens[4].lexeme, ";");
  ASSERT_EQ(cursor, 4);
}

void test_repair_applier_delete_token() {
  std::string source = "let x = 42 ;; let y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  RepairApplier applier;
  RepairPrediction pred;
  pred.operation = RepairOperation::DeleteToken;
  pred.position_offset = 5; // Second semicolon

  std::size_t cursor = 5;
  auto result = applier.apply(lex_res.tokens, cursor, pred);

  ASSERT_TRUE(result.applied);
  ASSERT_EQ(result.status, RepairApplyStatus::Applied);
  ASSERT_EQ(result.operation, RepairOperation::DeleteToken);
  ASSERT_EQ(result.tokens_changed.size(), 1);
  ASSERT_EQ(result.tokens_changed[0].lexeme, ";");
  ASSERT_EQ(cursor, 5);
}

void test_repair_applier_prevent_delete_eof() {
  std::string source = "let x = 42;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  std::size_t eof_index = lex_res.tokens.size() - 1;
  ASSERT_EQ(lex_res.tokens[eof_index].type, TokenType::Eof);

  RepairApplier applier;
  RepairPrediction pred;
  pred.operation = RepairOperation::DeleteToken;
  pred.position_offset = eof_index;

  std::size_t cursor = eof_index;
  auto result = applier.apply(lex_res.tokens, cursor, pred);

  ASSERT_FALSE(result.applied);
  ASSERT_EQ(result.status, RepairApplyStatus::InvalidState);
  ASSERT_EQ(lex_res.tokens.back().type, TokenType::Eof);
}

void test_repair_applier_replace_token() {
  std::string source = "let x = 42 : let y = 10;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  RepairApplier applier;
  RepairPrediction pred;
  pred.operation = RepairOperation::ReplaceToken;
  pred.token_type = TokenType::Semicolon;
  pred.token_lexeme = ";";
  pred.position_offset = 4; // The ':'

  std::size_t cursor = 4;
  auto result = applier.apply(lex_res.tokens, cursor, pred);

  ASSERT_TRUE(result.applied);
  ASSERT_EQ(result.status, RepairApplyStatus::Applied);
  ASSERT_EQ(result.operation, RepairOperation::ReplaceToken);
  ASSERT_EQ(result.tokens_changed.size(), 2);
  ASSERT_EQ(result.tokens_changed[0].lexeme, ":");
  ASSERT_EQ(result.tokens_changed[1].lexeme, ";");
  ASSERT_EQ(lex_res.tokens[4].type, TokenType::Semicolon);
  ASSERT_EQ(cursor, 4);
}

void test_repair_applier_no_repair() {
  std::string source = "let x = 42;";
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  ASSERT_FALSE(lex_res.has_errors());

  std::size_t original_size = lex_res.tokens.size();

  RepairApplier applier;
  RepairPrediction pred;
  pred.operation = RepairOperation::NoRepair;
  pred.position_offset = 2;

  std::size_t cursor = 2;
  auto result = applier.apply(lex_res.tokens, cursor, pred);

  ASSERT_FALSE(result.applied);
  ASSERT_EQ(result.status, RepairApplyStatus::NoRepair);
  ASSERT_EQ(lex_res.tokens.size(), original_size);
  ASSERT_EQ(cursor, 2);
}

}  // namespace

void run_repair_applier_tests() {
  test_repair_applier_insert_semicolon();
  test_repair_applier_delete_token();
  test_repair_applier_prevent_delete_eof();
  test_repair_applier_replace_token();
  test_repair_applier_no_repair();
}
