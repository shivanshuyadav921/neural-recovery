#include "test_harness.hpp"

#include "neuroparse/cli/cli.hpp"

#include <fstream>

void test_cli_help_and_version() {
  using namespace neuroparse::cli;
  const auto help = run(parse_args({"--help"}));
  NP_EXPECT(help.exit_code == 0);
  NP_EXPECT(help.stdout_text.find("neuroparse") != std::string::npos);

  const auto version = run(parse_args({"--version"}));
  NP_EXPECT(version.exit_code == 0);
  NP_EXPECT(version.stdout_text.find("0.1.0") != std::string::npos);
}

void test_cli_compile_valid() {
  using namespace neuroparse::cli;
  std::string file_path = "examples/valid/hello.nl";
  if (!std::ifstream(file_path).good() && std::ifstream("../examples/valid/hello.nl").good()) {
    file_path = "../examples/valid/hello.nl";
  }
  const auto compile = run(parse_args({file_path}));
  NP_EXPECT(compile.exit_code == 0);
  NP_EXPECT(compile.stderr_text.empty());
}

void test_cli_compile_invalid() {
  using namespace neuroparse::cli;
  std::string file_path = "examples/syntax_errors/missing_semicolon.nl";
  if (!std::ifstream(file_path).good() && std::ifstream("../examples/syntax_errors/missing_semicolon.nl").good()) {
    file_path = "../examples/syntax_errors/missing_semicolon.nl";
  }
  const auto compile = run(parse_args({file_path}));
  NP_EXPECT(compile.exit_code == 1);
  NP_EXPECT(compile.stderr_text.find("syntax error") != std::string::npos);
}

void test_cli_recovery_flag() {
  using namespace neuroparse::cli;
  const auto opt1 = parse_args({"--recovery=panic", "broken.nl"});
  NP_EXPECT(opt1.recovery == RecoveryMode::Panic);
  NP_EXPECT(opt1.input_path == "broken.nl");

  const auto opt2 = parse_args({"--recovery-mode=neural", "broken.nl"});
  NP_EXPECT(opt2.recovery == RecoveryMode::Neural);

  const auto opt3 = parse_args({"--recovery-mode", "heuristic", "broken.nl"});
  NP_EXPECT(opt3.recovery == RecoveryMode::Heuristic);

  const auto opt4 = parse_args({"--recovery", "none", "broken.nl"});
  NP_EXPECT(opt4.recovery == RecoveryMode::None);

  const auto opt5 = parse_args({"--explain-recovery", "--trace-recovery", "broken.nl"});
  NP_EXPECT(opt5.explain_recovery == true);
  NP_EXPECT(opt5.trace_recovery == true);
}

void test_cli_dump_tokens() {
  using namespace neuroparse::cli;

  std::string file_path = "examples/valid/hello.nl";
  if (!std::ifstream(file_path).good() && std::ifstream("../examples/valid/hello.nl").good()) {
    file_path = "../examples/valid/hello.nl";
  }

  const auto res = run(parse_args({"--dump-tokens", file_path}));
  NP_EXPECT(res.exit_code == 0);
  NP_EXPECT(res.stdout_text.find("KEYWORD function \"function\"") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("IDENTIFIER IDENTIFIER \"add\"") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("EOF") != std::string::npos);

  const auto no_file = run(parse_args({"--dump-tokens"}));
  NP_EXPECT(no_file.exit_code == 1);
  NP_EXPECT(no_file.stderr_text.find("no input file") != std::string::npos);

  const auto bad_file = run(parse_args({"--dump-tokens", "non_existent_file_12345.nl"}));
  NP_EXPECT(bad_file.exit_code == 1);
  NP_EXPECT(bad_file.stderr_text.find("could not open file") != std::string::npos);
}

void test_cli_dump_ast() {
  using namespace neuroparse::cli;

  std::string file_path = "examples/valid/hello.nl";
  if (!std::ifstream(file_path).good() && std::ifstream("../examples/valid/hello.nl").good()) {
    file_path = "../examples/valid/hello.nl";
  }

  const auto res = run(parse_args({"--dump-ast", file_path}));
  NP_EXPECT(res.exit_code == 0);
  NP_EXPECT(res.stdout_text.find("Program") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("FunctionDeclaration: add(a, b)") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("VariableDeclaration: x") != std::string::npos);

  const auto no_file = run(parse_args({"--dump-ast"}));
  NP_EXPECT(no_file.exit_code == 1);
  NP_EXPECT(no_file.stderr_text.find("no input file") != std::string::npos);

  const auto bad_file = run(parse_args({"--dump-ast", "non_existent_file_12345.nl"}));
  NP_EXPECT(bad_file.exit_code == 1);
  NP_EXPECT(bad_file.stderr_text.find("could not open file") != std::string::npos);

  std::string err_file = "examples/syntax_errors/missing_semicolon.nl";
  if (!std::ifstream(err_file).good() && std::ifstream("../examples/syntax_errors/missing_semicolon.nl").good()) {
    err_file = "../examples/syntax_errors/missing_semicolon.nl";
  }
  const auto err_res = run(parse_args({"--dump-ast", err_file}));
  NP_EXPECT(err_res.exit_code == 1);
  NP_EXPECT(err_res.stderr_text.find("syntax error") != std::string::npos);
}

void test_cli_diagnostics_json() {
  using namespace neuroparse::cli;

  std::string file_path = "examples/syntax_errors/missing_semicolon.nl";
  if (!std::ifstream(file_path).good() && std::ifstream("../examples/syntax_errors/missing_semicolon.nl").good()) {
    file_path = "../examples/syntax_errors/missing_semicolon.nl";
  }

  const auto res = run(parse_args({"--diagnostics=json", "--recovery=neural", file_path}));
  NP_EXPECT(res.exit_code == 1);
  NP_EXPECT(res.stdout_text.find("\"schema_version\": \"1.0.0\"") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("\"diagnostics\": [") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("\"severity\": \"error\"") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("\"strategy\": \"neural\"") != std::string::npos);
  NP_EXPECT(res.stdout_text.find("\"suggested_edit\":") != std::string::npos);
}

