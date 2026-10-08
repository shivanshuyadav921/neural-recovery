#include "neuroparse/lexer/lexer.hpp"

#include <cctype>
#include <string_view>
#include <utility>

namespace neuroparse::lexer {

Lexer::Lexer(std::string_view source)
    : source_(source), cursor_(0), line_(1), column_(1) {}

std::optional<TokenType> Lexer::lookup_keyword(std::string_view ident) noexcept {
  if (ident == "function") return TokenType::KwFunction;
  if (ident == "let")      return TokenType::KwLet;
  if (ident == "return")   return TokenType::KwReturn;
  if (ident == "if")       return TokenType::KwIf;
  if (ident == "else")     return TokenType::KwElse;
  if (ident == "while")    return TokenType::KwWhile;
  if (ident == "true")     return TokenType::KwTrue;
  if (ident == "false")    return TokenType::KwFalse;
  return std::nullopt;
}

bool Lexer::is_at_end() const noexcept {
  return cursor_ >= source_.size();
}

char Lexer::peek() const noexcept {
  if (is_at_end()) return '\0';
  return source_[cursor_];
}

char Lexer::peek_next() const noexcept {
  if (cursor_ + 1 >= source_.size()) return '\0';
  return source_[cursor_ + 1];
}

char Lexer::advance() noexcept {
  if (is_at_end()) return '\0';
  char c = source_[cursor_++];
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else if (c == '\r') {
    if (cursor_ < source_.size() && source_[cursor_] == '\n') {
      ++cursor_;
    }
    ++line_;
    column_ = 1;
    return '\n';
  } else {
    ++column_;
  }
  return c;
}

bool Lexer::match(char expected) noexcept {
  if (is_at_end()) return false;
  if (peek() != expected) return false;
  advance();
  return true;
}

void Lexer::add_error(std::string message, std::size_t start_offset,
                      std::size_t start_line, std::size_t start_col,
                      std::size_t len) {
  errors_.push_back(LexerError{
      .message = std::move(message),
      .location = SourceLocation{
          .offset = start_offset,
          .line = start_line,
          .column = start_col,
      },
      .length = len == 0 ? 1 : len,
  });
}

Token Lexer::make_token(TokenType type, std::size_t start_offset,
                        std::size_t start_line, std::size_t start_col) {
  std::size_t len = cursor_ - start_offset;
  std::string_view lexeme_view = source_.substr(start_offset, len);
  return Token{
      .type = type,
      .lexeme = std::string(lexeme_view),
      .location = SourceLocation{
          .offset = start_offset,
          .line = start_line,
          .column = start_col,
      },
      .length = len,
  };
}

void Lexer::skip_whitespace_and_comments() {
  while (!is_at_end()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n') {
      advance();
    } else if (c == '#') {
      // Single-line comment started with '#'
      while (!is_at_end() && peek() != '\n' && peek() != '\r') {
        advance();
      }
    } else if (c == '/' && peek_next() == '/') {
      // Single-line comment started with '//'
      advance(); // consume '/'
      advance(); // consume '/'
      while (!is_at_end() && peek() != '\n' && peek() != '\r') {
        advance();
      }
    } else if (c == '/' && peek_next() == '*') {
      // Block comment started with '/*'
      std::size_t start_offset = cursor_;
      std::size_t start_line = line_;
      std::size_t start_col = column_;
      advance(); // consume '/'
      advance(); // consume '*'
      bool closed = false;
      while (!is_at_end()) {
        if (peek() == '*' && peek_next() == '/') {
          advance(); // consume '*'
          advance(); // consume '/'
          closed = true;
          break;
        }
        advance();
      }
      if (!closed) {
        add_error("Unterminated block comment", start_offset, start_line, start_col, cursor_ - start_offset);
      }
    } else {
      break;
    }
  }
}

