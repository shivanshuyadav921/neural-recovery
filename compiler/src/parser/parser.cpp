#include "neuroparse/parser/parser.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace neuroparse::parser {
namespace {

const lexer::Token g_eof_token{
    .type = lexer::TokenType::Eof,
    .lexeme = "",
    .location = lexer::SourceLocation{.offset = 0, .line = 1, .column = 1},
    .length = 0,
};

std::string unescape_string_literal(std::string_view lexeme) {
  if (lexeme.size() >= 2 && lexeme.front() == '"' && lexeme.back() == '"') {
    std::string result;
    result.reserve(lexeme.size() - 2);
    for (std::size_t i = 1; i + 1 < lexeme.size(); ++i) {
      if (lexeme[i] == '\\' && i + 2 < lexeme.size()) {
        ++i;
        switch (lexeme[i]) {
          case 'n': result.push_back('\n'); break;
          case 't': result.push_back('\t'); break;
          case 'r': result.push_back('\r'); break;
          case '\\': result.push_back('\\'); break;
          case '"': result.push_back('"'); break;
          case '0': result.push_back('\0'); break;
          case 'b': result.push_back('\b'); break;
          case 'f': result.push_back('\f'); break;
          default: result.push_back(lexeme[i]); break;
        }
      } else {
        result.push_back(lexeme[i]);
      }
    }
    return result;
  }
  return std::string(lexeme);
}

}  // namespace

Parser::Parser(std::vector<lexer::Token> tokens, recovery::RecoveryMode recovery_mode)
    : tokens_(std::move(tokens)),
      cursor_(0),
      controller_(std::make_shared<recovery::RecoveryController>(recovery_mode)) {}

Parser::Parser(std::vector<lexer::Token> tokens, std::shared_ptr<recovery::RecoveryController> controller)
    : tokens_(std::move(tokens)),
      cursor_(0),
      controller_(std::move(controller)) {
  if (!controller_) {
    controller_ = std::make_shared<recovery::RecoveryController>(recovery::RecoveryMode::None);
  }
}

const lexer::Token& Parser::current() const noexcept {
  if (cursor_ < tokens_.size()) {
    return tokens_[cursor_];
  }
  if (!tokens_.empty()) {
    return tokens_.back();
  }
  return g_eof_token;
}

const lexer::Token& Parser::peek(std::size_t offset) const noexcept {
  if (cursor_ + offset < tokens_.size()) {
    return tokens_[cursor_ + offset];
  }
  if (!tokens_.empty()) {
    return tokens_.back();
  }
  return g_eof_token;
}

const lexer::Token& Parser::previous() const noexcept {
  if (cursor_ > 0 && cursor_ - 1 < tokens_.size()) {
    return tokens_[cursor_ - 1];
  }
  return current();
}

const lexer::Token& Parser::advance() noexcept {
  if (!is_at_end()) {
    ++cursor_;
  }
  return previous();
}

bool Parser::is_at_end() const noexcept {
  return cursor_ >= tokens_.size() || current().type == lexer::TokenType::Eof;
}

bool Parser::check(lexer::TokenType type) const noexcept {
  if (is_at_end() && type != lexer::TokenType::Eof) {
    return false;
  }
  return current().type == type;
}

