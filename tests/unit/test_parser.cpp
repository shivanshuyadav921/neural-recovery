#include "test_harness.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"

using namespace neuroparse;
using namespace neuroparse::lexer;
using namespace neuroparse::parser;
using namespace neuroparse::ast;

namespace {

ParseResult parse_source(std::string_view source) {
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  Parser parser(std::move(lex_res.tokens));
  return parser.parse();
}

}  // namespace

// 1. Empty program
void test_parser_empty_program() {
  auto res = parse_source("");
  NP_EXPECT(res.ok());
  NP_EXPECT(res.program != nullptr);
  NP_EXPECT(res.program->declarations().empty());
}

// 2. Simple function
void test_parser_simple_function() {
  auto res = parse_source("function main() { return 0; }");
  NP_EXPECT(res.ok());
  NP_EXPECT(res.program->declarations().size() == 1);

  const auto* fn = dynamic_cast<const FunctionDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  NP_EXPECT(fn->name() == "main");
  NP_EXPECT(fn->parameters().empty());
  NP_EXPECT(fn->body() != nullptr);
  NP_EXPECT(fn->body()->statements().size() == 1);
}

// 3. Function parameters
void test_parser_function_parameters() {
  auto res = parse_source("function add(a, b, c) { return a + b + c; }");
  NP_EXPECT(res.ok());
  const auto* fn = dynamic_cast<const FunctionDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  NP_EXPECT(fn->parameters().size() == 3);
  NP_EXPECT(fn->parameters()[0].name == "a");
  NP_EXPECT(fn->parameters()[1].name == "b");
  NP_EXPECT(fn->parameters()[2].name == "c");
}

// 4. Variable declaration
void test_parser_variable_declaration() {
  auto res = parse_source("let x = 42; let y = x + 1;");
  NP_EXPECT(res.ok());
  NP_EXPECT(res.program->declarations().size() == 2);

  const auto* v1 = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(v1 != nullptr);
  NP_EXPECT(v1->name() == "x");
  NP_EXPECT(v1->initializer() != nullptr);

  const auto* v2 = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[1].get());
  NP_EXPECT(v2 != nullptr);
  NP_EXPECT(v2->name() == "y");
}

// 5. Return statement
void test_parser_return_statement() {
  auto res = parse_source("function test() { return; return 123; }");
  NP_EXPECT(res.ok());
  const auto* fn = dynamic_cast<const FunctionDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  NP_EXPECT(fn->body()->statements().size() == 2);

  const auto* r1 = dynamic_cast<const ReturnStatement*>(fn->body()->statements()[0].get());
  NP_EXPECT(r1 != nullptr);
  NP_EXPECT(r1->value() == nullptr);

  const auto* r2 = dynamic_cast<const ReturnStatement*>(fn->body()->statements()[1].get());
  NP_EXPECT(r2 != nullptr);
  NP_EXPECT(r2->value() != nullptr);
}

// 6. If statement
void test_parser_if_statement() {
  auto res = parse_source("if (x > 0) { return true; }");
  NP_EXPECT(res.ok());
  const auto* stmt = dynamic_cast<const IfStatement*>(res.program->declarations()[0].get());
  NP_EXPECT(stmt != nullptr);
  NP_EXPECT(stmt->condition() != nullptr);
  NP_EXPECT(stmt->then_branch() != nullptr);
  NP_EXPECT(stmt->else_branch() == nullptr);
}

// 7. If/Else statement
void test_parser_if_else_statement() {
  auto res = parse_source("if (a == b) { return 1; } else { return 2; }");
  NP_EXPECT(res.ok());
  const auto* stmt = dynamic_cast<const IfStatement*>(res.program->declarations()[0].get());
  NP_EXPECT(stmt != nullptr);
  NP_EXPECT(stmt->then_branch() != nullptr);
  NP_EXPECT(stmt->else_branch() != nullptr);
}

