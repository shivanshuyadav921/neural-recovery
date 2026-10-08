#include "test_harness.hpp"
#include "neuroparse/lexer/lexer.hpp"

using namespace neuroparse::lexer;

void test_lexer_identifiers() {
  Lexer lexer("foo _bar count123 camelCase PascalCase CONST_VAL _1_2_");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 8); // 7 identifiers + EOF

  NP_EXPECT(res.tokens[0].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[0].lexeme == "foo");
  NP_EXPECT(res.tokens[0].line() == 1);
  NP_EXPECT(res.tokens[0].column() == 1);
  NP_EXPECT(res.tokens[0].offset() == 0);
  NP_EXPECT(res.tokens[0].length == 3);

  NP_EXPECT(res.tokens[1].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[1].lexeme == "_bar");

  NP_EXPECT(res.tokens[2].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[2].lexeme == "count123");

  NP_EXPECT(res.tokens[3].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[3].lexeme == "camelCase");

  NP_EXPECT(res.tokens[4].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[4].lexeme == "PascalCase");

  NP_EXPECT(res.tokens[5].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[5].lexeme == "CONST_VAL");

  NP_EXPECT(res.tokens[6].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[6].lexeme == "_1_2_");

  NP_EXPECT(res.tokens[7].type == TokenType::Eof);
}

void test_lexer_keywords() {
  Lexer lexer("function let return if else while true false");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 9);

  NP_EXPECT(res.tokens[0].type == TokenType::KwFunction);
  NP_EXPECT(res.tokens[0].is_keyword());
  NP_EXPECT(res.tokens[0].lexeme == "function");

  NP_EXPECT(res.tokens[1].type == TokenType::KwLet);
  NP_EXPECT(res.tokens[1].is_keyword());
  NP_EXPECT(res.tokens[1].lexeme == "let");

  NP_EXPECT(res.tokens[2].type == TokenType::KwReturn);
  NP_EXPECT(res.tokens[2].is_keyword());

  NP_EXPECT(res.tokens[3].type == TokenType::KwIf);
  NP_EXPECT(res.tokens[3].is_keyword());

  NP_EXPECT(res.tokens[4].type == TokenType::KwElse);
  NP_EXPECT(res.tokens[4].is_keyword());

  NP_EXPECT(res.tokens[5].type == TokenType::KwWhile);
  NP_EXPECT(res.tokens[5].is_keyword());

  NP_EXPECT(res.tokens[6].type == TokenType::KwTrue);
  NP_EXPECT(res.tokens[6].is_keyword());
  NP_EXPECT(res.tokens[6].is_literal());

  NP_EXPECT(res.tokens[7].type == TokenType::KwFalse);
  NP_EXPECT(res.tokens[7].is_keyword());
  NP_EXPECT(res.tokens[7].is_literal());

  NP_EXPECT(res.tokens[8].type == TokenType::Eof);
}

void test_lexer_integers() {
  Lexer lexer("0 42 1000000 999");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 5);

  NP_EXPECT(res.tokens[0].type == TokenType::IntegerLiteral);
  NP_EXPECT(res.tokens[0].lexeme == "0");
  NP_EXPECT(res.tokens[0].category() == TokenCategory::Integer);

  NP_EXPECT(res.tokens[1].type == TokenType::IntegerLiteral);
  NP_EXPECT(res.tokens[1].lexeme == "42");

  NP_EXPECT(res.tokens[2].type == TokenType::IntegerLiteral);
  NP_EXPECT(res.tokens[2].lexeme == "1000000");

  NP_EXPECT(res.tokens[3].type == TokenType::IntegerLiteral);
  NP_EXPECT(res.tokens[3].lexeme == "999");

  NP_EXPECT(res.tokens[4].type == TokenType::Eof);
}

