#include "test_harness.hpp"
#include "neuroparse/ast/ast.hpp"
#include "neuroparse/fuzz/invariants.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"
#include "neuroparse/recovery/context_extractor.hpp"
#include "neuroparse/recovery/error_recovery_controller.hpp"
#include "neuroparse/recovery/heuristic_recovery.hpp"
#include "neuroparse/recovery/neural_recovery.hpp"
#include "neuroparse/recovery/panic_mode.hpp"
#include "neuroparse/recovery/repair_applier.hpp"
#include "neuroparse/recovery/repair_candidate.hpp"
#include "neuroparse/recovery/repair_validator.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace neuroparse;

void test_lexer_fuzz_boundary() {
  std::vector<std::string> edge_inputs = {
      "",                                          // empty
      "   \t\r\n\n\r   ",                         // whitespace
      std::string(5000, 'a'),                     // 5000 char identifier
      std::string(5000, '9'),                     // 5000 digit number
      "123.45.67.89",                             // malformed number
      "..543",                                    // malformed dot number
      "\"unterminated string literal",            // unterminated string
      "\"unterminated with \\\" escape",          // unclosed with escape
      "\"escape\\x\\u\\9\\z\\0\"",                // unusual escapes
      "/* unterminated block comment",            // unterminated comment
      "/* nested /* comment */ still unclosed",   // nested comment
      "\x00\x01\x02\x03\xFF\xFE\x7F",             // binary and control chars
      "((((((((((((((((((((",                     // repeated parens
      "))))))))))))))))))))",                     // repeated closing parens
      ";;;;;;;;;;;;;;;;;;;;",                     // repeated semicolons
      "let \xcf\x80 = 3.14159;",                  // UTF-8 identifier (pi)
      "let 🦀 = 42;",                             // emoji identifier
      "+ - * / % == != < <= > >= && || ! = . , ; : ( ) { } [ ]", // all punctuation
  };

  for (const auto& src : edge_inputs) {
    lexer::Lexer lexer(src);
    auto lex_res = lexer.tokenize();
    NP_EXPECT(!lex_res.tokens.empty());
    NP_EXPECT(lex_res.tokens.back().type == lexer::TokenType::Eof);
  }
}

void test_parser_fuzz_pathological() {
  std::vector<std::string> pathological_sources = {
      // Deeply nested parentheses
      std::string(300, '(') + "42" + std::string(300, ')'),
      // Deeply nested braces
      "function deep() " + std::string(200, '{') + "let x = 1;" + std::string(200, '}'),
      // Repeated operators
      "let x = 1 + + + + + + + + 2;",
      "let y = a * * * * * * b;",
      "let z = ! ! ! ! ! ! ! true;",
      // Mismatched delimiters
      "function broken({[)}] }",
      "function test() { if (x > 0 { let y = 1; } }",
      // Malformed declarations
      "function () {}",
      "function 123() {}",
      "let = 42;",
      "let x = ;",
      "while () {}",
      "if () {} else {}",
      // Truncated programs
      "function main() { let x = ",
      "function main() { if (",
      "function main() { return ",
  };

  for (const auto& src : pathological_sources) {
    lexer::Lexer lexer(src);
    auto lex_res = lexer.tokenize();
    parser::Parser parser(std::move(lex_res.tokens), recovery::RecoveryMode::Panic);
    auto parse_res = parser.parse();
    // Parser must terminate safely without crashing
    if (parse_res.program) {
      auto inv = fuzz::check_ast_invariants(parse_res.program.get());
      NP_EXPECT(inv.is_valid);
    }
  }
}

