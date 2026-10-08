# Chapter 6: Context Extraction & Token Windowing

This chapter covers the **`ContextExtractor`**, its sliding window configuration, the structure of **`ErrorContext`**, boundary clamping, and how syntactic context is serialized for neural inference.

---

## 1. Context Windowing Architecture

Neural language models cannot process unbounded compiler token streams with low latency. The `ContextExtractor` ([`compiler/include/neuroparse/recovery/context_extractor.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/context_extractor.hpp)) extracts an immutable, localized window of tokens surrounding the point of syntactic failure.

```
Token Stream:
[tok_0, tok_1, ... tok_k-50, ... tok_k-1] [tok_k: ERROR] [tok_k+1, ... tok_k+10] ... [EOF]
 └──────────────────────────────────────┘                └─────────────────────┘
         50 tokens before error                             10 tokens after error
```

### Window Configuration: `ContextWindowConfig`
```cpp
struct ContextWindowConfig {
  std::size_t tokens_before = 50;  // Preceding syntactic context
  std::size_t tokens_after = 10;   // Forward lookahead context
};
```

---

## 2. The `ErrorContext` Structure

```cpp
struct ErrorContext {
  std::vector<lexer::Token> tokens_before;
  lexer::Token error_token;
  std::vector<lexer::Token> tokens_after;

  std::size_t error_token_index = 0;
  std::size_t total_tokens = 0;
  lexer::SourceLocation error_location;
  std::vector<lexer::TokenType> expected_tokens;
  std::string diagnostic_message;
  std::string construct;

  // Exact slice bounds [window_start_index, window_end_index)
  std::size_t window_start_index = 0;
  std::size_t window_end_index = 0;
  ContextWindowConfig config;

  [[nodiscard]] bool is_eof_error() const noexcept {
    return error_token.type == lexer::TokenType::Eof;
  }
};
```

---

## 3. Clamping & Boundary Handling

The extraction algorithm safely handles edge cases (e.g., error at token 0, error at EOF, small input files):

```cpp
ErrorContext ContextExtractor::extract(
    std::span<const lexer::Token> tokens,
    const parser::ParserError& error) const {
  ErrorContext ctx;
  if (tokens.empty()) return ctx;

  std::size_t err_idx = std::min(error.token_index, tokens.size() - 1);
  ctx.error_token = tokens[err_idx];
  ctx.error_token_index = err_idx;
  ctx.total_tokens = tokens.size();

  // 1. Compute start index (clamped to 0)
  ctx.window_start_index = (err_idx > config_.tokens_before) 
                             ? (err_idx - config_.tokens_before) 
                             : 0;

  // 2. Compute end index (clamped to tokens.size())
  ctx.window_end_index = std::min(err_idx + 1 + config_.tokens_after, tokens.size());

  // 3. Slice tokens_before
  for (std::size_t i = ctx.window_start_index; i < err_idx; ++i) {
    ctx.tokens_before.push_back(tokens[i]);
  }

  // 4. Slice tokens_after
  for (std::size_t i = err_idx + 1; i < ctx.window_end_index; ++i) {
    ctx.tokens_after.push_back(tokens[i]);
  }

  return ctx;
}
```

---

## 4. Context Dump CLI (`--dump-context`)

The compiler CLI provides `--dump-context` to inspect extracted context windows:

```bash
./build/neuroparse.exe examples/syntax_errors/missing_semicolon.nl --dump-context
```

### Example Output:
```text
=== Context Window [3, 10) ===
Tokens Before (4):
  [3] let (KEYWORD)
  [4] x (IDENTIFIER)
  [5] = (OPERATOR)
  [6] 42 (INTEGER)
Error Token:
  [7] let (KEYWORD) at line 2, col 1
Tokens After (2):
  [8] y (IDENTIFIER)
  [9] = (OPERATOR)
Expected Tokens: [';']
Construct Context: let binding
```
