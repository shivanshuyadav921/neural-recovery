# NeuroParse Language & Compiler Design (Phase 18 Final)

This document specifies the NeuroLang v1 grammar, token categories, repair candidate formats, and parsing strategies implemented in NeuroParse.

---

## 1. NeuroLang Grammar Specification (v1)

NeuroLang is a statically scoped, C-family experimental language designed for compiler error recovery research.

### Concrete Syntax (EBNF):
```text
program            := { function_decl | declaration } EOF
function_decl      := "function" IDENTIFIER "(" [ params ] ")" block
params             := IDENTIFIER { "," IDENTIFIER }
block              := "{" { statement } "}"
statement          := var_decl | assignment | return_stmt | if_stmt
                    | while_stmt | expr_stmt | block
var_decl           := "let" IDENTIFIER "=" expression ";"
assignment         := IDENTIFIER "=" expression ";"
return_stmt        := "return" [ expression ] ";"
if_stmt            := "if" "(" expression ")" statement [ "else" statement ]
while_stmt         := "while" "(" expression ")" statement
expr_stmt          := expression ";"
expression         := pratt_expression   # Unary, binary, comparisons, calls, identifiers, literals
```

### Keywords (9):
`function`, `let`, `return`, `if`, `else`, `while`, `true`, `false`, `null`

### Delimiters (8):
`(`, `)`, `{`, `}`, `[`, `]`, `;`, `,`

### Operators (15):
`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `=`, `!`, `&&`, `||`

---

## 2. Structured Repair Candidate Schema

The neural model does not generate free-form text. It predicts discrete, structured edit operations:

```json
{
  "operation": "INSERT_TOKEN",
  "position": 12,
  "token": ";",
  "confidence": 0.95
}
```

### Valid Operations:
- `INSERT_TOKEN`: Inserts a specific vocabulary token at the given sequence index.
- `DELETE_TOKEN`: Erases the erroneous token at the given sequence index.
- `REPLACE_TOKEN`: Replaces the erroneous token at the given index with a predicted token.
- `NO_REPAIR`: Declares that no local repair should be attempted.

---

## 3. Parsing Strategy

- **Declarations & Statements:** Recursive descent with deterministic lookahead.
- **Expressions:** Pratt parser with precedence binding:
  - Multiplicative (`*`, `/`, `%`): Precedence 60
  - Additive (`+`, `-`): Precedence 50
  - Relational (`<`, `<=`, `>`, `>=`): Precedence 40
  - Equality (`==`, `!=`): Precedence 30
  - Logical AND (`&&`): Precedence 20
  - Logical OR (`||`): Precedence 10
  - Assignment (`=`): Precedence 5

---

## 4. Technology Stack & Runtime Bounds

- **C++ Standard:** C++20 (`std::string_view`, `std::unique_ptr`, `constexpr`).
- **Memory Management:** RAII, zero manual `delete`, bounded vector allocations.
- **Inference Runtime:** Local ONNX Runtime (CPUExecutionProvider). Zero cloud dependencies, zero external network traffic.