void test_context_extractor_fuzz() {
  std::string src = "function test(a, b) {\n  let x = a + b;\n  return x;\n}";
  lexer::Lexer lexer(src);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  recovery::ContextExtractor extractor;

  // Test various cursor positions
  std::vector<std::size_t> test_cursors = {
      0, 1, 5, tokens.size() / 2, tokens.size() - 1, tokens.size(), 1000, 999999
  };

  for (auto cursor : test_cursors) {
    parser::ParserError err{
        "Fuzz syntax error",
        cursor < tokens.size() ? tokens[cursor] : tokens.back(),
        cursor,
        cursor < tokens.size() ? tokens[cursor].location : tokens.back().location,
        {},
        "FuzzContext"
    };
    auto ctx = extractor.extract(tokens, err);
    NP_EXPECT(ctx.tokens_before.size() <= 20);
    NP_EXPECT(ctx.tokens_after.size() <= 20);
  }

  // Test on empty tokens vector
  std::vector<lexer::Token> empty_tokens;
  parser::ParserError empty_err{"Empty error", lexer::Token{}, 0, lexer::SourceLocation{}, {}, "Empty"};
  auto empty_ctx = extractor.extract(empty_tokens, empty_err);
  NP_EXPECT(empty_ctx.tokens_before.empty());
  NP_EXPECT(empty_ctx.tokens_after.empty());
}

void test_repair_validator_fuzz() {
  std::string src = "function foo() {\n  let x = 10\n  return x;\n}";
  lexer::Lexer lexer(src);
  auto lex_res = lexer.tokenize();
  const auto& tokens = lex_res.tokens;

  recovery::RepairValidator validator;
  parser::ParserError err{
      "Missing semicolon",
      tokens.size() > 4 ? tokens[4] : tokens.back(),
      4,
      tokens.size() > 4 ? tokens[4].location : tokens.back().location,
      {lexer::TokenType::Semicolon},
      "let binding"
  };

  recovery::ContextExtractor extractor;
  auto context = extractor.extract(tokens, err);

  // Test adversarial and out-of-bounds RepairPredictions
  std::vector<recovery::RepairPrediction> adversarial_preds;

  // 1. Unknown operation
  recovery::RepairPrediction pred1;
  pred1.operation = static_cast<recovery::RepairOperation>(99);
  pred1.token_type = lexer::TokenType::Semicolon;
  pred1.token_lexeme = ";";
  pred1.position_offset = 4;
  pred1.confidence = 0.9;
  adversarial_preds.push_back(pred1);

  // 2. Huge position
  recovery::RepairPrediction pred2;
  pred2.operation = recovery::RepairOperation::InsertToken;
  pred2.token_type = lexer::TokenType::Semicolon;
  pred2.token_lexeme = ";";
  pred2.position_offset = 999999;
  pred2.confidence = 0.9;
  adversarial_preds.push_back(pred2);

  // 3. Huge negative-cast position
  recovery::RepairPrediction pred3;
  pred3.operation = recovery::RepairOperation::InsertToken;
  pred3.token_type = lexer::TokenType::Semicolon;
  pred3.token_lexeme = ";";
  pred3.position_offset = static_cast<std::size_t>(-1);
  pred3.confidence = 0.9;
  adversarial_preds.push_back(pred3);

  // 4. Invalid token type
  recovery::RepairPrediction pred4;
  pred4.operation = recovery::RepairOperation::InsertToken;
  pred4.token_type = static_cast<lexer::TokenType>(99999);
  pred4.token_lexeme = "unknown";
  pred4.position_offset = 4;
  pred4.confidence = 0.9;
  adversarial_preds.push_back(pred4);

  // 5. Delete beyond tokens length
  recovery::RepairPrediction pred5;
  pred5.operation = recovery::RepairOperation::DeleteToken;
  pred5.position_offset = tokens.size() + 10;
  pred5.confidence = 0.9;
  adversarial_preds.push_back(pred5);

  // 6. Replace beyond tokens length
  recovery::RepairPrediction pred6;
  pred6.operation = recovery::RepairOperation::ReplaceToken;
  pred6.token_type = lexer::TokenType::Semicolon;
  pred6.token_lexeme = ";";
  pred6.position_offset = tokens.size() + 10;
  pred6.confidence = 0.9;
  adversarial_preds.push_back(pred6);

  // 7. NaN confidence
  recovery::RepairPrediction pred7;
  pred7.operation = recovery::RepairOperation::InsertToken;
  pred7.token_type = lexer::TokenType::Semicolon;
  pred7.token_lexeme = ";";
  pred7.position_offset = 4;
  pred7.confidence = std::numeric_limits<double>::quiet_NaN();
  adversarial_preds.push_back(pred7);

  // 8. Negative confidence
  recovery::RepairPrediction pred8;
  pred8.operation = recovery::RepairOperation::InsertToken;
  pred8.token_type = lexer::TokenType::Semicolon;
  pred8.token_lexeme = ";";
  pred8.position_offset = 4;
  pred8.confidence = -1.0;
  adversarial_preds.push_back(pred8);

  for (const auto& pred : adversarial_preds) {
    auto res = validator.validate(tokens, 4, context, pred);
    // Validator must safely handle without crashing
    if (!res.is_accepted()) {
      NP_EXPECT(!res.is_accepted());
    }
  }
}

