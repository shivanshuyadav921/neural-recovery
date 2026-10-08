#include "test_harness.hpp"
#include "neuroparse/ast/ast.hpp"
#include "neuroparse/ast/ast_printer.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"

#include <algorithm>
#include <fstream>

using namespace neuroparse;
using namespace neuroparse::lexer;
using namespace neuroparse::parser;
using namespace neuroparse::ast;

namespace {

std::unique_ptr<Program> parse_program_ast(std::string_view source) {
  Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  Parser parser(std::move(lex_res.tokens));
  auto res = parser.parse();
  NP_EXPECT(res.ok());
  return std::move(res.program);
}

// Counting visitor to verify visitor pattern & tree traversal
class NodeCountVisitor : public ConstASTVisitor {
public:
  std::size_t count = 0;

  void visit(const Program& node) override {
    ++count;
    for (const auto& decl : node.declarations()) {
      if (decl) decl->accept(*this);
    }
  }

  void visit(const FunctionDeclaration& node) override {
    ++count;
    if (node.body()) node.body()->accept(*this);
  }

  void visit(const VariableDeclaration& node) override {
    ++count;
    if (node.initializer()) node.initializer()->accept(*this);
  }

  void visit(const AssignmentStatement& node) override {
    ++count;
    if (node.value()) node.value()->accept(*this);
  }

  void visit(const ReturnStatement& node) override {
    ++count;
    if (node.value()) node.value()->accept(*this);
  }

  void visit(const IfStatement& node) override {
    ++count;
    if (node.condition()) node.condition()->accept(*this);
    if (node.then_branch()) node.then_branch()->accept(*this);
    if (node.else_branch()) node.else_branch()->accept(*this);
  }

  void visit(const WhileStatement& node) override {
    ++count;
    if (node.condition()) node.condition()->accept(*this);
    if (node.body()) node.body()->accept(*this);
  }

  void visit(const BlockStatement& node) override {
    ++count;
    for (const auto& stmt : node.statements()) {
      if (stmt) stmt->accept(*this);
    }
  }

  void visit(const ExpressionStatement& node) override {
    ++count;
    if (node.expression()) node.expression()->accept(*this);
  }

  void visit(const LiteralExpression&) override { ++count; }
  void visit(const IdentifierExpression&) override { ++count; }

  void visit(const UnaryExpression& node) override {
    ++count;
    if (node.operand()) node.operand()->accept(*this);
  }

  void visit(const BinaryExpression& node) override {
    ++count;
    if (node.left()) node.left()->accept(*this);
    if (node.right()) node.right()->accept(*this);
  }

  void visit(const CallExpression& node) override {
    ++count;
    if (node.callee()) node.callee()->accept(*this);
    for (const auto& arg : node.arguments()) {
      if (arg) arg->accept(*this);
    }
  }

  void visit(const GroupExpression& node) override {
    ++count;
    if (node.expression()) node.expression()->accept(*this);
  }
};

}  // namespace

// 1. Empty program
void test_ast_empty_program() {
  auto prog = parse_program_ast("");
  NP_EXPECT(prog != nullptr);
  NP_EXPECT(prog->kind() == ASTNodeKind::Program);
  NP_EXPECT(prog->declarations().empty());
}

