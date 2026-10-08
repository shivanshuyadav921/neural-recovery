# Chapter 2: NeuroLang Grammar & Typed AST Hierarchy

This chapter describes the formal grammar of **NeuroLang v1**, its operator precedence rules, the typed Abstract Syntax Tree (AST) node hierarchy, the ownership semantics, and AST visitor implementations.

---

## 1. NeuroLang v1 Grammar Specification (EBNF)

NeuroLang is a statically scoped, expression-rich language designed specifically for deterministic compiler recovery experiments.

```ebnf
program             ::= { declaration_or_statement } EOF ;

declaration_or_stmt ::= function_decl
                      | statement ;

function_decl       ::= "function" IDENTIFIER "(" [ param_list ] ")" block_stmt ;
param_list          ::= IDENTIFIER { "," IDENTIFIER } ;

statement           ::= var_decl_stmt
                      | assignment_stmt
                      | return_stmt
                      | if_stmt
                      | while_stmt
                      | block_stmt
                      | expr_stmt ;

var_decl_stmt       ::= "let" IDENTIFIER "=" expression ";" ;
assignment_stmt     ::= IDENTIFIER "=" expression ";" ;
return_stmt         ::= "return" [ expression ] ";" ;
if_stmt             ::= "if" "(" expression ")" statement [ "else" statement ] ;
while_stmt          ::= "while" "(" expression ")" statement ;
block_stmt          ::= "{" { statement } "}" ;
expr_stmt           ::= expression ";" ;

expression          ::= pratt_expression ;
```

---

## 2. Lexical Elements

### Keywords (9)
| Keyword | TokenType | Description |
|---|---|---|
| `function` | `TokenType::KwFunction` | Function definition |
| `let` | `TokenType::KwLet` | Variable binding declaration |
| `return` | `TokenType::KwReturn` | Function return statement |
| `if` | `TokenType::KwIf` | Conditional branch |
| `else` | `TokenType::KwElse` | Alternative branch |
| `while` | `TokenType::KwWhile` | Loop construct |
| `true` | `TokenType::KwTrue` | Boolean true literal |
| `false` | `TokenType::KwFalse` | Boolean false literal |
| `null` | *(Reserved)* | Reserved literal |

### Delimiters & Punctuation (8)
| Lexeme | TokenType | Category |
|---|---|---|
| `(` | `TokenType::LeftParen` | Delimiter |
| `)` | `TokenType::RightParen` | Delimiter |
| `{` | `TokenType::LeftBrace` | Delimiter |
| `}` | `TokenType::RightBrace` | Delimiter |
| `[` | `TokenType::LeftBracket` | Delimiter |
| `]` | `TokenType::RightBracket` | Delimiter |
| `;` | `TokenType::Semicolon` | Punctuation / Statement Terminator |
| `,` | `TokenType::Comma` | Punctuation / Parameter Separator |

### Operators & Precedence (15)

Expressions in NeuroLang are parsed via a **Pratt (Top-Down Operator Precedence)** parser defined in [`compiler/include/neuroparse/parser/parser.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/parser/parser.hpp#L30-L42):

| Precedence Level | Enum Name | Operators | Associativity |
|---|---|---|---|
| **10** | `Precedence::Primary` | Literals, Identifiers, Grouping `(...)` | - |
| **9** | `Precedence::Call` | Function Call `foo(...)` | Left |
| **8** | `Precedence::Unary` | `!`, `-`, `+` | Right |
| **7** | `Precedence::Factor` | `*`, `/`, `%` | Left |
| **6** | `Precedence::Term` | `+`, `-` | Left |
| **5** | `Precedence::Comparison` | `<`, `<=`, `>`, `>=` | Left |
| **4** | `Precedence::Equality` | `==`, `!=` | Left |
| **3** | `Precedence::LogicalAnd`| `&&` | Left |
| **2** | `Precedence::LogicalOr` | `\|\|` | Left |
| **1** | `Precedence::Assignment`| `=` | Right |

---

## 3. Typed AST Node Hierarchy

The AST definitions reside in [`compiler/include/neuroparse/ast/ast.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast.hpp).