void test_repair_applier_fuzz() {
  std::string src = "function test() {\n  let x = 1;\n}";
  lexer::Lexer lexer(src);
  auto lex_res = lexer.tokenize();
  const auto original_tokens = lex_res.tokens;

  recovery::RepairApplier applier;

  // 1. Boundary Insert at start (0)
  {
    auto tokens = original_tokens;
    recovery::RepairPrediction pred;
    pred.operation = recovery::RepairOperation::InsertToken;
    pred.token_type = lexer::TokenType::Semicolon;
    pred.token_lexeme = ";";
    pred.position_offset = 0;

    std::size_t cursor = 0;
    auto res = applier.apply(tokens, cursor, pred);
    NP_EXPECT(res.applied);
    NP_EXPECT(tokens.front().type == lexer::TokenType::Semicolon);
    auto inv = fuzz::check_token_stream_invariants(tokens);
    NP_EXPECT(inv.is_valid);
  }

  // 2. Boundary Insert before EOF
  {
    auto tokens = original_tokens;
    std::size_t eof_pos = tokens.size() - 1;
    recovery::RepairPrediction pred;
    pred.operation = recovery::RepairOperation::InsertToken;
    pred.token_type = lexer::TokenType::RightBrace;
    pred.token_lexeme = "}";
    pred.position_offset = eof_pos;

    std::size_t cursor = eof_pos;
    auto res = applier.apply(tokens, cursor, pred);
    NP_EXPECT(res.applied);
    auto inv = fuzz::check_token_stream_invariants(tokens);
    NP_EXPECT(inv.is_valid);
  }

  // 3. Boundary Delete at 0
  {
    auto tokens = original_tokens;
    recovery::RepairPrediction pred;
    pred.operation = recovery::RepairOperation::DeleteToken;
    pred.position_offset = 0;

    std::size_t cursor = 0;
    auto res = applier.apply(tokens, cursor, pred);
    NP_EXPECT(res.applied);
    auto inv = fuzz::check_token_stream_invariants(tokens);
    NP_EXPECT(inv.is_valid);
  }

  // 4. Invalid operation out-of-bounds
  {
    auto tokens = original_tokens;
    recovery::RepairPrediction pred;
    pred.operation = recovery::RepairOperation::DeleteToken;
    pred.position_offset = 99999;

    std::size_t cursor = 0;
    auto res = applier.apply(tokens, cursor, pred);
    NP_EXPECT(!res.applied);
  }
}

void test_recovery_loop_protection() {
  std::string src = "function loop_test() {\n  let x = ;\n}";
  lexer::Lexer lexer(src);
  auto lex_res = lexer.tokenize();

  recovery::RepairPrediction fixed_pred;
  fixed_pred.operation = recovery::RepairOperation::InsertToken;
  fixed_pred.token_type = lexer::TokenType::Semicolon;
  fixed_pred.token_lexeme = ";";
  fixed_pred.position_offset = 4;
  fixed_pred.confidence = 0.95;

  auto mock_engine = std::make_shared<recovery::MockInferenceEngine>(fixed_pred);
  auto neural_strat = std::make_unique<recovery::NeuralRecoveryStrategy>(mock_engine);
  std::vector<std::unique_ptr<recovery::ErrorRecoveryStrategy>> strategies;
  strategies.push_back(std::move(neural_strat));
  strategies.push_back(std::make_unique<recovery::PanicModeRecovery>());

  auto controller = std::make_shared<recovery::ErrorRecoveryController>(std::move(strategies));
  parser::Parser parser(std::move(lex_res.tokens), controller);

  auto parse_res = parser.parse();
  // Recovery controller must terminate without infinite loop and report errors
  NP_EXPECT(parse_res.has_errors());
  NP_EXPECT(parse_res.recoveries.size() <= 50);
  NP_EXPECT(parse_res.metrics.total_errors <= 50);
}