// 2. Function declaration
void test_ast_function_declaration() {
  auto prog = parse_program_ast("function calculate() { return 1; }");
  NP_EXPECT(prog->declarations().size() == 1);

  const auto* fn = dynamic_cast<const FunctionDeclaration*>(prog->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  NP_EXPECT(fn->kind() == ASTNodeKind::FunctionDeclaration);
  NP_EXPECT(fn->name() == "calculate");
  NP_EXPECT(fn->parameters().empty());
  NP_EXPECT(fn->body() != nullptr);
  NP_EXPECT(fn->body()->statements().size() == 1);
}

// 3. Parameters
void test_ast_parameters() {
  auto prog = parse_program_ast("function add(first, second, third) {}");
  const auto* fn = dynamic_cast<const FunctionDeclaration*>(prog->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  NP_EXPECT(fn->parameters().size() == 3);
  NP_EXPECT(fn->parameters()[0].name == "first");
  NP_EXPECT(fn->parameters()[1].name == "second");
  NP_EXPECT(fn->parameters()[2].name == "third");
}

// 4. Variable declarations
void test_ast_variable_declarations() {
  auto prog = parse_program_ast("let answer = 42;");
  NP_EXPECT(prog->declarations().size() == 1);
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  NP_EXPECT(var != nullptr);
  NP_EXPECT(var->name() == "answer");
  NP_EXPECT(var->initializer() != nullptr);
}

// 5. Literals
void test_ast_literals() {
  auto prog = parse_program_ast("let i = 100; let f = 3.14; let s = \"neuro\"; let b = true; let b2 = false;");
  NP_EXPECT(prog->declarations().size() == 5);

  const auto* v0 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* l0 = dynamic_cast<const LiteralExpression*>(v0->initializer());
  NP_EXPECT(l0 != nullptr);
  NP_EXPECT(l0->is_int());
  NP_EXPECT(l0->as_int() == 100);

  const auto* v1 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[1].get());
  const auto* l1 = dynamic_cast<const LiteralExpression*>(v1->initializer());
  NP_EXPECT(l1 != nullptr);
  NP_EXPECT(l1->is_float());
  NP_EXPECT(l1->as_float() > 3.13 && l1->as_float() < 3.15);

  const auto* v2 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[2].get());
  const auto* l2 = dynamic_cast<const LiteralExpression*>(v2->initializer());
  NP_EXPECT(l2 != nullptr);
  NP_EXPECT(l2->is_string());
  NP_EXPECT(l2->as_string() == "neuro");

  const auto* v3 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[3].get());
  const auto* l3 = dynamic_cast<const LiteralExpression*>(v3->initializer());
  NP_EXPECT(l3 != nullptr);
  NP_EXPECT(l3->is_bool());
  NP_EXPECT(l3->as_bool() == true);

  const auto* v4 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[4].get());
  const auto* l4 = dynamic_cast<const LiteralExpression*>(v4->initializer());
  NP_EXPECT(l4 != nullptr);
  NP_EXPECT(l4->is_bool());
  NP_EXPECT(l4->as_bool() == false);
}

// 6. Identifiers
void test_ast_identifiers() {
  auto prog = parse_program_ast("let y = x;");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* id = dynamic_cast<const IdentifierExpression*>(var->initializer());
  NP_EXPECT(id != nullptr);
  NP_EXPECT(id->name() == "x");
}

// 7. Unary expressions
void test_ast_unary_expressions() {
  auto prog = parse_program_ast("let neg = -a; let not_val = !b; let pos = +c;");
  NP_EXPECT(prog->declarations().size() == 3);

  const auto* v0 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* u0 = dynamic_cast<const UnaryExpression*>(v0->initializer());
  NP_EXPECT(u0 != nullptr);
  NP_EXPECT(u0->op() == UnaryOperator::Minus);

  const auto* v1 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[1].get());
  const auto* u1 = dynamic_cast<const UnaryExpression*>(v1->initializer());
  NP_EXPECT(u1 != nullptr);
  NP_EXPECT(u1->op() == UnaryOperator::Not);

  const auto* v2 = dynamic_cast<const VariableDeclaration*>(prog->declarations()[2].get());
  const auto* u2 = dynamic_cast<const UnaryExpression*>(v2->initializer());
  NP_EXPECT(u2 != nullptr);
  NP_EXPECT(u2->op() == UnaryOperator::Plus);
}

// 8. Binary expressions
void test_ast_binary_expressions() {
  auto prog = parse_program_ast("let res = a + b;");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* bin = dynamic_cast<const BinaryExpression*>(var->initializer());
  NP_EXPECT(bin != nullptr);
  NP_EXPECT(bin->op() == BinaryOperator::Add);

  const auto* left = dynamic_cast<const IdentifierExpression*>(bin->left());
  NP_EXPECT(left != nullptr);
  NP_EXPECT(left->name() == "a");

  const auto* right = dynamic_cast<const IdentifierExpression*>(bin->right());
  NP_EXPECT(right != nullptr);
  NP_EXPECT(right->name() == "b");
}

// 9. Operator precedence in AST structure
void test_ast_operator_precedence() {
  // Verify 1 + 2 * 3 produces:
  // BinaryExpression(+) -> left: 1, right: BinaryExpression(*) -> left: 2, right: 3
  auto prog = parse_program_ast("let x = 1 + 2 * 3;");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* root_bin = dynamic_cast<const BinaryExpression*>(var->initializer());
  NP_EXPECT(root_bin != nullptr);
  NP_EXPECT(root_bin->op() == BinaryOperator::Add);

  const auto* left_lit = dynamic_cast<const LiteralExpression*>(root_bin->left());
  NP_EXPECT(left_lit != nullptr);
  NP_EXPECT(left_lit->as_int() == 1);

  const auto* right_bin = dynamic_cast<const BinaryExpression*>(root_bin->right());
  NP_EXPECT(right_bin != nullptr);
  NP_EXPECT(right_bin->op() == BinaryOperator::Multiply);

  const auto* mul_l = dynamic_cast<const LiteralExpression*>(right_bin->left());
  NP_EXPECT(mul_l != nullptr);
  NP_EXPECT(mul_l->as_int() == 2);

  const auto* mul_r = dynamic_cast<const LiteralExpression*>(right_bin->right());
  NP_EXPECT(mul_r != nullptr);
  NP_EXPECT(mul_r->as_int() == 3);
}