// 8. While statement
void test_parser_while_statement() {
  auto res = parse_source("while (count < 10) { count = count + 1; }");
  NP_EXPECT(res.ok());
  const auto* stmt = dynamic_cast<const WhileStatement*>(res.program->declarations()[0].get());
  NP_EXPECT(stmt != nullptr);
  NP_EXPECT(stmt->condition() != nullptr);
  NP_EXPECT(stmt->body() != nullptr);
}

// 9. Nested blocks
void test_parser_nested_blocks() {
  auto res = parse_source("{ { let inner = 1; } }");
  NP_EXPECT(res.ok());
  const auto* b1 = dynamic_cast<const BlockStatement*>(res.program->declarations()[0].get());
  NP_EXPECT(b1 != nullptr);
  NP_EXPECT(b1->statements().size() == 1);
  const auto* b2 = dynamic_cast<const BlockStatement*>(b1->statements()[0].get());
  NP_EXPECT(b2 != nullptr);
  NP_EXPECT(b2->statements().size() == 1);
}

// 10. Function calls
void test_parser_function_calls() {
  auto res = parse_source("let res = add(1, calc(2, 3));");
  NP_EXPECT(res.ok());
  const auto* decl = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(decl != nullptr);

  const auto* call = dynamic_cast<const CallExpression*>(decl->initializer());
  NP_EXPECT(call != nullptr);
  NP_EXPECT(call->arguments().size() == 2);

  const auto* inner_call = dynamic_cast<const CallExpression*>(call->arguments()[1].get());
  NP_EXPECT(inner_call != nullptr);
  NP_EXPECT(inner_call->arguments().size() == 2);
}

// 11. Arithmetic expressions
void test_parser_arithmetic_expressions() {
  auto res = parse_source("let x = 1 + 2 * 3 - 4 / 5 % 6;");
  NP_EXPECT(res.ok());
  const auto* decl = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(decl != nullptr);
  const auto* bin = dynamic_cast<const BinaryExpression*>(decl->initializer());
  NP_EXPECT(bin != nullptr);
}

// 12. Comparison expressions
void test_parser_comparison_expressions() {
  auto res = parse_source("let a = x < y; let b = x <= y; let c = x > y; let d = x >= y; let e = x == y; let f = x != y;");
  NP_EXPECT(res.ok());
  NP_EXPECT(res.program->declarations().size() == 6);
}

// 13. Logical expressions
void test_parser_logical_expressions() {
  auto res = parse_source("let ok = (a > 0) && (b < 10) || !flag;");
  NP_EXPECT(res.ok());
}

// 14. Unary expressions
void test_parser_unary_expressions() {
  auto res = parse_source("let neg = -42; let pos = +10; let inv = !true;");
  NP_EXPECT(res.ok());
  const auto* d1 = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(d1 != nullptr);
  const auto* u1 = dynamic_cast<const UnaryExpression*>(d1->initializer());
  NP_EXPECT(u1 != nullptr);
  NP_EXPECT(u1->op() == UnaryOperator::Minus);
}

// 15. Operator precedence (* higher than +)
void test_parser_operator_precedence() {
  auto res = parse_source("let x = 1 + 2 * 3;");
  NP_EXPECT(res.ok());
  const auto* decl = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(decl != nullptr);

  // Root must be '+'
  const auto* add = dynamic_cast<const BinaryExpression*>(decl->initializer());
  NP_EXPECT(add != nullptr);
  NP_EXPECT(add->op() == BinaryOperator::Add);

  // Right operand of '+' must be '*'
  const auto* mul = dynamic_cast<const BinaryExpression*>(add->right());
  NP_EXPECT(mul != nullptr);
  NP_EXPECT(mul->op() == BinaryOperator::Multiply);
}