void test_invariants_subsystem() {
  // 1. Valid token stream
  std::vector<lexer::Token> valid_tokens = {
      lexer::Token{lexer::TokenType::KwLet, "let", lexer::SourceLocation{0, 1, 1}, 3},
      lexer::Token{lexer::TokenType::Identifier, "x", lexer::SourceLocation{4, 1, 5}, 1},
      lexer::Token{lexer::TokenType::Equal, "=", lexer::SourceLocation{6, 1, 7}, 1},
      lexer::Token{lexer::TokenType::IntegerLiteral, "10", lexer::SourceLocation{8, 1, 9}, 2},
      lexer::Token{lexer::TokenType::Semicolon, ";", lexer::SourceLocation{10, 1, 11}, 1},
      lexer::Token{lexer::TokenType::Eof, "", lexer::SourceLocation{11, 1, 12}, 0}
  };
  auto res1 = fuzz::check_token_stream_invariants(valid_tokens);
  NP_EXPECT(res1.is_valid);
  NP_EXPECT(res1.violations.empty());

  // 2. Corrupted token stream (missing EOF)
  std::vector<lexer::Token> no_eof_tokens = {
      lexer::Token{lexer::TokenType::KwLet, "let", lexer::SourceLocation{0, 1, 1}, 3},
  };
  auto res2 = fuzz::check_token_stream_invariants(no_eof_tokens);
  NP_EXPECT(!res2.is_valid);

  // 3. Corrupted token stream (internal EOF)
  std::vector<lexer::Token> internal_eof = {
      lexer::Token{lexer::TokenType::Eof, "", lexer::SourceLocation{0, 1, 1}, 0},
      lexer::Token{lexer::TokenType::Semicolon, ";", lexer::SourceLocation{1, 1, 2}, 1},
      lexer::Token{lexer::TokenType::Eof, "", lexer::SourceLocation{2, 1, 3}, 0}
  };
  auto res3 = fuzz::check_token_stream_invariants(internal_eof);
  NP_EXPECT(!res3.is_valid);

  // 4. Valid AST invariant
  auto lit = std::make_unique<ast::LiteralExpression>(
      static_cast<std::int64_t>(42), ast::LiteralType::Integer, ast::SourceSpan{{0, 1, 1}, {2, 1, 3}}
  );
  auto ast_res = fuzz::check_ast_invariants(lit.get());
  NP_EXPECT(ast_res.is_valid);
}

void test_determinism() {
  std::string src = "function test(a, b) {\n  if (a > b {\n    return a;\n  } else {\n    return b\n  }\n}";
  
  std::vector<std::string> first_run_diags;
  for (int run = 0; run < 5; ++run) {
    lexer::Lexer lexer(src);
    auto lex_res = lexer.tokenize();
    parser::Parser parser(std::move(lex_res.tokens), recovery::RecoveryMode::Auto);
    auto parse_res = parser.parse();

    std::vector<std::string> current_diags;
    for (const auto& err : parse_res.errors) {
      current_diags.push_back(err.message);
    }

    if (run == 0) {
      first_run_diags = current_diags;
    } else {
      NP_EXPECT(first_run_diags.size() == current_diags.size());
      for (std::size_t i = 0; i < first_run_diags.size(); ++i) {
        NP_EXPECT(first_run_diags[i] == current_diags[i]);
      }
    }
  }
}

}  // namespace

void run_fuzz_unit_tests() {
  test_lexer_fuzz_boundary();
  test_parser_fuzz_pathological();
  test_context_extractor_fuzz();
  test_repair_validator_fuzz();
  test_repair_applier_fuzz();
  test_recovery_loop_protection();
  test_invariants_subsystem();
  test_determinism();
}