bool Parser::match(lexer::TokenType type) noexcept {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::match(std::initializer_list<lexer::TokenType> types) noexcept {
  for (lexer::TokenType t : types) {
    if (check(t)) {
      advance();
      return true;
    }
  }
  return false;
}

void Parser::report_error(std::string message, std::vector<lexer::TokenType> expected, std::string construct) {
  const auto& tok = current();
  errors_.push_back(ParserError{
      .message = std::move(message),
      .token = tok,
      .token_index = cursor_,
      .location = tok.location,
      .expected_tokens = std::move(expected),
      .construct = std::move(construct),
  });
}

std::optional<lexer::Token> Parser::expect(lexer::TokenType type, std::string_view message, std::string_view construct) {
  if (check(type)) {
    return advance();
  }
  report_error(std::string(message), {type}, std::string(construct));

  if (controller_ && controller_->is_recovery_enabled()) {
    auto rec = controller_->attempt_recovery(tokens_, cursor_, errors_.back(), construct);
    if (rec.success && check(type)) {
      return advance();
    }
  }

  return std::nullopt;
}

Precedence Parser::get_infix_precedence(lexer::TokenType type) noexcept {
  switch (type) {
    case lexer::TokenType::PipePipe:
      return Precedence::LogicalOr;
    case lexer::TokenType::AmpAmp:
      return Precedence::LogicalAnd;
    case lexer::TokenType::EqualEqual:
    case lexer::TokenType::BangEqual:
      return Precedence::Equality;
    case lexer::TokenType::Less:
    case lexer::TokenType::LessEqual:
    case lexer::TokenType::Greater:
    case lexer::TokenType::GreaterEqual:
      return Precedence::Comparison;
    case lexer::TokenType::Plus:
    case lexer::TokenType::Minus:
      return Precedence::Term;
    case lexer::TokenType::Star:
    case lexer::TokenType::Slash:
    case lexer::TokenType::Percent:
      return Precedence::Factor;
    case lexer::TokenType::LeftParen:
      return Precedence::Call;
    default:
      return Precedence::None;
  }
}

ParseResult Parser::parse() {
  auto program = parse_program();
  auto recoveries = controller_ ? controller_->history() : std::vector<recovery::RecoveryResult>{};
  auto metrics = controller_ ? controller_->metrics() : recovery::RecoveryMetrics{};
  return ParseResult{
      .program = std::move(program),
      .errors = errors_,
      .recoveries = std::move(recoveries),
      .metrics = metrics,
  };
}

std::unique_ptr<ast::Program> Parser::parse_program() {
  std::vector<std::unique_ptr<ast::ASTNode>> declarations;
  ast::SourceSpan program_span = ast::SourceSpan::from_token(current());

  while (!is_at_end()) {
    std::size_t start_idx = cursor_;
    auto decl = parse_declaration_or_statement();
    if (decl) {
      program_span = declarations.empty() ? decl->span() : ast::SourceSpan::cover(program_span, decl->span());
      declarations.push_back(std::move(decl));
    } else {
      if (!controller_ || !controller_->is_recovery_enabled()) {
        return std::make_unique<ast::Program>(std::move(declarations), program_span);
      }
      if (is_at_end()) {
        break;
      }
      if (cursor_ <= start_idx && !is_at_end()) {
        advance();
      }
    }
  }

  return std::make_unique<ast::Program>(std::move(declarations), program_span);
}

std::unique_ptr<ast::ASTNode> Parser::parse_declaration_or_statement() {
  if (check(lexer::TokenType::KwFunction)) {
    return parse_function_declaration();
  }
  return parse_statement();
}

std::unique_ptr<ast::FunctionDeclaration> Parser::parse_function_declaration() {
  auto fn_token_opt = expect(lexer::TokenType::KwFunction, "Expected 'function' keyword.", "function declaration");
  if (!fn_token_opt) return nullptr;
  lexer::Token fn_token = *fn_token_opt;

  auto name_opt = expect(lexer::TokenType::Identifier, "Expected function name.", "function declaration");
  if (!name_opt) return nullptr;
  lexer::Token name_token = *name_opt;

  if (!expect(lexer::TokenType::LeftParen, "Expected '(' after function name.", "function declaration")) {
    return nullptr;
  }

  std::vector<ast::FunctionParameter> params;
  if (!check(lexer::TokenType::RightParen)) {
    do {
      auto param_opt = expect(lexer::TokenType::Identifier, "Expected parameter name.", "parameter list");
      if (!param_opt) {
        if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
        if (check(lexer::TokenType::RightParen)) break;
      } else {
        params.push_back(ast::FunctionParameter{
            .name = param_opt->lexeme,
            .span = ast::SourceSpan::from_token(*param_opt),
        });
      }
    } while (match(lexer::TokenType::Comma) ||
             (controller_ && controller_->is_recovery_enabled() &&
              check(lexer::TokenType::Identifier) &&
              expect(lexer::TokenType::Comma, "Expected ',' between parameters.", "parameter list").has_value()));
  }

  if (!expect(lexer::TokenType::RightParen, "Expected ')' after parameter list.", "function declaration")) {
    if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
  }

  if (!check(lexer::TokenType::LeftBrace)) {
    report_error("Expected '{' before function body.", {lexer::TokenType::LeftBrace}, "function declaration");
    if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
    auto rec = controller_->attempt_recovery(tokens_, cursor_, errors_.back(), "function declaration");
    if (!rec.success && !check(lexer::TokenType::LeftBrace)) {
      return nullptr;
    }
  }

  auto body = parse_block_statement();
  if (!body) return nullptr;

  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(fn_token), body->span());
  return std::make_unique<ast::FunctionDeclaration>(name_token.lexeme, std::move(params), std::move(body), span);
}

