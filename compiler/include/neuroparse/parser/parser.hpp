#pragma once

#include "neuroparse/ast/ast.hpp"
#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/recovery_controller.hpp"
#include "neuroparse/recovery/recovery_result.hpp"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace neuroparse::parser {

struct ParseResult {
  std::unique_ptr<ast::Program> program;
  std::vector<ParserError> errors;
  std::vector<recovery::RecoveryResult> recoveries;
  recovery::RecoveryMetrics metrics;

  [[nodiscard]] bool ok() const noexcept { return errors.empty() && program != nullptr; }
  [[nodiscard]] bool has_errors() const noexcept { return !errors.empty(); }
  [[nodiscard]] bool has_recovered() const noexcept {
    return !errors.empty() && metrics.recovered_errors > 0;
  }
};

enum class Precedence : int {
  None = 0,
  Assignment = 1,   // =
  LogicalOr = 2,    // ||
  LogicalAnd = 3,   // &&
  Equality = 4,     // ==, !=
  Comparison = 5,   // <, <=, >, >=
  Term = 6,         // +, -
  Factor = 7,       // *, /, %
  Unary = 8,        // !, -, +
  Call = 9,         // ()
  Primary = 10,
};

class Parser {
public:
  explicit Parser(
      std::vector<lexer::Token> tokens,
      recovery::RecoveryMode recovery_mode = recovery::RecoveryMode::Auto);
  explicit Parser(
      std::vector<lexer::Token> tokens,
      std::shared_ptr<recovery::RecoveryController> controller);

  // Parse the complete token stream into an AST Program
  [[nodiscard]] ParseResult parse();

  // Token stream navigation
  [[nodiscard]] const lexer::Token& current() const noexcept;
  [[nodiscard]] const lexer::Token& peek(std::size_t offset = 0) const noexcept;
  [[nodiscard]] const lexer::Token& previous() const noexcept;
  const lexer::Token& advance() noexcept;
  [[nodiscard]] bool check(lexer::TokenType type) const noexcept;
  bool match(lexer::TokenType type) noexcept;
  bool match(std::initializer_list<lexer::TokenType> types) noexcept;
  std::optional<lexer::Token> expect(
      lexer::TokenType type,
      std::string_view message,
      std::string_view construct = "");

  [[nodiscard]] bool is_at_end() const noexcept;
  [[nodiscard]] std::size_t current_index() const noexcept { return cursor_; }
  [[nodiscard]] const std::vector<ParserError>& errors() const noexcept { return errors_; }
  [[nodiscard]] bool has_errors() const noexcept { return !errors_.empty(); }

  [[nodiscard]] const recovery::RecoveryController* recovery_controller() const noexcept {
    return controller_.get();
  }

  // Grammar constructs
  std::unique_ptr<ast::Program> parse_program();
  std::unique_ptr<ast::ASTNode> parse_declaration_or_statement();
  std::unique_ptr<ast::FunctionDeclaration> parse_function_declaration();
  std::unique_ptr<ast::Statement> parse_statement();
  std::unique_ptr<ast::VariableDeclaration> parse_var_decl_statement();
  std::unique_ptr<ast::AssignmentStatement> parse_assignment_statement();
  std::unique_ptr<ast::ReturnStatement> parse_return_statement();
  std::unique_ptr<ast::IfStatement> parse_if_statement();
  std::unique_ptr<ast::WhileStatement> parse_while_statement();
  std::unique_ptr<ast::BlockStatement> parse_block_statement();
  std::unique_ptr<ast::ExpressionStatement> parse_expression_statement();

  // Pratt Expression parsing
  std::unique_ptr<ast::Expression> parse_expression(int min_precedence = 0);
  std::unique_ptr<ast::Expression> parse_primary();
  std::unique_ptr<ast::Expression> parse_call(std::unique_ptr<ast::Expression> callee);

  static Precedence get_infix_precedence(lexer::TokenType type) noexcept;

private:
  void report_error(
      std::string message,
      std::vector<lexer::TokenType> expected = {},
      std::string construct = "");

  std::vector<lexer::Token> tokens_;
  std::size_t cursor_ = 0;
  std::vector<ParserError> errors_;
  std::shared_ptr<recovery::RecoveryController> controller_;
};

}  // namespace neuroparse::parser
