#pragma once

namespace neuroparse::ast {

// Forward declarations of AST nodes
class Program;
class FunctionDeclaration;
class VariableDeclaration;
class AssignmentStatement;
class ReturnStatement;
class IfStatement;
class WhileStatement;
class BlockStatement;
class ExpressionStatement;
class LiteralExpression;
class IdentifierExpression;
class UnaryExpression;
class BinaryExpression;
class CallExpression;
class GroupExpression;

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  virtual void visit(Program& node) = 0;
  virtual void visit(FunctionDeclaration& node) = 0;
  virtual void visit(VariableDeclaration& node) = 0;
  virtual void visit(AssignmentStatement& node) = 0;
  virtual void visit(ReturnStatement& node) = 0;
  virtual void visit(IfStatement& node) = 0;
  virtual void visit(WhileStatement& node) = 0;
  virtual void visit(BlockStatement& node) = 0;
  virtual void visit(ExpressionStatement& node) = 0;
  virtual void visit(LiteralExpression& node) = 0;
  virtual void visit(IdentifierExpression& node) = 0;
  virtual void visit(UnaryExpression& node) = 0;
  virtual void visit(BinaryExpression& node) = 0;
  virtual void visit(CallExpression& node) = 0;
  virtual void visit(GroupExpression& node) = 0;
};

class ConstASTVisitor {
public:
  virtual ~ConstASTVisitor() = default;

  virtual void visit(const Program& node) = 0;
  virtual void visit(const FunctionDeclaration& node) = 0;
  virtual void visit(const VariableDeclaration& node) = 0;
  virtual void visit(const AssignmentStatement& node) = 0;
  virtual void visit(const ReturnStatement& node) = 0;
  virtual void visit(const IfStatement& node) = 0;
  virtual void visit(const WhileStatement& node) = 0;
  virtual void visit(const BlockStatement& node) = 0;
  virtual void visit(const ExpressionStatement& node) = 0;
  virtual void visit(const LiteralExpression& node) = 0;
  virtual void visit(const IdentifierExpression& node) = 0;
  virtual void visit(const UnaryExpression& node) = 0;
  virtual void visit(const BinaryExpression& node) = 0;
  virtual void visit(const CallExpression& node) = 0;
  virtual void visit(const GroupExpression& node) = 0;
};

}  // namespace neuroparse::ast