std::unique_ptr<ast::Statement> Parser::parse_statement() {
  if (check(lexer::TokenType::KwLet)) {
    return parse_var_decl_statement();
  }
  if (check(lexer::TokenType::KwReturn)) {
    return parse_return_statement();
  }
  if (check(lexer::TokenType::KwIf)) {
    return parse_if_statement();
  }
  if (check(lexer::TokenType::KwWhile)) {
    return parse_while_statement();
  }
  if (check(lexer::TokenType::LeftBrace)) {
    return parse_block_statement();
  }
  if (check(lexer::TokenType::Identifier) && peek(1).type == lexer::TokenType::Equal) {
    return parse_assignment_statement();
  }
  return parse_expression_statement();
}

std::unique_ptr<ast::VariableDeclaration> Parser::parse_var_decl_statement() {
  auto let_token_opt = expect(lexer::TokenType::KwLet, "Expected 'let' in variable declaration.", "variable declaration");
  if (!let_token_opt) return nullptr;
  lexer::Token let_token = *let_token_opt;

  auto name_opt = expect(lexer::TokenType::Identifier, "Expected variable name after 'let'.", "variable declaration");
  if (!name_opt) return nullptr;
  lexer::Token name = *name_opt;

  if (!expect(lexer::TokenType::Equal, "Expected '=' after variable name in declaration.", "variable declaration")) {
    return nullptr;
  }

  auto initializer = parse_expression(0);
  if (!initializer) return nullptr;

  auto semi_opt = expect(lexer::TokenType::Semicolon, "Expected ';' after variable declaration.", "variable declaration");
  if (!semi_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = semi_opt ? ast::SourceSpan::from_token(*semi_opt) : initializer->span();
  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(let_token), end_span);
  return std::make_unique<ast::VariableDeclaration>(name.lexeme, std::move(initializer), span);
}

std::unique_ptr<ast::AssignmentStatement> Parser::parse_assignment_statement() {
  auto name_opt = expect(lexer::TokenType::Identifier, "Expected identifier in assignment.", "assignment statement");
  if (!name_opt) return nullptr;
  lexer::Token name = *name_opt;

  if (!expect(lexer::TokenType::Equal, "Expected '=' in assignment statement.", "assignment statement")) {
    return nullptr;
  }

  auto value = parse_expression(0);
  if (!value) return nullptr;

  auto semi_opt = expect(lexer::TokenType::Semicolon, "Expected ';' after assignment.", "assignment statement");
  if (!semi_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = semi_opt ? ast::SourceSpan::from_token(*semi_opt) : value->span();
  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(name), end_span);
  return std::make_unique<ast::AssignmentStatement>(name.lexeme, std::move(value), span);
}

std::unique_ptr<ast::ReturnStatement> Parser::parse_return_statement() {
  auto return_token_opt = expect(lexer::TokenType::KwReturn, "Expected 'return'.", "return statement");
  if (!return_token_opt) return nullptr;
  lexer::Token return_token = *return_token_opt;

  std::unique_ptr<ast::Expression> value;
  if (!check(lexer::TokenType::Semicolon)) {
    value = parse_expression(0);
  }

  auto semi_opt = expect(lexer::TokenType::Semicolon, "Expected ';' after return statement.", "return statement");
  if (!semi_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = semi_opt ? ast::SourceSpan::from_token(*semi_opt)
                             : (value ? value->span() : ast::SourceSpan::from_token(return_token));
  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(return_token), end_span);
  return std::make_unique<ast::ReturnStatement>(std::move(value), span);
}

std::unique_ptr<ast::IfStatement> Parser::parse_if_statement() {
  auto if_token_opt = expect(lexer::TokenType::KwIf, "Expected 'if'.", "if statement");
  if (!if_token_opt) return nullptr;
  lexer::Token if_token = *if_token_opt;

  if (!expect(lexer::TokenType::LeftParen, "Expected '(' after 'if'.", "if statement")) {
    return nullptr;
  }

  auto condition = parse_expression(0);
  if (!condition) return nullptr;

  if (!expect(lexer::TokenType::RightParen, "Expected ')' after if condition.", "if statement")) {
    if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
  }

  auto then_branch = parse_statement();
  if (!then_branch) return nullptr;

  std::unique_ptr<ast::Statement> else_branch;
  if (match(lexer::TokenType::KwElse)) {
    else_branch = parse_statement();
  }

  ast::SourceSpan span = ast::SourceSpan::cover(
      ast::SourceSpan::from_token(if_token),
      else_branch ? else_branch->span() : then_branch->span());

  return std::make_unique<ast::IfStatement>(std::move(condition), std::move(then_branch), std::move(else_branch), span);
}