// 10. Nested expressions with parentheses
void test_ast_nested_expressions() {
  // Verify (1 + 2) * 3 produces:
  // BinaryExpression(*) -> left: GroupExpression(BinaryExpression(+)), right: 3
  auto prog = parse_program_ast("let x = (1 + 2) * 3;");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* root_bin = dynamic_cast<const BinaryExpression*>(var->initializer());
  NP_EXPECT(root_bin != nullptr);
  NP_EXPECT(root_bin->op() == BinaryOperator::Multiply);

  const auto* grp = dynamic_cast<const GroupExpression*>(root_bin->left());
  NP_EXPECT(grp != nullptr);
  const auto* inner_bin = dynamic_cast<const BinaryExpression*>(grp->expression());
  NP_EXPECT(inner_bin != nullptr);
  NP_EXPECT(inner_bin->op() == BinaryOperator::Add);
}

// 11. Return statement
void test_ast_return_statement() {
  auto prog = parse_program_ast("return 100; return;");
  NP_EXPECT(prog->declarations().size() == 2);

  const auto* r0 = dynamic_cast<const ReturnStatement*>(prog->declarations()[0].get());
  NP_EXPECT(r0 != nullptr);
  NP_EXPECT(r0->value() != nullptr);

  const auto* r1 = dynamic_cast<const ReturnStatement*>(prog->declarations()[1].get());
  NP_EXPECT(r1 != nullptr);
  NP_EXPECT(r1->value() == nullptr);
}

// 12. If statement
void test_ast_if_statement() {
  auto prog = parse_program_ast("if (valid) { return 1; }");
  const auto* if_stmt = dynamic_cast<const IfStatement*>(prog->declarations()[0].get());
  NP_EXPECT(if_stmt != nullptr);
  NP_EXPECT(if_stmt->condition() != nullptr);
  NP_EXPECT(if_stmt->then_branch() != nullptr);
  NP_EXPECT(if_stmt->else_branch() == nullptr);
}

// 13. If/Else statement
void test_ast_if_else_statement() {
  auto prog = parse_program_ast("if (a > b) { return a; } else { return b; }");
  const auto* if_stmt = dynamic_cast<const IfStatement*>(prog->declarations()[0].get());
  NP_EXPECT(if_stmt != nullptr);
  NP_EXPECT(if_stmt->then_branch() != nullptr);
  NP_EXPECT(if_stmt->else_branch() != nullptr);
}

// 14. While statement
void test_ast_while_statement() {
  auto prog = parse_program_ast("while (i < 10) { i = i + 1; }");
  const auto* while_stmt = dynamic_cast<const WhileStatement*>(prog->declarations()[0].get());
  NP_EXPECT(while_stmt != nullptr);
  NP_EXPECT(while_stmt->condition() != nullptr);
  NP_EXPECT(while_stmt->body() != nullptr);
}

// 15. Blocks
void test_ast_blocks() {
  auto prog = parse_program_ast("{ let a = 1; let b = 2; }");
  const auto* blk = dynamic_cast<const BlockStatement*>(prog->declarations()[0].get());
  NP_EXPECT(blk != nullptr);
  NP_EXPECT(blk->statements().size() == 2);
}

// 16. Function calls
void test_ast_function_calls() {
  auto prog = parse_program_ast("let r = foo(1, 2, 3);");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* call = dynamic_cast<const CallExpression*>(var->initializer());
  NP_EXPECT(call != nullptr);
  const auto* callee = dynamic_cast<const IdentifierExpression*>(call->callee());
  NP_EXPECT(callee != nullptr);
  NP_EXPECT(callee->name() == "foo");
  NP_EXPECT(call->arguments().size() == 3);
}

