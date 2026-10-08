# Chapter 3: Lexer & Token System

This chapter specifies the lexical analyzer (`Lexer`), token structures, location tracking, and structural stream invariants in **NeuroParse**.

---

## 1. Token Data Structures

All lexical definitions reside in [`compiler/include/neuroparse/lexer/token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp).

### Source Coordinates: `SourceLocation` & `SourceSpan`
```cpp
struct SourceLocation {
  std::size_t offset = 0;   // Byte offset from start of input (0-indexed)
  std::size_t line = 1;     // Line number (1-indexed)
  std::size_t column = 1;   // Column number (1-indexed)

  constexpr bool operator==(const SourceLocation&) const = default;
};

struct SourceSpan {
  SourceLocation start;
  SourceLocation end;

  constexpr bool operator==(const SourceSpan&) const = default;
};
```

### The `Token` Structure
```cpp
struct Token {
  TokenType type = TokenType::Invalid;
  std::string lexeme;
  SourceLocation location;
  std::size_t length = 0;

  [[nodiscard]] std::size_t offset() const noexcept { return location.offset; }
  [[nodiscard]] std::size_t line() const noexcept { return location.line; }
  [[nodiscard]] std::size_t column() const noexcept { return location.column; }
  [[nodiscard]] TokenCategory category() const noexcept { return token_category(type); }

  [[nodiscard]] bool is(TokenType t) const noexcept { return type == t; }
  [[nodiscard]] bool is_keyword() const noexcept { return category() == TokenCategory::Keyword; }
  [[nodiscard]] bool is_operator() const noexcept { return category() == TokenCategory::Operator; }
  [[nodiscard]] bool is_delimiter() const noexcept { return category() == TokenCategory::Delimiter; }
  [[nodiscard]] bool is_punctuation() const noexcept { return category() == TokenCategory::Punctuation; }
  [[nodiscard]] bool is_literal() const noexcept;

  bool operator==(const Token&) const = default;
};
```

---

## 2. TokenType Inventory (35 Tokens)

The 35 concrete token types are categorized as follows:

| Category | Count | Token Types | Lexemes / Examples |
|---|---|---|---|
| **Eof / Errors** | 2 | `Eof`, `Invalid` | `<EOF>`, invalid byte |
| **Literals** | 4 | `Identifier`, `IntegerLiteral`, `FloatLiteral`, `StringLiteral` | `x`, `42`, `3.14`, `"hello"` |
| **Keywords** | 8 | `KwFunction`, `KwLet`, `KwReturn`, `KwIf`, `KwElse`, `KwWhile`, `KwTrue`, `KwFalse` | `function`, `let`, `return`, `if`, `else`, `while`, `true`, `false` |
| **Operators** | 15 | `Plus`, `Minus`, `Star`, `Slash`, `Percent`, `Equal`, `EqualEqual`, `BangEqual`, `Less`, `LessEqual`, `Greater`, `GreaterEqual`, `Bang`, `AmpAmp`, `PipePipe` | `+`, `-`, `*`, `/`, `%`, `=`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `!`, `&&`, `\|\|` |
| **Delimiters** | 6 | `LeftParen`, `RightParen`, `LeftBrace`, `RightBrace`, `LeftBracket`, `RightBracket` | `(`, `)`, `{`, `}`, `[`, `]` |
| **Punctuation**| 4 | `Comma`, `Semicolon`, `Colon`, `Dot` | `,`, `;`, `:`, `.` |

---

## 3. Scanner State Machine & Implementation

The `Lexer` is implemented in [`compiler/src/lexer/lexer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/lexer/lexer.cpp):

```cpp
class Lexer {
public:
  explicit Lexer(std::string_view source);
  LexResult tokenize();
  // ...
};
```

### Lexing Logic:
1. **Whitespace & Comments:** Skips spaces, tabs, carriage returns, and newlines (incrementing `line` and resetting `column`). Skips line comments starting with `//` until `\n`.
2. **Identifiers & Keywords:** Identifiers begin with `[a-zA-Z_]` followed by `[a-zA-Z0-9_]*`. Keywords are recognized via fast lookup.
3. **Numeric Literals:**
   - Scans digits `[0-9]+`.
   - If followed by `.` and a digit, parses as `FloatLiteral`.
   - Otherwise, parses as `IntegerLiteral`.
4. **String Literals:**
   - Enclosed in `"..."`.
   - Supports escape sequences (`\n`, `\t`, `\"`, `\\`).
5. **Multi-Character Operators:**
   - Checks two-character sequences (`==`, `!=`, `<=`, `>=`, `&&`, `||`) before single characters (`=`, `!`, `<`, `>`).

---

## 4. Token Stream Structural Invariants

The fuzzer and parser enforce strict structural invariants verified by `check_token_stream_invariants`:
1. **Non-Empty Guarantee:** `tokens.empty() == false`. Even an empty file produces `[Token(Eof)]`.
2. **EOF Invariant:** Exactly one EOF token exists in the vector, positioned at index `tokens.size() - 1`.
3. **Monotonic Offsets:** For any indices $i < j$, `tokens[i].offset() <= tokens[j].offset()`.
4. **Valid Source Locations:** All token line and column values are $\ge 1$.
5. **No Intermediate Invalid Tokens:** In clean compilations, no `TokenType::Invalid` tokens exist.