Token Lexer::scan_identifier_or_keyword(std::size_t start_offset,
                                        std::size_t start_line,
                                        std::size_t start_col) {
  while (!is_at_end()) {
    char c = peek();
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      advance();
    } else {
      break;
    }
  }

  std::string_view text = source_.substr(start_offset, cursor_ - start_offset);
  if (auto kw = lookup_keyword(text); kw.has_value()) {
    return make_token(*kw, start_offset, start_line, start_col);
  }
  return make_token(TokenType::Identifier, start_offset, start_line, start_col);
}

Token Lexer::scan_number(std::size_t start_offset,
                         std::size_t start_line,
                         std::size_t start_col) {
  while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
    advance();
  }

  bool is_float = false;
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
    is_float = true;
    advance(); // consume '.'
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
      advance();
    }
  }

  // Scientific exponent: e.g. 1e5, 1.2e-3, 4E+10
  if (peek() == 'e' || peek() == 'E') {
    char next = peek_next();
    if (std::isdigit(static_cast<unsigned char>(next)) ||
        ((next == '+' || next == '-') && cursor_ + 2 < source_.size() &&
         std::isdigit(static_cast<unsigned char>(source_[cursor_ + 2])))) {
      is_float = true;
      advance(); // consume 'e' / 'E'
      if (peek() == '+' || peek() == '-') {
        advance();
      }
      while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
    }
  }

  // Detect malformed number with additional decimal points e.g. 1.2.3
  if (peek() == '.') {
    while (!is_at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '.')) {
      advance();
    }
    std::size_t len = cursor_ - start_offset;
    add_error("Malformed floating-point literal", start_offset, start_line, start_col, len);
    return make_token(TokenType::Invalid, start_offset, start_line, start_col);
  }

  // Detect invalid letters directly attached to number (e.g. 123abc or 1.2xyz)
  if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_') {
    while (!is_at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
      advance();
    }
    std::size_t len = cursor_ - start_offset;
    add_error(is_float ? "Invalid suffix on floating-point literal" : "Invalid suffix on integer literal",
              start_offset, start_line, start_col, len);
    return make_token(TokenType::Invalid, start_offset, start_line, start_col);
  }

  return make_token(is_float ? TokenType::FloatLiteral : TokenType::IntegerLiteral,
                    start_offset, start_line, start_col);
}

Token Lexer::scan_string(std::size_t start_offset,
                         std::size_t start_line,
                         std::size_t start_col) {
  advance(); // consume opening '"'
  bool has_error = false;

  while (!is_at_end() && peek() != '"') {
    char c = peek();
    if (c == '\n' || c == '\r') {
      add_error("Unterminated string literal", start_offset, start_line, start_col, cursor_ - start_offset);
      return make_token(TokenType::Invalid, start_offset, start_line, start_col);
    }
    if (c == '\\') {
      advance(); // consume '\\'
      if (is_at_end()) {
        add_error("Unterminated string literal with trailing escape", start_offset, start_line, start_col, cursor_ - start_offset);
        return make_token(TokenType::Invalid, start_offset, start_line, start_col);
      }
      char esc = advance();
      switch (esc) {
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case '0':
          break;
        default:
          add_error(std::string("Invalid escape sequence '\\") + esc + "' in string literal",
                    start_offset, start_line, start_col, cursor_ - start_offset);
          has_error = true;
          break;
      }
    } else {
      advance();
    }
  }

  if (is_at_end()) {
    add_error("Unterminated string literal", start_offset, start_line, start_col, cursor_ - start_offset);
    return make_token(TokenType::Invalid, start_offset, start_line, start_col);
  }

  advance(); // consume closing '"'

  if (has_error) {
    return make_token(TokenType::Invalid, start_offset, start_line, start_col);
  }

  return make_token(TokenType::StringLiteral, start_offset, start_line, start_col);
}

