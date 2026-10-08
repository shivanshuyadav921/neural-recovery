#include "test_harness.hpp"

#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"
#include "neuroparse/recovery/context_extractor.hpp"
#include "neuroparse/recovery/context_window_config.hpp"
#include "neuroparse/recovery/error_context.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace neuroparse;

std::pair<std::vector<lexer::Token>, parser::ParseResult> parse_for_errors(
    std::string_view source,
    recovery::RecoveryMode mode = recovery::RecoveryMode::Auto) {
  lexer::Lexer lexer(source);
  auto lex_res = lexer.tokenize();
  parser::Parser parser(lex_res.tokens, mode);
  auto parse_res = parser.parse();
  return {std::move(lex_res.tokens), std::move(parse_res)};
}

void test_context_error_middle() {
  const std::string source =
      "let a = 1;\n"
      "let b = 2;\n"
      "let c = + ;\n"
      "let d = 4;\n"
      "let e = 5;\n";

  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor(recovery::ContextWindowConfig{.tokens_before = 5, .tokens_after = 4});
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(ctx.tokens_before.size() <= 5u);
  NP_EXPECT(ctx.tokens_after.size() <= 4u);
  NP_EXPECT(ctx.error_token.type == lexer::TokenType::Semicolon || ctx.error_token.lexeme == ";");
  NP_EXPECT(ctx.error_token_index < tokens.size());

  // Check that error token is not in before or after
  for (const auto& tok : ctx.tokens_before) {
    NP_EXPECT(tok.offset() < ctx.error_token.offset());
  }
  for (const auto& tok : ctx.tokens_after) {
    NP_EXPECT(tok.offset() > ctx.error_token.offset());
  }
}

void test_context_error_first_token() {
  const std::string source = "; let x = 10;";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(ctx.error_token_index == 0u);
  NP_EXPECT(ctx.tokens_before.empty());
  NP_EXPECT(!ctx.tokens_after.empty());
  NP_EXPECT(ctx.error_token.lexeme == ";");
}

void test_context_error_second_token() {
  const std::string source = "let ; = 10;";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(ctx.error_token_index == 1u);
  NP_EXPECT(ctx.tokens_before.size() == 1u);
  NP_EXPECT(ctx.tokens_before[0].lexeme == "let");
  NP_EXPECT(ctx.error_token.lexeme == ";");
}

void test_context_error_at_eof() {
  const std::string source = "let x =";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(ctx.is_eof_error());
  NP_EXPECT(ctx.tokens_after.empty());
  NP_EXPECT(!ctx.tokens_before.empty());
  NP_EXPECT(ctx.error_token.type == lexer::TokenType::Eof);
}

void test_context_window_bounds_less_than_limits() {
  const std::string source = "let x = ;";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  // Default 50 before, 10 after
  recovery::ContextExtractor extractor(recovery::ContextWindowConfig{.tokens_before = 50, .tokens_after = 10});
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  // Total tokens is around 5, so before and after should cleanly clamp
  NP_EXPECT(ctx.tokens_before.size() < 50u);
  NP_EXPECT(ctx.tokens_after.size() < 10u);
  NP_EXPECT(ctx.total_tokens == tokens.size());
}

void test_context_configurable_window_sizes() {
  const std::string source =
      "let a = 1;\n"
      "let b = 2;\n"
      "let c = 3;\n"
      "let d = + ;\n"
      "let e = 5;\n"
      "let f = 6;\n"
      "let g = 7;\n";

  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  // Test 2 before, 2 after
  recovery::ContextExtractor ext2(recovery::ContextWindowConfig{.tokens_before = 2, .tokens_after = 2});
  auto ctx2 = ext2.extract(tokens, parse_res.errors[0]);
  NP_EXPECT(ctx2.tokens_before.size() == 2u);
  NP_EXPECT(ctx2.tokens_after.size() == 2u);

  // Test 0 before, 3 after
  recovery::ContextExtractor ext_zero_before(recovery::ContextWindowConfig{.tokens_before = 0, .tokens_after = 3});
  auto ctx_zb = ext_zero_before.extract(tokens, parse_res.errors[0]);
  NP_EXPECT(ctx_zb.tokens_before.empty());
  NP_EXPECT(ctx_zb.tokens_after.size() == 3u);

  // Test 3 before, 0 after
  recovery::ContextExtractor ext_zero_after(recovery::ContextWindowConfig{.tokens_before = 3, .tokens_after = 0});
  auto ctx_za = ext_zero_after.extract(tokens, parse_res.errors[0]);
  NP_EXPECT(ctx_za.tokens_before.size() == 3u);
  NP_EXPECT(ctx_za.tokens_after.empty());

  // Test 0 before, 0 after
  recovery::ContextExtractor ext_zero_both(recovery::ContextWindowConfig{.tokens_before = 0, .tokens_after = 0});
  auto ctx_both = ext_zero_both.extract(tokens, parse_res.errors[0]);
  NP_EXPECT(ctx_both.tokens_before.empty());
  NP_EXPECT(ctx_both.tokens_after.empty());
}

