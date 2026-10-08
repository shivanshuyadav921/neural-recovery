#pragma once

#include "neuroparse/ast/ast.hpp"
#include "neuroparse/lexer/token.hpp"

#include <string>
#include <vector>

namespace neuroparse::fuzz {

struct InvariantResult {
  bool is_valid = true;
  std::vector<std::string> violations;

  void add_violation(std::string msg) {
    is_valid = false;
    violations.push_back(std::move(msg));
  }
};

/// Validates structural invariants of a token stream.
/// Invariants:
/// 1. Stream is non-empty.
/// 2. Exactly one EOF token exists, and it is at the very end (index == size() - 1).
/// 3. Token source positions are monotonically non-decreasing.
/// 4. SourceSpans are non-inverted (start.line <= end.line, start.column <= end.column if same line).
/// 5. No token has TokenType::Invalid unless explicitly permitted.
[[nodiscard]] InvariantResult check_token_stream_invariants(const std::vector<lexer::Token>& tokens);

/// Validates structural invariants of an AST hierarchy.
/// Invariants:
/// 1. Node pointer is non-null.
/// 2. Program node has non-null child declarations.
/// 3. BinaryExpression has non-null left and right child pointers.
/// 4. UnaryExpression has non-null operand.
/// 5. CallExpression has non-null callee and non-null arguments.
/// 6. IfStatement has non-null condition and then_branch.
/// 7. WhileStatement has non-null condition and body.
/// 8. BlockStatement has non-null statement pointers.
/// 9. FunctionDeclaration has non-null body.
/// 10. Source spans are non-inverted.
[[nodiscard]] InvariantResult check_ast_invariants(const ast::ASTNode* root);

}  // namespace neuroparse::fuzz