// 17. Nested function calls
void test_ast_nested_function_calls() {
  auto prog = parse_program_ast("let r = f(g(h()));");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  const auto* call_f = dynamic_cast<const CallExpression*>(var->initializer());
  NP_EXPECT(call_f != nullptr);
  const auto* call_g = dynamic_cast<const CallExpression*>(call_f->arguments()[0].get());
  NP_EXPECT(call_g != nullptr);
  const auto* call_h = dynamic_cast<const CallExpression*>(call_g->arguments()[0].get());
  NP_EXPECT(call_h != nullptr);
  NP_EXPECT(call_h->arguments().empty());
}

// 18. Nested statements
void test_ast_nested_statements() {
  auto prog = parse_program_ast(
      "function outer() {\n"
      "    if (true) {\n"
      "        while (false) {\n"
      "            return 42;\n"
      "        }\n"
      "    }\n"
      "}\n");
  const auto* fn = dynamic_cast<const FunctionDeclaration*>(prog->declarations()[0].get());
  NP_EXPECT(fn != nullptr);
  const auto* if_s = dynamic_cast<const IfStatement*>(fn->body()->statements()[0].get());
  NP_EXPECT(if_s != nullptr);
  const auto* then_b = dynamic_cast<const BlockStatement*>(if_s->then_branch());
  NP_EXPECT(then_b != nullptr);
  const auto* while_s = dynamic_cast<const WhileStatement*>(then_b->statements()[0].get());
  NP_EXPECT(while_s != nullptr);
}

// 19. Source locations
void test_ast_source_locations() {
  auto prog = parse_program_ast("let alpha = 100;");
  const auto* var = dynamic_cast<const VariableDeclaration*>(prog->declarations()[0].get());
  NP_EXPECT(var != nullptr);
  NP_EXPECT(var->span().line() == 1);
  NP_EXPECT(var->span().column() == 1);
  NP_EXPECT(var->span().offset() == 0);
  NP_EXPECT(var->span().length() > 0);
}

// 20. AST ownership / lifetime / visitor
void test_ast_ownership_and_visitor() {
  auto prog = parse_program_ast(
      "function add(a, b) {\n"
      "    return a + b;\n"
      "}\n"
      "let x = add(1, 2);\n");
  NP_EXPECT(prog != nullptr);

  NodeCountVisitor visitor;
  prog->accept(visitor);
  NP_EXPECT(visitor.count > 0);
}

// 21. AST pretty printing
void test_ast_pretty_printing() {
  auto prog = parse_program_ast(
      "function add(a, b) {\n"
      "    return a + b;\n"
      "}\n"
      "let x = add(1, 2);\n");
  std::string tree = dump_ast(*prog);

  NP_EXPECT(!tree.empty());
  NP_EXPECT(tree.find("Program") != std::string::npos);
  NP_EXPECT(tree.find("FunctionDeclaration: add(a, b)") != std::string::npos);
  NP_EXPECT(tree.find("ReturnStatement") != std::string::npos);
  NP_EXPECT(tree.find("BinaryExpression: +") != std::string::npos);
  NP_EXPECT(tree.find("VariableDeclaration: x") != std::string::npos);
  NP_EXPECT(tree.find("CallExpression") != std::string::npos);
}

// 22. AST golden snapshot test
void test_ast_snapshot_golden() {
  auto prog = parse_program_ast(
      "function add(a, b) {\n"
      "    return a + b;\n"
      "}\n"
      "let x = add(1, 2);\n");
  std::string tree = dump_ast(*prog);

  std::string snapshot_path = "examples/expected_ast/hello.ast";
  if (!std::ifstream(snapshot_path).good() && std::ifstream("../examples/expected_ast/hello.ast").good()) {
    snapshot_path = "../examples/expected_ast/hello.ast";
  }

  std::ifstream f(snapshot_path, std::ios::binary);
  if (f.is_open()) {
    std::string expected((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Normalize newlines for comparison
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    std::string actual = tree;
    actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
    NP_EXPECT(actual == expected);
  }
}

void test_ast_all() {
  test_ast_empty_program();
  test_ast_function_declaration();
  test_ast_parameters();
  test_ast_variable_declarations();
  test_ast_literals();
  test_ast_identifiers();
  test_ast_unary_expressions();
  test_ast_binary_expressions();
  test_ast_operator_precedence();
  test_ast_nested_expressions();
  test_ast_return_statement();
  test_ast_if_statement();
  test_ast_if_else_statement();
  test_ast_while_statement();
  test_ast_blocks();
  test_ast_function_calls();
  test_ast_nested_function_calls();
  test_ast_nested_statements();
  test_ast_source_locations();
  test_ast_ownership_and_visitor();
  test_ast_pretty_printing();
  test_ast_snapshot_golden();
}