void test_context_expected_tokens_preserved() {
  const std::string source = "let x = 42";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(!ctx.expected_tokens.empty());
  bool found_semicolon = false;
  for (auto tok_type : ctx.expected_tokens) {
    if (tok_type == lexer::TokenType::Semicolon) {
      found_semicolon = true;
      break;
    }
  }
  NP_EXPECT(found_semicolon);
}

void test_context_location_and_construct() {
  const std::string source = "function f( { return 1; }";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  NP_EXPECT(ctx.error_location.line == 1u);
  NP_EXPECT(!ctx.construct.empty());
  NP_EXPECT(!ctx.diagnostic_message.empty());
}

void test_context_immutability_and_determinism() {
  const std::string source = "function add(a, b) { return a + ; }";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  auto tokens_copy = tokens;

  recovery::ContextExtractor extractor;
  auto ctx1 = extractor.extract(tokens, parse_res.errors[0]);
  auto ctx2 = extractor.extract(tokens, parse_res.errors[0]);

  // Invariant: tokens vector is completely unmodified
  NP_EXPECT(tokens == tokens_copy);

  // Invariant: deterministic results
  NP_EXPECT(ctx1 == ctx2);
  NP_EXPECT(ctx1.error_token == ctx2.error_token);
  NP_EXPECT(ctx1.tokens_before == ctx2.tokens_before);
  NP_EXPECT(ctx1.tokens_after == ctx2.tokens_after);
}

void test_context_extract_all_multiple_errors() {
  const std::string source =
      "let a = 1\n"
      "let b = 2\n"
      "let c = 3\n";

  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::Auto);
  NP_EXPECT(parse_res.errors.size() >= 3u);

  recovery::ContextExtractor extractor;
  auto contexts = extractor.extract_all(tokens, parse_res.errors);

  NP_EXPECT(contexts.size() == parse_res.errors.size());
  for (std::size_t i = 0; i < contexts.size(); ++i) {
    NP_EXPECT(contexts[i].error_location.line == parse_res.errors[i].location.line);
  }
}

void test_context_formatting() {
  const std::string source = "let x = 42\nlet y = 10;";
  auto [tokens, parse_res] = parse_for_errors(source, recovery::RecoveryMode::None);
  NP_EXPECT(!parse_res.errors.empty());

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(tokens, parse_res.errors[0]);

  std::string formatted = ctx.format("test.nl");
  NP_EXPECT(!formatted.empty());
  NP_EXPECT(formatted.find("test.nl:") != std::string::npos);
  NP_EXPECT(formatted.find("Tokens before") != std::string::npos);
  NP_EXPECT(formatted.find("Current error token") != std::string::npos);
  NP_EXPECT(formatted.find("Tokens after") != std::string::npos);
}

void test_context_edge_cases() {
  // Empty token stream
  std::vector<lexer::Token> empty_tokens;
  parser::ParserError err;
  err.message = "Syntax error";
  err.token.type = lexer::TokenType::Eof;
  err.token.lexeme = "";
  err.token_index = 0;
  err.location = lexer::SourceLocation{.offset = 0, .line = 1, .column = 1};

  recovery::ContextExtractor extractor;
  auto ctx = extractor.extract(empty_tokens, err);
  NP_EXPECT(ctx.tokens_before.empty());
  NP_EXPECT(ctx.tokens_after.empty());
  NP_EXPECT(ctx.total_tokens == 0u);

  // Single EOF token stream
  lexer::Token eof_tok;
  eof_tok.type = lexer::TokenType::Eof;
  eof_tok.lexeme = "";
  std::vector<lexer::Token> single_eof = {eof_tok};
  auto ctx_eof = extractor.extract(single_eof, err);
  NP_EXPECT(ctx_eof.tokens_before.empty());
  NP_EXPECT(ctx_eof.tokens_after.empty());
  NP_EXPECT(ctx_eof.is_eof_error());
}

}  // namespace

void run_context_extractor_tests() {
  test_context_error_middle();
  test_context_error_first_token();
  test_context_error_second_token();
  test_context_error_at_eof();
  test_context_window_bounds_less_than_limits();
  test_context_configurable_window_sizes();
  test_context_expected_tokens_preserved();
  test_context_location_and_construct();
  test_context_immutability_and_determinism();
  test_context_extract_all_multiple_errors();
  test_context_formatting();
  test_context_edge_cases();
}