Token Lexer::next_token() {
  skip_whitespace_and_comments();

  if (is_at_end()) {
    return Token{
        .type = TokenType::Eof,
        .lexeme = "",
        .location = SourceLocation{
            .offset = cursor_,
            .line = line_,
            .column = column_,
        },
        .length = 0,
    };
  }

  std::size_t start_offset = cursor_;
  std::size_t start_line = line_;
  std::size_t start_col = column_;
  char c = peek();

  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
    return scan_identifier_or_keyword(start_offset, start_line, start_col);
  }

  if (std::isdigit(static_cast<unsigned char>(c))) {
    return scan_number(start_offset, start_line, start_col);
  }

  if (c == '"') {
    return scan_string(start_offset, start_line, start_col);
  }

  advance(); // consume character

  switch (c) {
    case '(': return make_token(TokenType::LeftParen, start_offset, start_line, start_col);
    case ')': return make_token(TokenType::RightParen, start_offset, start_line, start_col);
    case '{': return make_token(TokenType::LeftBrace, start_offset, start_line, start_col);
    case '}': return make_token(TokenType::RightBrace, start_offset, start_line, start_col);
    case '[': return make_token(TokenType::LeftBracket, start_offset, start_line, start_col);
    case ']': return make_token(TokenType::RightBracket, start_offset, start_line, start_col);
    case ',': return make_token(TokenType::Comma, start_offset, start_line, start_col);
    case ';': return make_token(TokenType::Semicolon, start_offset, start_line, start_col);
    case ':': return make_token(TokenType::Colon, start_offset, start_line, start_col);
    case '.': return make_token(TokenType::Dot, start_offset, start_line, start_col);
    case '+': return make_token(TokenType::Plus, start_offset, start_line, start_col);
    case '-': return make_token(TokenType::Minus, start_offset, start_line, start_col);
    case '*': return make_token(TokenType::Star, start_offset, start_line, start_col);
    case '/': return make_token(TokenType::Slash, start_offset, start_line, start_col);
    case '%': return make_token(TokenType::Percent, start_offset, start_line, start_col);

    case '=':
      if (match('=')) {
        return make_token(TokenType::EqualEqual, start_offset, start_line, start_col);
      }
      return make_token(TokenType::Equal, start_offset, start_line, start_col);

    case '!':
      if (match('=')) {
        return make_token(TokenType::BangEqual, start_offset, start_line, start_col);
      }
      return make_token(TokenType::Bang, start_offset, start_line, start_col);

    case '<':
      if (match('=')) {
        return make_token(TokenType::LessEqual, start_offset, start_line, start_col);
      }
      return make_token(TokenType::Less, start_offset, start_line, start_col);

    case '>':
      if (match('=')) {
        return make_token(TokenType::GreaterEqual, start_offset, start_line, start_col);
      }
      return make_token(TokenType::Greater, start_offset, start_line, start_col);

    case '&':
      if (match('&')) {
        return make_token(TokenType::AmpAmp, start_offset, start_line, start_col);
      }
      add_error("Unexpected character '&' (did you mean '&&'?)", start_offset, start_line, start_col, 1);
      return make_token(TokenType::Invalid, start_offset, start_line, start_col);

    case '|':
      if (match('|')) {
        return make_token(TokenType::PipePipe, start_offset, start_line, start_col);
      }
      add_error("Unexpected character '|' (did you mean '||'?)", start_offset, start_line, start_col, 1);
      return make_token(TokenType::Invalid, start_offset, start_line, start_col);

    default:
      add_error(std::string("Unexpected character '") + c + "'", start_offset, start_line, start_col, 1);
      return make_token(TokenType::Invalid, start_offset, start_line, start_col);
  }
}

LexResult Lexer::tokenize() {
  std::vector<Token> tokens;
  while (true) {
    Token tok = next_token();
    tokens.push_back(tok);
    if (tok.type == TokenType::Eof) {
      break;
    }
  }
  return LexResult{
      .tokens = std::move(tokens),
      .errors = errors_,
  };
}

}  // namespace neuroparse::lexer