std::unique_ptr<ast::WhileStatement> Parser::parse_while_statement() {
  auto while_token_opt = expect(lexer::TokenType::KwWhile, "Expected 'while'.", "while statement");
  if (!while_token_opt) return nullptr;
  lexer::Token while_token = *while_token_opt;

  if (!expect(lexer::TokenType::LeftParen, "Expected '(' after 'while'.", "while statement")) {
    return nullptr;
  }

  auto condition = parse_expression(0);
  if (!condition) return nullptr;

  if (!expect(lexer::TokenType::RightParen, "Expected ')' after while condition.", "while statement")) {
    if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
  }

  auto body = parse_statement();
  if (!body) return nullptr;

  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(while_token), body->span());
  return std::make_unique<ast::WhileStatement>(std::move(condition), std::move(body), span);
}

std::unique_ptr<ast::BlockStatement> Parser::parse_block_statement() {
  auto open_brace_opt = expect(lexer::TokenType::LeftBrace, "Expected '{' to start block.", "block statement");
  if (!open_brace_opt) return nullptr;
  lexer::Token open_brace = *open_brace_opt;

  std::vector<std::unique_ptr<ast::Statement>> statements;
  while (!check(lexer::TokenType::RightBrace) && !is_at_end()) {
    std::size_t start_idx = cursor_;
    auto stmt = parse_statement();
    if (stmt) {
      statements.push_back(std::move(stmt));
    } else {
      if (!controller_ || !controller_->is_recovery_enabled()) {
        return nullptr;
      }
      if (check(lexer::TokenType::RightBrace) || is_at_end()) {
        break;
      }
      if (cursor_ <= start_idx && !is_at_end()) {
        advance();
      }
    }
  }

  auto close_brace_opt = expect(lexer::TokenType::RightBrace, "Expected '}' after block.", "block statement");
  if (!close_brace_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = close_brace_opt ? ast::SourceSpan::from_token(*close_brace_opt)
                             : (statements.empty() ? ast::SourceSpan::from_token(open_brace)
                                                   : statements.back()->span());
  ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(open_brace), end_span);
  return std::make_unique<ast::BlockStatement>(std::move(statements), span);
}

std::unique_ptr<ast::ExpressionStatement> Parser::parse_expression_statement() {
  auto expr = parse_expression(0);
  if (!expr) return nullptr;

  auto semi_opt = expect(lexer::TokenType::Semicolon, "Expected ';' after expression.", "expression statement");
  if (!semi_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = semi_opt ? ast::SourceSpan::from_token(*semi_opt) : expr->span();
  ast::SourceSpan span = ast::SourceSpan::cover(expr->span(), end_span);
  return std::make_unique<ast::ExpressionStatement>(std::move(expr), span);
}

std::unique_ptr<ast::Expression> Parser::parse_expression(int min_precedence) {
  std::unique_ptr<ast::Expression> left;

  if (match({lexer::TokenType::Plus, lexer::TokenType::Minus, lexer::TokenType::Bang})) {
    lexer::Token op_token = previous();
    auto op_enum = *ast::token_to_unary_op(op_token.type);
    auto operand = parse_expression(static_cast<int>(Precedence::Unary));
    if (!operand) return nullptr;
    ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(op_token), operand->span());
    left = std::make_unique<ast::UnaryExpression>(op_enum, std::move(operand), span);
  } else {
    left = parse_primary();
    if (!left) return nullptr;
  }

  while (true) {
    lexer::TokenType current_type = current().type;
    Precedence prec = get_infix_precedence(current_type);
    if (static_cast<int>(prec) <= min_precedence) {
      break;
    }

    if (current_type == lexer::TokenType::LeftParen) {
      advance(); // consume '('
      left = parse_call(std::move(left));
      if (!left) return nullptr;
    } else {
      advance(); // consume operator
      lexer::Token op_token = previous();
      auto op_enum = *ast::token_to_binary_op(op_token.type);
      auto right = parse_expression(static_cast<int>(prec));
      if (!right) return nullptr;
      ast::SourceSpan span = ast::SourceSpan::cover(left->span(), right->span());
      left = std::make_unique<ast::BinaryExpression>(std::move(left), op_enum, std::move(right), span);
    }
  }

  return left;
}