void test_lexer_floats() {
  Lexer lexer("3.14 0.0 123.456 1e5 2.5e-3 4E+10");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 7);

  NP_EXPECT(res.tokens[0].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[0].lexeme == "3.14");
  NP_EXPECT(res.tokens[0].category() == TokenCategory::Float);

  NP_EXPECT(res.tokens[1].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[1].lexeme == "0.0");

  NP_EXPECT(res.tokens[2].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[2].lexeme == "123.456");

  NP_EXPECT(res.tokens[3].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[3].lexeme == "1e5");

  NP_EXPECT(res.tokens[4].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[4].lexeme == "2.5e-3");

  NP_EXPECT(res.tokens[5].type == TokenType::FloatLiteral);
  NP_EXPECT(res.tokens[5].lexeme == "4E+10");

  NP_EXPECT(res.tokens[6].type == TokenType::Eof);
}

void test_lexer_strings() {
  Lexer lexer(R"("hello" "hello world" "with \"escaped\" quotes" "line\nbreak\ttab\\slash")");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 5);

  NP_EXPECT(res.tokens[0].type == TokenType::StringLiteral);
  NP_EXPECT(res.tokens[0].lexeme == "\"hello\"");
  NP_EXPECT(res.tokens[0].category() == TokenCategory::String);

  NP_EXPECT(res.tokens[1].type == TokenType::StringLiteral);
  NP_EXPECT(res.tokens[1].lexeme == "\"hello world\"");

  NP_EXPECT(res.tokens[2].type == TokenType::StringLiteral);
  NP_EXPECT(res.tokens[2].lexeme == "\"with \\\"escaped\\\" quotes\"");

  NP_EXPECT(res.tokens[3].type == TokenType::StringLiteral);
  NP_EXPECT(res.tokens[3].lexeme == "\"line\\nbreak\\ttab\\\\slash\"");

  NP_EXPECT(res.tokens[4].type == TokenType::Eof);
}

void test_lexer_operators() {
  Lexer lexer("+ - * / % = == != < <= > >= ! && ||");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 16);

  NP_EXPECT(res.tokens[0].type == TokenType::Plus);
  NP_EXPECT(res.tokens[0].is_operator());

  NP_EXPECT(res.tokens[1].type == TokenType::Minus);
  NP_EXPECT(res.tokens[2].type == TokenType::Star);
  NP_EXPECT(res.tokens[3].type == TokenType::Slash);
  NP_EXPECT(res.tokens[4].type == TokenType::Percent);
  NP_EXPECT(res.tokens[5].type == TokenType::Equal);
  NP_EXPECT(res.tokens[6].type == TokenType::EqualEqual);
  NP_EXPECT(res.tokens[7].type == TokenType::BangEqual);
  NP_EXPECT(res.tokens[8].type == TokenType::Less);
  NP_EXPECT(res.tokens[9].type == TokenType::LessEqual);
  NP_EXPECT(res.tokens[10].type == TokenType::Greater);
  NP_EXPECT(res.tokens[11].type == TokenType::GreaterEqual);
  NP_EXPECT(res.tokens[12].type == TokenType::Bang);
  NP_EXPECT(res.tokens[13].type == TokenType::AmpAmp);
  NP_EXPECT(res.tokens[14].type == TokenType::PipePipe);
  NP_EXPECT(res.tokens[15].type == TokenType::Eof);
}

void test_lexer_delimiters_and_punctuation() {
  Lexer lexer("( ) { } [ ] , ; : .");
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 11);

  NP_EXPECT(res.tokens[0].type == TokenType::LeftParen);
  NP_EXPECT(res.tokens[0].is_delimiter());

  NP_EXPECT(res.tokens[1].type == TokenType::RightParen);
  NP_EXPECT(res.tokens[1].is_delimiter());

  NP_EXPECT(res.tokens[2].type == TokenType::LeftBrace);
  NP_EXPECT(res.tokens[2].is_delimiter());

  NP_EXPECT(res.tokens[3].type == TokenType::RightBrace);
  NP_EXPECT(res.tokens[3].is_delimiter());

  NP_EXPECT(res.tokens[4].type == TokenType::LeftBracket);
  NP_EXPECT(res.tokens[4].is_delimiter());

  NP_EXPECT(res.tokens[5].type == TokenType::RightBracket);
  NP_EXPECT(res.tokens[5].is_delimiter());

  NP_EXPECT(res.tokens[6].type == TokenType::Comma);
  NP_EXPECT(res.tokens[6].is_punctuation());

  NP_EXPECT(res.tokens[7].type == TokenType::Semicolon);
  NP_EXPECT(res.tokens[7].is_punctuation());

  NP_EXPECT(res.tokens[8].type == TokenType::Colon);
  NP_EXPECT(res.tokens[8].is_punctuation());

  NP_EXPECT(res.tokens[9].type == TokenType::Dot);
  NP_EXPECT(res.tokens[9].is_punctuation());

  NP_EXPECT(res.tokens[10].type == TokenType::Eof);
}

