#pragma once

#include "neuroparse/ast/ast_visitor.hpp"
#include "neuroparse/ast/source_span.hpp"
#include "neuroparse/lexer/token.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace neuroparse::ast {

enum class BinaryOperator {
  Add,
  Subtract,
  Multiply,
  Divide,
  Modulo,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  LogicalAnd,
  LogicalOr,
};

enum class UnaryOperator {
  Plus,
  Minus,
  Not,
};

constexpr std::string_view to_string(BinaryOperator op) noexcept {
  switch (op) {
    case BinaryOperator::Add:          return "+";
    case BinaryOperator::Subtract:     return "-";
    case BinaryOperator::Multiply:     return "*";
    case BinaryOperator::Divide:       return "/";
    case BinaryOperator::Modulo:       return "%";
    case BinaryOperator::Equal:        return "==";
    case BinaryOperator::NotEqual:     return "!=";
    case BinaryOperator::Less:         return "<";
    case BinaryOperator::LessEqual:    return "<=";
    case BinaryOperator::Greater:      return ">";
    case BinaryOperator::GreaterEqual: return ">=";
    case BinaryOperator::LogicalAnd:   return "&&";
    case BinaryOperator::LogicalOr:    return "||";
    default:                           return "UNKNOWN_BINARY_OP";
  }
}

constexpr std::string_view to_string(UnaryOperator op) noexcept {
  switch (op) {
    case UnaryOperator::Plus:  return "+";
    case UnaryOperator::Minus: return "-";
    case UnaryOperator::Not:   return "!";
    default:                   return "UNKNOWN_UNARY_OP";
  }
}

constexpr std::optional<BinaryOperator> token_to_binary_op(lexer::TokenType type) noexcept {
  switch (type) {
    case lexer::TokenType::Plus:         return BinaryOperator::Add;
    case lexer::TokenType::Minus:        return BinaryOperator::Subtract;
    case lexer::TokenType::Star:         return BinaryOperator::Multiply;
    case lexer::TokenType::Slash:        return BinaryOperator::Divide;
    case lexer::TokenType::Percent:      return BinaryOperator::Modulo;
    case lexer::TokenType::EqualEqual:   return BinaryOperator::Equal;
    case lexer::TokenType::BangEqual:    return BinaryOperator::NotEqual;
    case lexer::TokenType::Less:         return BinaryOperator::Less;
    case lexer::TokenType::LessEqual:    return BinaryOperator::LessEqual;
    case lexer::TokenType::Greater:      return BinaryOperator::Greater;
    case lexer::TokenType::GreaterEqual: return BinaryOperator::GreaterEqual;
    case lexer::TokenType::AmpAmp:       return BinaryOperator::LogicalAnd;
    case lexer::TokenType::PipePipe:     return BinaryOperator::LogicalOr;
    default:                             return std::nullopt;
  }
}

constexpr std::optional<UnaryOperator> token_to_unary_op(lexer::TokenType type) noexcept {
  switch (type) {
    case lexer::TokenType::Plus:  return UnaryOperator::Plus;
    case lexer::TokenType::Minus: return UnaryOperator::Minus;
    case lexer::TokenType::Bang:  return UnaryOperator::Not;
    default:                      return std::nullopt;
  }
}

enum class LiteralType {
  Integer,
  Float,
  String,
  Boolean,
};

using LiteralValue = std::variant<std::int64_t, double, std::string, bool>;

enum class ASTNodeKind {
  Program,
  FunctionDeclaration,
  VariableDeclaration,
  AssignmentStatement,
  ReturnStatement,
  IfStatement,
  WhileStatement,
  BlockStatement,
  ExpressionStatement,
  LiteralExpression,
  IdentifierExpression,
  UnaryExpression,
  BinaryExpression,
  CallExpression,
  GroupExpression,
};

class ASTNode {
public:
  virtual ~ASTNode() = default;

  [[nodiscard]] virtual ASTNodeKind kind() const noexcept = 0;
  [[nodiscard]] virtual SourceSpan span() const noexcept = 0;
  [[nodiscard]] lexer::SourceLocation location() const noexcept { return span().start; }

  virtual void accept(ASTVisitor& visitor) = 0;
  virtual void accept(ConstASTVisitor& visitor) const = 0;
};

// Category base classes

class Declaration : public ASTNode {
public:
  ~Declaration() override = default;
};

class Statement : public ASTNode {
public:
  ~Statement() override = default;
};