// 16. Parentheses override precedence
void test_parser_parentheses() {
  auto res = parse_source("let x = (1 + 2) * 3;");
  NP_EXPECT(res.ok());
  const auto* decl = dynamic_cast<const VariableDeclaration*>(res.program->declarations()[0].get());
  NP_EXPECT(decl != nullptr);

  // Root must be '*'
  const auto* mul = dynamic_cast<const BinaryExpression*>(decl->initializer());
  NP_EXPECT(mul != nullptr);
  NP_EXPECT(mul->op() == BinaryOperator::Multiply);

  // Left operand must be GroupExpr
  const auto* group = dynamic_cast<const GroupExpression*>(mul->left());
  NP_EXPECT(group != nullptr);
}

// 17. Malformed function declaration
void test_parser_malformed_function_declaration() {
  // Missing parameter paren
  {
    auto res = parse_source("function foo(a, b { return 0; }");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(res.errors[0].construct == "function declaration");
    NP_EXPECT(!res.errors[0].expected_tokens.empty());
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::RightParen);
  }

  // Missing function name
  {
    auto res = parse_source("function () {}");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(!res.errors[0].expected_tokens.empty());
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::Identifier);
  }
}

// 18. Malformed variable declaration
void test_parser_malformed_variable_declaration() {
  // Missing '='
  {
    auto res = parse_source("let x 42;");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(res.errors[0].construct == "variable declaration");
    NP_EXPECT(!res.errors[0].expected_tokens.empty());
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::Equal);
  }

  // Missing ';'
  {
    auto res = parse_source("let x = 42");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(!res.errors[0].expected_tokens.empty());
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::Semicolon);
  }
}

// 19. Malformed return statement
void test_parser_malformed_return_statement() {
  auto res = parse_source("function f() { return 10 }");
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors[0].construct == "return statement");
  NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::Semicolon);
}

// 20. Malformed if statement
void test_parser_malformed_if_statement() {
  // Missing condition paren
  {
    auto res = parse_source("if x > 0 { return 1; }");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(res.errors[0].construct == "if statement");
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::LeftParen);
  }
}

// 21. Malformed while statement
void test_parser_malformed_while_statement() {
  // Missing ')'
  {
    auto res = parse_source("while (x > 0 { x = x - 1; }");
    NP_EXPECT(res.has_errors());
    NP_EXPECT(res.errors[0].construct == "while statement");
    NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::RightParen);
  }
}

// 22. Unexpected EOF
void test_parser_unexpected_eof() {
  auto res = parse_source("function foo(");
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors[0].token.type == TokenType::Eof);
}

// 23. Unexpected delimiter
void test_parser_unexpected_delimiter() {
  auto res = parse_source("let x = ) + 1;");
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors[0].token.type == TokenType::RightParen);
}

// 24. Missing delimiter
void test_parser_missing_delimiter() {
  // Unclosed block
  auto res = parse_source("function test() { let a = 1;");
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors[0].expected_tokens[0] == TokenType::RightBrace);
}

// 25. Invalid expression
void test_parser_invalid_expression() {
  auto res = parse_source("let a = * 5;");
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors[0].token.type == TokenType::Star);
}

void test_parser_all() {
  test_parser_empty_program();
  test_parser_simple_function();
  test_parser_function_parameters();
  test_parser_variable_declaration();
  test_parser_return_statement();
  test_parser_if_statement();
  test_parser_if_else_statement();
  test_parser_while_statement();
  test_parser_nested_blocks();
  test_parser_function_calls();
  test_parser_arithmetic_expressions();
  test_parser_comparison_expressions();
  test_parser_logical_expressions();
  test_parser_unary_expressions();
  test_parser_operator_precedence();
  test_parser_parentheses();
  test_parser_malformed_function_declaration();
  test_parser_malformed_variable_declaration();
  test_parser_malformed_return_statement();
  test_parser_malformed_if_statement();
  test_parser_malformed_while_statement();
  test_parser_unexpected_eof();
  test_parser_unexpected_delimiter();
  test_parser_missing_delimiter();
  test_parser_invalid_expression();
}
