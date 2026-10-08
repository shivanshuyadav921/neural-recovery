#include "neuroparse/ast/ast_printer.hpp"

#include <sstream>

namespace neuroparse::ast {

namespace {

struct Frame {
  bool has_next = false;
};

}  // namespace

class ASTPrinterImpl : public ConstASTVisitor {
public:
  std::string print(const ASTNode& root) {
    output_.clear();
    indent_.clear();
    root.accept(*this);
    return output_;
  }

  void visit(const Program& node) override {
    print_header("Program");
    const auto& decls = node.declarations();
    for (std::size_t i = 0; i < decls.size(); ++i) {
      if (decls[i]) {
        bool has_next = (i + 1 < decls.size());
        print_child(decls[i].get(), has_next);
      }
    }
  }

  void visit(const FunctionDeclaration& node) override {
    std::string text = "FunctionDeclaration: " + node.name() + "(";
    for (std::size_t i = 0; i < node.parameters().size(); ++i) {
      if (i > 0) text += ", ";
      text += node.parameters()[i].name;
    }
    text += ")";
    print_header(text);

    if (node.body()) {
      print_child(node.body(), false);
    }
  }

  void visit(const VariableDeclaration& node) override {
    print_header("VariableDeclaration: " + node.name());
    if (node.initializer()) {
      print_child(node.initializer(), false);
    }
  }

  void visit(const AssignmentStatement& node) override {
    print_header("AssignmentStatement: " + node.name());
    if (node.value()) {
      print_child(node.value(), false);
    }
  }

  void visit(const ReturnStatement& node) override {
    print_header("ReturnStatement");
    if (node.value()) {
      print_child(node.value(), false);
    }
  }

  void visit(const IfStatement& node) override {
    print_header("IfStatement");
    bool has_else = (node.else_branch() != nullptr);
    if (node.condition()) {
      print_child(node.condition(), true);
    }
    if (node.then_branch()) {
      print_child(node.then_branch(), has_else);
    }
    if (has_else) {
      print_child(node.else_branch(), false);
    }
  }

  void visit(const WhileStatement& node) override {
    print_header("WhileStatement");
    if (node.condition()) {
      print_child(node.condition(), true);
    }
    if (node.body()) {
      print_child(node.body(), false);
    }
  }

  void visit(const BlockStatement& node) override {
    print_header("BlockStatement");
    const auto& stmts = node.statements();
    for (std::size_t i = 0; i < stmts.size(); ++i) {
      if (stmts[i]) {
        bool has_next = (i + 1 < stmts.size());
        print_child(stmts[i].get(), has_next);
      }
    }
  }

  void visit(const ExpressionStatement& node) override {
    print_header("ExpressionStatement");
    if (node.expression()) {
      print_child(node.expression(), false);
    }
  }

  void visit(const LiteralExpression& node) override {
    std::string text = "Literal: ";
    if (node.is_int()) {
      text += std::to_string(node.as_int()) + " (Integer)";
    } else if (node.is_float()) {
      std::ostringstream ss;
      ss << node.as_float();
      text += ss.str() + " (Float)";
    } else if (node.is_string()) {
      text += "\"" + node.as_string() + "\" (String)";
    } else if (node.is_bool()) {
      text += (node.as_bool() ? "true" : "false") + std::string(" (Boolean)");
    }
    print_header(text);
  }

  void visit(const IdentifierExpression& node) override {
    print_header("Identifier: " + node.name());
  }

  void visit(const UnaryExpression& node) override {
    print_header(std::string("UnaryExpression: ") + std::string(to_string(node.op())));
    if (node.operand()) {
      print_child(node.operand(), false);
    }
  }

  void visit(const BinaryExpression& node) override {
    print_header(std::string("BinaryExpression: ") + std::string(to_string(node.op())));
    if (node.left()) {
      print_child(node.left(), true);
    }
    if (node.right()) {
      print_child(node.right(), false);
    }
  }

  void visit(const CallExpression& node) override {
    print_header("CallExpression");
    const auto& args = node.arguments();
    bool has_args = !args.empty();
    if (node.callee()) {
      print_child(node.callee(), has_args);
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (args[i]) {
        bool has_next = (i + 1 < args.size());
        print_child(args[i].get(), has_next);
      }
    }
  }

  void visit(const GroupExpression& node) override {
    print_header("GroupExpression");
    if (node.expression()) {
      print_child(node.expression(), false);
    }
  }

private:
  void print_header(std::string_view text) {
    if (!indent_.empty()) {
      for (std::size_t i = 0; i + 1 < indent_.size(); ++i) {
        output_ += indent_[i].has_next ? "│   " : "    ";
      }
      output_ += indent_.back().has_next ? "├── " : "└── ";
    }
    output_ += text;
    output_ += "\n";
  }

  void print_child(const ASTNode* child, bool has_next) {
    if (!child) return;
    indent_.push_back(Frame{.has_next = has_next});
    child->accept(*this);
    indent_.pop_back();
  }

  std::string output_;
  std::vector<Frame> indent_;
};

std::string ASTPrinter::print(const ASTNode& root) {
  ASTPrinterImpl impl;
  return impl.print(root);
}

void ASTPrinter::visit(const Program& node) { (void)node; }
void ASTPrinter::visit(const FunctionDeclaration& node) { (void)node; }
void ASTPrinter::visit(const VariableDeclaration& node) { (void)node; }
void ASTPrinter::visit(const AssignmentStatement& node) { (void)node; }
void ASTPrinter::visit(const ReturnStatement& node) { (void)node; }
void ASTPrinter::visit(const IfStatement& node) { (void)node; }
void ASTPrinter::visit(const WhileStatement& node) { (void)node; }
void ASTPrinter::visit(const BlockStatement& node) { (void)node; }
void ASTPrinter::visit(const ExpressionStatement& node) { (void)node; }
void ASTPrinter::visit(const LiteralExpression& node) { (void)node; }
void ASTPrinter::visit(const IdentifierExpression& node) { (void)node; }
void ASTPrinter::visit(const UnaryExpression& node) { (void)node; }
void ASTPrinter::visit(const BinaryExpression& node) { (void)node; }
void ASTPrinter::visit(const CallExpression& node) { (void)node; }
void ASTPrinter::visit(const GroupExpression& node) { (void)node; }

std::string dump_ast(const ASTNode& root) {
  ASTPrinter printer;
  return printer.print(root);
}

}  // namespace neuroparse::ast
