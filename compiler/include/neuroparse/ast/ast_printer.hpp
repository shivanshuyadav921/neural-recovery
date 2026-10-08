#pragma once

#include "neuroparse/ast/ast.hpp"
#include "neuroparse/ast/ast_visitor.hpp"

#include <string>
#include <vector>

namespace neuroparse::ast {

class ASTPrinter : public ConstASTVisitor {
public:
  ASTPrinter() = default;

  [[nodiscard]] std::string print(const ASTNode& root);

  void visit(const Program& node) override;
  void visit(const FunctionDeclaration& node) override;
  void visit(const VariableDeclaration& node) override;
  void visit(const AssignmentStatement& node) override;
  void visit(const ReturnStatement& node) override;
  void visit(const IfStatement& node) override;
  void visit(const WhileStatement& node) override;
  void visit(const BlockStatement& node) override;
  void visit(const ExpressionStatement& node) override;
  void visit(const LiteralExpression& node) override;
  void visit(const IdentifierExpression& node) override;
  void visit(const UnaryExpression& node) override;
  void visit(const BinaryExpression& node) override;
  void visit(const CallExpression& node) override;
  void visit(const GroupExpression& node) override;

private:
  void append_indent();
  void append_line(std::string_view text);

  std::string output_;
  std::vector<bool> indent_stack_;
};

[[nodiscard]] std::string dump_ast(const ASTNode& root);

}  // namespace neuroparse::ast