class Expression : public ASTNode {
public:
  ~Expression() override = default;
};

// Expression nodes

class LiteralExpression : public Expression {
public:
  LiteralExpression(LiteralValue value, LiteralType type, SourceSpan span)
      : value_(std::move(value)), type_(type), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::LiteralExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }

  [[nodiscard]] const LiteralValue& value() const noexcept { return value_; }
  [[nodiscard]] LiteralType literal_type() const noexcept { return type_; }

  [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<std::int64_t>(value_); }
  [[nodiscard]] bool is_float() const noexcept { return std::holds_alternative<double>(value_); }
  [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
  [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }

  [[nodiscard]] std::int64_t as_int() const { return std::get<std::int64_t>(value_); }
  [[nodiscard]] double as_float() const { return std::get<double>(value_); }
  [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(value_); }
  [[nodiscard]] bool as_bool() const { return std::get<bool>(value_); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  LiteralValue value_;
  LiteralType type_;
  SourceSpan span_;
};

class IdentifierExpression : public Expression {
public:
  IdentifierExpression(std::string name, SourceSpan span)
      : name_(std::move(name)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::IdentifierExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::string name_;
  SourceSpan span_;
};

class UnaryExpression : public Expression {
public:
  UnaryExpression(UnaryOperator op, std::unique_ptr<Expression> operand, SourceSpan span)
      : op_(op), operand_(std::move(operand)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::UnaryExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] UnaryOperator op() const noexcept { return op_; }
  [[nodiscard]] const Expression* operand() const noexcept { return operand_.get(); }
  [[nodiscard]] Expression* operand() noexcept { return operand_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  UnaryOperator op_;
  std::unique_ptr<Expression> operand_;
  SourceSpan span_;
};

class BinaryExpression : public Expression {
public:
  BinaryExpression(std::unique_ptr<Expression> left, BinaryOperator op, std::unique_ptr<Expression> right, SourceSpan span)
      : left_(std::move(left)), op_(op), right_(std::move(right)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::BinaryExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] BinaryOperator op() const noexcept { return op_; }
  [[nodiscard]] const Expression* left() const noexcept { return left_.get(); }
  [[nodiscard]] Expression* left() noexcept { return left_.get(); }
  [[nodiscard]] const Expression* right() const noexcept { return right_.get(); }
  [[nodiscard]] Expression* right() noexcept { return right_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> left_;
  BinaryOperator op_;
  std::unique_ptr<Expression> right_;
  SourceSpan span_;
};

class CallExpression : public Expression {
public:
  CallExpression(std::unique_ptr<Expression> callee, std::vector<std::unique_ptr<Expression>> arguments, SourceSpan span)
      : callee_(std::move(callee)), arguments_(std::move(arguments)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::CallExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* callee() const noexcept { return callee_.get(); }
  [[nodiscard]] Expression* callee() noexcept { return callee_.get(); }
  [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& arguments() const noexcept { return arguments_; }
  [[nodiscard]] std::vector<std::unique_ptr<Expression>>& arguments() noexcept { return arguments_; }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> callee_;
  std::vector<std::unique_ptr<Expression>> arguments_;
  SourceSpan span_;
};

class GroupExpression : public Expression {
public:
  GroupExpression(std::unique_ptr<Expression> expression, SourceSpan span)
      : expression_(std::move(expression)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::GroupExpression; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* expression() const noexcept { return expression_.get(); }
  [[nodiscard]] Expression* expression() noexcept { return expression_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> expression_;
  SourceSpan span_;
};

// Statement & Declaration nodes

class VariableDeclaration : public Statement {
public:
  VariableDeclaration(std::string name, std::unique_ptr<Expression> initializer, SourceSpan span)
      : name_(std::move(name)), initializer_(std::move(initializer)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::VariableDeclaration; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const Expression* initializer() const noexcept { return initializer_.get(); }
  [[nodiscard]] Expression* initializer() noexcept { return initializer_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::string name_;
  std::unique_ptr<Expression> initializer_;
  SourceSpan span_;
};

class AssignmentStatement : public Statement {
public:
  AssignmentStatement(std::string name, std::unique_ptr<Expression> value, SourceSpan span)
      : name_(std::move(name)), value_(std::move(value)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::AssignmentStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const Expression* value() const noexcept { return value_.get(); }
  [[nodiscard]] Expression* value() noexcept { return value_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::string name_;
  std::unique_ptr<Expression> value_;
  SourceSpan span_;
};

class ReturnStatement : public Statement {
public:
  ReturnStatement(std::unique_ptr<Expression> value, SourceSpan span)
      : value_(std::move(value)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::ReturnStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* value() const noexcept { return value_.get(); }
  [[nodiscard]] Expression* value() noexcept { return value_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> value_;
  SourceSpan span_;
};

class BlockStatement;

class IfStatement : public Statement {
public:
  IfStatement(std::unique_ptr<Expression> condition,
              std::unique_ptr<Statement> then_branch,
              std::unique_ptr<Statement> else_branch,
              SourceSpan span)
      : condition_(std::move(condition)),
        then_branch_(std::move(then_branch)),
        else_branch_(std::move(else_branch)),
        span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::IfStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* condition() const noexcept { return condition_.get(); }
  [[nodiscard]] Expression* condition() noexcept { return condition_.get(); }
  [[nodiscard]] const Statement* then_branch() const noexcept { return then_branch_.get(); }
  [[nodiscard]] Statement* then_branch() noexcept { return then_branch_.get(); }
  [[nodiscard]] const Statement* else_branch() const noexcept { return else_branch_.get(); }
  [[nodiscard]] Statement* else_branch() noexcept { return else_branch_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> condition_;
  std::unique_ptr<Statement> then_branch_;
  std::unique_ptr<Statement> else_branch_;
  SourceSpan span_;
};

class WhileStatement : public Statement {
public:
  WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> body, SourceSpan span)
      : condition_(std::move(condition)), body_(std::move(body)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::WhileStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* condition() const noexcept { return condition_.get(); }
  [[nodiscard]] Expression* condition() noexcept { return condition_.get(); }
  [[nodiscard]] const Statement* body() const noexcept { return body_.get(); }
  [[nodiscard]] Statement* body() noexcept { return body_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> condition_;
  std::unique_ptr<Statement> body_;
  SourceSpan span_;
};

class BlockStatement : public Statement {
public:
  BlockStatement(std::vector<std::unique_ptr<Statement>> statements, SourceSpan span)
      : statements_(std::move(statements)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::BlockStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Statement>>& statements() const noexcept { return statements_; }
  [[nodiscard]] std::vector<std::unique_ptr<Statement>>& statements() noexcept { return statements_; }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::vector<std::unique_ptr<Statement>> statements_;
  SourceSpan span_;
};

class ExpressionStatement : public Statement {
public:
  ExpressionStatement(std::unique_ptr<Expression> expression, SourceSpan span)
      : expression_(std::move(expression)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::ExpressionStatement; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const Expression* expression() const noexcept { return expression_.get(); }
  [[nodiscard]] Expression* expression() noexcept { return expression_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::unique_ptr<Expression> expression_;
  SourceSpan span_;
};

struct FunctionParameter {
  std::string name;
  SourceSpan span;

  constexpr bool operator==(const FunctionParameter&) const = default;
};

class FunctionDeclaration : public Declaration {
public:
  FunctionDeclaration(std::string name,
                      std::vector<FunctionParameter> parameters,
                      std::unique_ptr<BlockStatement> body,
                      SourceSpan span)
      : name_(std::move(name)),
        parameters_(std::move(parameters)),
        body_(std::move(body)),
        span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::FunctionDeclaration; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<FunctionParameter>& parameters() const noexcept { return parameters_; }
  [[nodiscard]] const BlockStatement* body() const noexcept { return body_.get(); }
  [[nodiscard]] BlockStatement* body() noexcept { return body_.get(); }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::string name_;
  std::vector<FunctionParameter> parameters_;
  std::unique_ptr<BlockStatement> body_;
  SourceSpan span_;
};

// Root Program node

class Program : public ASTNode {
public:
  Program(std::vector<std::unique_ptr<ASTNode>> declarations, SourceSpan span)
      : declarations_(std::move(declarations)), span_(span) {}

  [[nodiscard]] ASTNodeKind kind() const noexcept override { return ASTNodeKind::Program; }
  [[nodiscard]] SourceSpan span() const noexcept override { return span_; }
  [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& declarations() const noexcept { return declarations_; }
  [[nodiscard]] std::vector<std::unique_ptr<ASTNode>>& declarations() noexcept { return declarations_; }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
  void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

private:
  std::vector<std::unique_ptr<ASTNode>> declarations_;
  SourceSpan span_;
};

}  // namespace neuroparse::ast
