# Chapter 4: Recursive-Descent & Pratt Parser Implementation

This chapter details the parsing algorithms in **NeuroParse**, covering declaration/statement recursive descent, the Pratt expression engine, and the error recovery integration hook.

---

## 1. Parser Architecture

The parser resides in [`compiler/include/neuroparse/parser/parser.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neuroparse/parser/parser.hpp) and [`compiler/src/parser/parser.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/parser/parser.cpp).

```cpp
class Parser {
public:
  explicit Parser(
      std::vector<lexer::Token> tokens,
      recovery::RecoveryMode recovery_mode = recovery::RecoveryMode::Auto);
      
  ParseResult parse();
  // ...
private:
  std::vector<lexer::Token> tokens_;
  std::size_t cursor_ = 0;
  std::vector<ParserError> errors_;
  std::shared_ptr<recovery::RecoveryController> controller_;
};
```

---

## 2. Token Stream Navigation Primitives

The parser navigates the token stream via lookahead and assertion methods:

```cpp
const Token& current() const noexcept;                  // Returns tokens_[cursor_]
const Token& peek(std::size_t offset = 0) const noexcept;// Looks ahead without advancing
const Token& previous() const noexcept;                 // Returns tokens_[cursor_ - 1]
const Token& advance() noexcept;                        // Consumes current token, increments cursor_
bool check(TokenType type) const noexcept;              // Tests current().is(type)
bool match(TokenType type) noexcept;                    // If check(type), advances and returns true
std::optional<Token> expect(TokenType type, std::string_view msg, std::string_view construct);
```

### The `expect()` Contract
When `expect(type, msg, construct)` is called:
1. If `check(type)` is `true`, consumes and returns the token.
2. If `check(type)` is `false`:
   - Records a `ParserError` with expected tokens and construct context.
   - Invokes `controller_->attempt_recovery(tokens_, cursor_, error, construct)`.
   - If recovery succeeds, the token stream and cursor are adjusted, allowing parsing to proceed.
   - If recovery fails, returns `std::nullopt`.

---

## 3. Recursive-Descent Grammar Parsers

### Function Declarations
```cpp
std::unique_ptr<ast::FunctionDeclaration> Parser::parse_function_declaration() {
  expect(TokenType::KwFunction, "expected 'function'");
  Token name_tok = *expect(TokenType::Identifier, "expected function name");
  expect(TokenType::LeftParen, "expected '(' after function name");
  // Parse parameter list...
  expect(TokenType::RightParen, "expected ')' after parameters");
  auto body = parse_block_statement();
  return std::make_unique<ast::FunctionDeclaration>(name_tok.lexeme, std::move(params), std::move(body), span);
}
```

### Statements & Control Flow
- **`parse_var_decl_statement()`**: Matches `let` $\to$ Identifier $\to$ `=` $\to$ `parse_expression()` $\to$ `;`.
- **`parse_assignment_statement()`**: Matches Identifier $\to$ `=` $\to$ `parse_expression()` $\to$ `;`.
- **`parse_return_statement()`**: Matches `return` $\to$ optional `parse_expression()` $\to$ `;`.
- **`parse_if_statement()`**: Matches `if` $\to$ `(` $\to$ `parse_expression()` $\to$ `)` $\to$ `parse_statement()` $\to$ optional `else` branch.
- **`parse_while_statement()`**: Matches `while` $\to$ `(` $\to$ `parse_expression()` $\to$ `)` $\to$ `parse_statement()`.
- **`parse_block_statement()`**: Matches `{` $\to$ statements loop until `}` $\to$ `}`.

---

## 4. Pratt Expression Parsing Engine

NeuroParse uses **Pratt Top-Down Operator Precedence** parsing to handle complex binary and unary expressions cleanly:

```cpp
std::unique_ptr<ast::Expression> Parser::parse_expression(int min_precedence) {
  // 1. Prefix parse (Unary, Literals, Identifiers, Grouping)
  auto left = parse_primary();
  if (!left) return nullptr;

  // 2. Infix parse loop
  while (!is_at_end()) {
    TokenType op_type = current().type;
    Precedence prec = get_infix_precedence(op_type);
    if (static_cast<int>(prec) <= min_precedence) {
      break;
    }

    // Handle Function Call suffix: expr '(' args ')'
    if (op_type == TokenType::LeftParen) {
      left = parse_call(std::move(left));
      continue;
    }

    // Consume binary operator
    Token op_token = advance();
    auto bin_op = ast::token_to_binary_op(op_type).value();

    // Parse right-hand side with higher binding power (or equal for right-associative)
    int next_min_prec = static_cast<int>(prec);
    auto right = parse_expression(next_min_prec);
    
    SourceSpan span{left->span().start, right ? right->span().end : op_token.location};
    left = std::make_unique<ast::BinaryExpression>(
        std::move(left), bin_op, std::move(right), span);
  }

  return left;
}
```

---

## 5. The Error Recovery Integration Hook

When a syntax expectation fails inside any grammar rule, the parser does not abort. Instead, it captures localized context:

```cpp
void Parser::report_error(
    std::string message,
    std::vector<lexer::TokenType> expected,
    std::string construct) {
  ParserError err;
  err.message = std::move(message);
  err.token = current();
  err.token_index = cursor_;
  err.expected_tokens = std::move(expected);
  err.construct = std::move(construct);
  errors_.push_back(err);

  if (controller_ && controller_->is_recovery_enabled()) {
    auto rec_result = controller_->attempt_recovery(tokens_, cursor_, err, err.construct);
    // ... tracks telemetry and adjusts parser cursor ...
  }
}
```

The parsed program is packed into a `ParseResult`:
```cpp
struct ParseResult {
  std::unique_ptr<ast::Program> program;
  std::vector<ParserError> errors;
  std::vector<recovery::RecoveryResult> recoveries;
  recovery::RecoveryMetrics metrics;

  [[nodiscard]] bool ok() const noexcept { return errors.empty() && program != nullptr; }
  [[nodiscard]] bool has_recovered() const noexcept;
};
```