```
                  ASTNode (Abstract Base)
                     │
       ┌─────────────┼──────────────┐
       ▼             ▼              ▼
  Declaration    Statement      Expression
       │             │              │
       │             ├─ VariableDecl├─ LiteralExpression
       │             ├─ Assignment  ├─ IdentifierExpression
       ▼             ├─ Return      ├─ UnaryExpression
  FunctionDecl       ├─ If          ├─ BinaryExpression
                     ├─ While       ├─ CallExpression
                     ├─ Block       └─ GroupExpression
                     └─ ExprStmt
```

### 1. Abstract Base: `ASTNode`
```cpp
class ASTNode {
public:
  virtual ~ASTNode() = default;
  [[nodiscard]] virtual ASTNodeKind kind() const noexcept = 0;
  [[nodiscard]] virtual SourceSpan span() const noexcept = 0;
  virtual void accept(ASTVisitor& visitor) = 0;
  virtual void accept(ConstASTVisitor& visitor) const = 0;
};
```

### 2. Category Base Classes
- `class Declaration : public ASTNode`
- `class Statement : public ASTNode`
- `class Expression : public ASTNode`

### 3. Detailed Node Definitions

#### Root: `Program`
Contains a list of top-level declarations and statements:
```cpp
class Program : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> nodes_;
  SourceSpan span_;
  // ...
};
```

#### Declarations & Statements
- **`FunctionDeclaration`**: Function name (`std::string`), parameters (`std::vector<std::string>`), body (`std::unique_ptr<BlockStatement>`).
- **`VariableDeclaration`**: Variable name (`std::string`), initializer (`std::unique_ptr<Expression>`).
- **`AssignmentStatement`**: Target identifier name (`std::string`), value (`std::unique_ptr<Expression>`).
- **`ReturnStatement`**: Optional return expression (`std::unique_ptr<Expression>`).
- **`IfStatement`**: Condition (`std::unique_ptr<Expression>`), then-branch (`std::unique_ptr<Statement>`), optional else-branch (`std::unique_ptr<Statement>`).
- **`WhileStatement`**: Condition (`std::unique_ptr<Expression>`), loop body (`std::unique_ptr<Statement>`).
- **`BlockStatement`**: Statements sequence (`std::vector<std::unique_ptr<Statement>>`).
- **`ExpressionStatement`**: Evaluated expression (`std::unique_ptr<Expression>`).

#### Expressions
- **`LiteralExpression`**: Value variant (`std::variant<std::int64_t, double, std::string, bool>`), `LiteralType`.
- **`IdentifierExpression`**: Identifier symbol (`std::string`).
- **`UnaryExpression`**: Operator (`UnaryOperator`), operand (`std::unique_ptr<Expression>`).
- **`BinaryExpression`**: Left expression (`std::unique_ptr<Expression>`), operator (`BinaryOperator`), right expression (`std::unique_ptr<Expression>`).
- **`CallExpression`**: Callee expression (`std::unique_ptr<Expression>`), arguments (`std::vector<std::unique_ptr<Expression>>`).
- **`GroupExpression`**: Subexpression inside parentheses `(expr)`.

---

## 4. Ownership & Memory Semantics

1. **Strict Single-Ownership:** Nodes own child subtrees exclusively through `std::unique_ptr<T>`.
2. **Move Semantics:** Node constructors take child pointers by value and move them (`std::move(...)`).
3. **Observer Pointers:** Getters return raw non-owning pointers (`const Expression* operand() const noexcept { return operand_.get(); }`).
4. **Zero Manual Memory Deallocation:** Memory is automatically released recursively through RAII destructors.

---

## 5. Visitor Pattern & AST Printer

NeuroParse implements the standard Visitor pattern via `ASTVisitor` and `ConstASTVisitor` in [`compiler/include/neuroparse/ast/ast_visitor.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast_visitor.hpp).

### AST Printer (`ASTPrinter`)
Implemented in [`compiler/src/ast/ast_printer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/ast/ast_printer.cpp), the printer serializes ASTs into an ASCII tree structure (`--dump-ast` CLI flag):

```text
Program [1:1 - 4:2]
  FunctionDeclaration: add [1:1 - 4:2]
    Parameters: [a, b]
    BlockStatement [1:23 - 4:2]
      ReturnStatement [2:3 - 2:16]
        BinaryExpression: + [2:10 - 2:15]
          IdentifierExpression: a [2:10 - 2:11]
          IdentifierExpression: b [2:14 - 2:15]
```
