#include "neuroparse/fuzz/invariants.hpp"

#include <sstream>

namespace neuroparse::fuzz {

InvariantResult check_token_stream_invariants(const std::vector<lexer::Token>& tokens) {
  InvariantResult result;

  if (tokens.empty()) {
    result.add_violation("Token stream is empty (must contain at least EOF).");
    return result;
  }

  // Check EOF token at end
  if (tokens.back().type != lexer::TokenType::Eof) {
    result.add_violation("Token stream does not end with TokenType::Eof.");
  }

  // Check no internal EOF tokens
  for (std::size_t i = 0; i < tokens.size() - 1; ++i) {
    if (tokens[i].type == lexer::TokenType::Eof) {
      result.add_violation("Internal TokenType::Eof found at index " + std::to_string(i));
    }
  }

  // Check token locations and monotonicity
  lexer::SourceLocation prev_loc{0, 1, 1};
  bool has_prev = false;

  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const auto& tok = tokens[i];

    if (tok.type == lexer::TokenType::Invalid) {
      result.add_violation("Invalid token type encountered at index " + std::to_string(i));
    }

    const auto& loc = tok.location;
    if (loc.line == 0 || loc.column == 0) {
      result.add_violation("Invalid 0 line or column at index " + std::to_string(i));
    }

    // Monotonicity of token positions
    if (has_prev) {
      if (loc.line < prev_loc.line ||
          (loc.line == prev_loc.line && loc.column < prev_loc.column)) {
        result.add_violation("Non-monotonic token start position at index " + std::to_string(i));
      }
    }
    prev_loc = loc;
    has_prev = true;
  }

  return result;
}

namespace {

class ASTInvariantChecker : public ast::ConstASTVisitor {
public:
  explicit ASTInvariantChecker(InvariantResult& res) : result_(res) {}

  void check_span(const ast::SourceSpan& span, const std::string& node_desc) {
    if (span.start.line > span.end.line ||
        (span.start.line == span.end.line && span.start.column > span.end.column)) {
      result_.add_violation("Inverted source span in " + node_desc + " (" +
                            std::to_string(span.start.line) + ":" + std::to_string(span.start.column) + " .. " +
                            std::to_string(span.end.line) + ":" + std::to_string(span.end.column) + ")");
    }
  }

  void visit(const ast::Program& node) override {
    check_span(node.span(), "Program");
    for (std::size_t i = 0; i < node.declarations().size(); ++i) {
      const auto& decl = node.declarations()[i];
      if (!decl) {
        result_.add_violation("Null declaration pointer in Program at index " + std::to_string(i));
      } else {
        decl->accept(*this);
      }
    }
  }

  void visit(const ast::FunctionDeclaration& node) override {
    check_span(node.span(), "FunctionDeclaration");
    if (node.name().empty()) {
      result_.add_violation("Empty function name in FunctionDeclaration");
    }
    if (!node.body()) {
      result_.add_violation("Null body in FunctionDeclaration " + node.name());
    } else {
      node.body()->accept(*this);
    }
  }

  void visit(const ast::VariableDeclaration& node) override {
    check_span(node.span(), "VariableDeclaration");
    if (node.name().empty()) {
      result_.add_violation("Empty variable name in VariableDeclaration");
    }
    if (node.initializer()) {
      node.initializer()->accept(*this);
    }
  }

  void visit(const ast::AssignmentStatement& node) override {
    check_span(node.span(), "AssignmentStatement");
    if (node.name().empty()) {
      result_.add_violation("Empty variable name in AssignmentStatement");
    }
    if (!node.value()) {
      result_.add_violation("Null value expression in AssignmentStatement");
    } else {
      node.value()->accept(*this);
    }
  }

  void visit(const ast::ReturnStatement& node) override {
    check_span(node.span(), "ReturnStatement");
    if (node.value()) {
      node.value()->accept(*this);
    }
  }

  void visit(const ast::IfStatement& node) override {
    check_span(node.span(), "IfStatement");
    if (!node.condition()) {
      result_.add_violation("Null condition in IfStatement");
    } else {
      node.condition()->accept(*this);
    }
    if (!node.then_branch()) {
      result_.add_violation("Null then_branch in IfStatement");
    } else {
      node.then_branch()->accept(*this);
    }
    if (node.else_branch()) {
      node.else_branch()->accept(*this);
    }
  }

  void visit(const ast::WhileStatement& node) override {
    check_span(node.span(), "WhileStatement");
    if (!node.condition()) {
      result_.add_violation("Null condition in WhileStatement");
    } else {
      node.condition()->accept(*this);
    }
    if (!node.body()) {
      result_.add_violation("Null body in WhileStatement");
    } else {
      node.body()->accept(*this);
    }
  }

  void visit(const ast::BlockStatement& node) override {
    check_span(node.span(), "BlockStatement");
    for (std::size_t i = 0; i < node.statements().size(); ++i) {
      const auto& stmt = node.statements()[i];
      if (!stmt) {
        result_.add_violation("Null statement in BlockStatement at index " + std::to_string(i));
      } else {
        stmt->accept(*this);
      }
    }
  }

  void visit(const ast::ExpressionStatement& node) override {
    check_span(node.span(), "ExpressionStatement");
    if (!node.expression()) {
      result_.add_violation("Null expression in ExpressionStatement");
    } else {
      node.expression()->accept(*this);
    }
  }

  void visit(const ast::LiteralExpression& node) override {
    check_span(node.span(), "LiteralExpression");
  }

  void visit(const ast::IdentifierExpression& node) override {
    check_span(node.span(), "IdentifierExpression");
    if (node.name().empty()) {
      result_.add_violation("Empty identifier name in IdentifierExpression");
    }
  }

  void visit(const ast::UnaryExpression& node) override {
    check_span(node.span(), "UnaryExpression");
    if (!node.operand()) {
      result_.add_violation("Null operand in UnaryExpression");
    } else {
      node.operand()->accept(*this);
    }
  }

  void visit(const ast::BinaryExpression& node) override {
    check_span(node.span(), "BinaryExpression");
    if (!node.left()) {
      result_.add_violation("Null left operand in BinaryExpression");
    } else {
      node.left()->accept(*this);
    }
    if (!node.right()) {
      result_.add_violation("Null right operand in BinaryExpression");
    } else {
      node.right()->accept(*this);
    }
  }

  void visit(const ast::CallExpression& node) override {
    check_span(node.span(), "CallExpression");
    if (!node.callee()) {
      result_.add_violation("Null callee in CallExpression");
    } else {
      node.callee()->accept(*this);
    }
    for (std::size_t i = 0; i < node.arguments().size(); ++i) {
      const auto& arg = node.arguments()[i];
      if (!arg) {
        result_.add_violation("Null argument in CallExpression at index " + std::to_string(i));
      } else {
        arg->accept(*this);
      }
    }
  }

  void visit(const ast::GroupExpression& node) override {
    check_span(node.span(), "GroupExpression");
    if (!node.expression()) {
      result_.add_violation("Null inner expression in GroupExpression");
    } else {
      node.expression()->accept(*this);
    }
  }

private:
  InvariantResult& result_;
};

}  // namespace

InvariantResult check_ast_invariants(const ast::ASTNode* root) {
  InvariantResult result;
  if (!root) {
    result.add_violation("Root AST node is null");
    return result;
  }

  ASTInvariantChecker checker(result);
  root->accept(checker);
  return result;
}

}  // namespace neuroparse::fuzz