void test_lexer_whitespace_and_comments() {
  std::string_view src =
      "  # python style comment\n"
      "// c style single comment\n"
      "/* block comment \n"
      "   multi line */\n"
      "let x = 10; # trailing comment\n";

  Lexer lexer(src);
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());
  NP_EXPECT(res.tokens.size() == 6);

  NP_EXPECT(res.tokens[0].type == TokenType::KwLet);
  NP_EXPECT(res.tokens[0].line() == 5);
  NP_EXPECT(res.tokens[0].column() == 1);

  NP_EXPECT(res.tokens[1].type == TokenType::Identifier);
  NP_EXPECT(res.tokens[1].lexeme == "x");
  NP_EXPECT(res.tokens[1].column() == 5);

  NP_EXPECT(res.tokens[2].type == TokenType::Equal);
  NP_EXPECT(res.tokens[3].type == TokenType::IntegerLiteral);
  NP_EXPECT(res.tokens[3].lexeme == "10");
  NP_EXPECT(res.tokens[4].type == TokenType::Semicolon);
  NP_EXPECT(res.tokens[5].type == TokenType::Eof);
}

void test_lexer_line_column_offset_tracking() {
  std::string_view src = "let a = 1;\nlet b = 20;\n  let c = 300;";
  Lexer lexer(src);
  auto res = lexer.tokenize();
  NP_EXPECT(res.ok());

  // Line 1: let a = 1;
  NP_EXPECT(res.tokens[0].lexeme == "let");
  NP_EXPECT(res.tokens[0].line() == 1);
  NP_EXPECT(res.tokens[0].column() == 1);
  NP_EXPECT(res.tokens[0].offset() == 0);
  NP_EXPECT(res.tokens[0].length == 3);

  NP_EXPECT(res.tokens[1].lexeme == "a");
  NP_EXPECT(res.tokens[1].line() == 1);
  NP_EXPECT(res.tokens[1].column() == 5);
  NP_EXPECT(res.tokens[1].offset() == 4);
  NP_EXPECT(res.tokens[1].length == 1);

  // Line 2: let b = 20;
  NP_EXPECT(res.tokens[5].lexeme == "let");
  NP_EXPECT(res.tokens[5].line() == 2);
  NP_EXPECT(res.tokens[5].column() == 1);
  NP_EXPECT(res.tokens[5].offset() == 11);

  // Line 3:   let c = 300;
  NP_EXPECT(res.tokens[10].lexeme == "let");
  NP_EXPECT(res.tokens[10].line() == 3);
  NP_EXPECT(res.tokens[10].column() == 3);
  NP_EXPECT(res.tokens[10].offset() == 25);

  NP_EXPECT(res.tokens[13].lexeme == "300");
  NP_EXPECT(res.tokens[13].line() == 3);
  NP_EXPECT(res.tokens[13].column() == 11);
  NP_EXPECT(res.tokens[13].length == 3);
}

void test_lexer_malformed_input() {
  // Unterminated string
  {
    Lexer lexer("\"unterminated string\n");
    auto res = lexer.tokenize();
    NP_EXPECT(res.has_errors());
    NP_EXPECT(res.errors.size() >= 1);
  }

  // Invalid escape
  {
    Lexer lexer(R"("bad \q escape")");
    auto res = lexer.tokenize();
    NP_EXPECT(res.has_errors());
  }

  // Malformed float (multiple dots)
  {
    Lexer lexer("12.34.56");
    auto res = lexer.tokenize();
    NP_EXPECT(res.has_errors());
  }

  // Invalid number suffix
  {
    Lexer lexer("123abc");
    auto res = lexer.tokenize();
    NP_EXPECT(res.has_errors());
  }

  // Unterminated block comment
  {
    Lexer lexer("/* unclosed comment");
    auto res = lexer.tokenize();
    NP_EXPECT(res.has_errors());
  }
}