std::unique_ptr<ast::Expression> Parser::parse_primary() {
  if (match(lexer::TokenType::IntegerLiteral)) {
    lexer::Token tok = previous();
    std::int64_t val = 0;
    try {
      val = std::stoll(tok.lexeme);
    } catch (...) {
      val = 0;
    }
    return std::make_unique<ast::LiteralExpression>(
        val, ast::LiteralType::Integer, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::FloatLiteral)) {
    lexer::Token tok = previous();
    double val = 0.0;
    try {
      val = std::stod(tok.lexeme);
    } catch (...) {
      val = 0.0;
    }
    return std::make_unique<ast::LiteralExpression>(
        val, ast::LiteralType::Float, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::StringLiteral)) {
    lexer::Token tok = previous();
    std::string val = unescape_string_literal(tok.lexeme);
    return std::make_unique<ast::LiteralExpression>(
        std::move(val), ast::LiteralType::String, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::KwTrue)) {
    lexer::Token tok = previous();
    return std::make_unique<ast::LiteralExpression>(
        true, ast::LiteralType::Boolean, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::KwFalse)) {
    lexer::Token tok = previous();
    return std::make_unique<ast::LiteralExpression>(
        false, ast::LiteralType::Boolean, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::Identifier)) {
    lexer::Token tok = previous();
    return std::make_unique<ast::IdentifierExpression>(tok.lexeme, ast::SourceSpan::from_token(tok));
  }

  if (match(lexer::TokenType::LeftParen)) {
    lexer::Token left_paren = previous();
    auto expr = parse_expression(0);
    if (!expr) return nullptr;
    auto right_paren_opt = expect(lexer::TokenType::RightParen, "Expected ')' after grouped expression.", "group expression");
    if (!right_paren_opt && (!controller_ || !controller_->is_recovery_enabled())) {
      return nullptr;
    }
    ast::SourceSpan end_span = right_paren_opt ? ast::SourceSpan::from_token(*right_paren_opt) : expr->span();
    ast::SourceSpan span = ast::SourceSpan::cover(ast::SourceSpan::from_token(left_paren), end_span);
    return std::make_unique<ast::GroupExpression>(std::move(expr), span);
  }

  report_error("Unexpected token '" + current().lexeme + "' in expression.",
               {lexer::TokenType::Identifier, lexer::TokenType::IntegerLiteral, lexer::TokenType::LeftParen},
               "expression");
  if (controller_ && controller_->is_recovery_enabled()) {
    auto rec = controller_->attempt_recovery(tokens_, cursor_, errors_.back(), "expression");
    if (rec.success) {
      if (check(lexer::TokenType::Identifier) ||
          check(lexer::TokenType::IntegerLiteral) ||
          check(lexer::TokenType::FloatLiteral) ||
          check(lexer::TokenType::StringLiteral) ||
          check(lexer::TokenType::LeftParen) ||
          check(lexer::TokenType::KwTrue) ||
          check(lexer::TokenType::KwFalse)) {
        return parse_primary();
      }
    }
  }
  return nullptr;
}

std::unique_ptr<ast::Expression> Parser::parse_call(std::unique_ptr<ast::Expression> callee) {
  std::vector<std::unique_ptr<ast::Expression>> args;

  if (!check(lexer::TokenType::RightParen)) {
    do {
      auto arg = parse_expression(0);
      if (!arg) {
        if (!controller_ || !controller_->is_recovery_enabled()) return nullptr;
        if (check(lexer::TokenType::RightParen)) break;
      } else {
        args.push_back(std::move(arg));
      }
    } while (match(lexer::TokenType::Comma) ||
             (controller_ && controller_->is_recovery_enabled() &&
              (current().type == lexer::TokenType::Identifier || current().is_literal()) &&
              expect(lexer::TokenType::Comma, "Expected ',' between arguments.", "function call").has_value()));
  }

  auto right_paren_opt = expect(lexer::TokenType::RightParen, "Expected ')' after function arguments.", "function call");
  if (!right_paren_opt && (!controller_ || !controller_->is_recovery_enabled())) {
    return nullptr;
  }

  ast::SourceSpan end_span = right_paren_opt ? ast::SourceSpan::from_token(*right_paren_opt)
                             : (args.empty() ? callee->span() : args.back()->span());
  ast::SourceSpan span = ast::SourceSpan::cover(callee->span(), end_span);
  return std::make_unique<ast::CallExpression>(std::move(callee), std::move(args), span);
}

}  // namespace neuroparse::parser