void test_lexer_unknown_characters() {
  Lexer lexer("let x = @ + $;");
  auto res = lexer.tokenize();
  NP_EXPECT(res.has_errors());
  NP_EXPECT(res.errors.size() == 2);
  NP_EXPECT(res.errors[0].line() == 1);
  NP_EXPECT(res.errors[0].column() == 9); // '@'
  NP_EXPECT(res.errors[1].column() == 13); // '$'
}

void test_lexer_eof() {
  // Empty input
  {
    Lexer lexer("");
    auto res = lexer.tokenize();
    NP_EXPECT(res.ok());
    NP_EXPECT(res.tokens.size() == 1);
    NP_EXPECT(res.tokens[0].type == TokenType::Eof);
    NP_EXPECT(res.tokens[0].line() == 1);
    NP_EXPECT(res.tokens[0].column() == 1);
    NP_EXPECT(res.tokens[0].offset() == 0);
  }

  // Only whitespace
  {
    Lexer lexer("   \n\t  \n");
    auto res = lexer.tokenize();
    NP_EXPECT(res.ok());
    NP_EXPECT(res.tokens.size() == 1);
    NP_EXPECT(res.tokens[0].type == TokenType::Eof);
    NP_EXPECT(res.tokens[0].line() == 3);
  }
}

void test_lexer_representative_programs() {
  // Program 1: hello.nl style function and variable
  {
    std::string_view code =
        "# Valid NeuroLang function\n"
        "function add(a, b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "let x = add(1, 2);\n";

    Lexer lexer(code);
    auto res = lexer.tokenize();
    NP_EXPECT(res.ok());

    std::vector<TokenType> expected = {
        TokenType::KwFunction, TokenType::Identifier, TokenType::LeftParen,
        TokenType::Identifier, TokenType::Comma, TokenType::Identifier,
        TokenType::RightParen, TokenType::LeftBrace,
        TokenType::KwReturn, TokenType::Identifier, TokenType::Plus,
        TokenType::Identifier, TokenType::Semicolon,
        TokenType::RightBrace,
        TokenType::KwLet, TokenType::Identifier, TokenType::Equal,
        TokenType::Identifier, TokenType::LeftParen, TokenType::IntegerLiteral,
        TokenType::Comma, TokenType::IntegerLiteral, TokenType::RightParen,
        TokenType::Semicolon,
        TokenType::Eof
    };

    NP_EXPECT(res.tokens.size() == expected.size());
    for (std::size_t i = 0; i < expected.size() && i < res.tokens.size(); ++i) {
      NP_EXPECT(res.tokens[i].type == expected[i]);
    }
  }

  // Program 2: Control flow with if/else/while
  {
    std::string_view code =
        "function max(a, b) {\n"
        "    if (a > b) {\n"
        "        return a;\n"
        "    } else {\n"
        "        return b;\n"
        "    }\n"
        "}\n"
        "while (x != 0) {\n"
        "    let valid = true;\n"
        "}\n";

    Lexer lexer(code);
    auto res = lexer.tokenize();
    NP_EXPECT(res.ok());
    NP_EXPECT(!res.tokens.empty());
    NP_EXPECT(res.tokens.back().type == TokenType::Eof);
  }
}

void test_lexer_all() {
  test_lexer_identifiers();
  test_lexer_keywords();
  test_lexer_integers();
  test_lexer_floats();
  test_lexer_strings();
  test_lexer_operators();
  test_lexer_delimiters_and_punctuation();
  test_lexer_whitespace_and_comments();
  test_lexer_line_column_offset_tracking();
  test_lexer_malformed_input();
  test_lexer_unknown_characters();
  test_lexer_eof();
  test_lexer_representative_programs();
}
