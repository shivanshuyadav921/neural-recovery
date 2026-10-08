#include "test_harness.hpp"

void test_version_string();
void test_cli_help_and_version();
void test_cli_compile_valid();
void test_cli_compile_invalid();
void test_cli_recovery_flag();
void test_cli_dump_tokens();
void test_cli_dump_ast();
void test_cli_diagnostics_json();
void test_lexer_all();
void test_parser_all();
void test_ast_all();
void run_recovery_tests();
void run_context_extractor_tests();
void run_repair_validator_tests();
void run_repair_applier_tests();
void run_neural_recovery_tests();
void run_fuzz_unit_tests();

int main(int argc, char* argv[]) {
  std::string_view suite = "all";
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg.rfind("--suite=", 0) == 0) {
      suite = arg.substr(8);
    } else if (arg != "--help" && arg != "-h") {
      suite = arg;
    }
  }

  bool run_all = (suite == "all");

  if (run_all || suite == "version") {
    test_version_string();
  }
  if (run_all || suite == "cli") {
    test_cli_help_and_version();
    test_cli_compile_valid();
    test_cli_compile_invalid();
    test_cli_recovery_flag();
    test_cli_dump_tokens();
    test_cli_dump_ast();
    test_cli_diagnostics_json();
  }
  if (run_all || suite == "lexer") {
    test_lexer_all();
  }
  if (run_all || suite == "parser") {
    test_parser_all();
  }
  if (run_all || suite == "ast") {
    test_ast_all();
  }
  if (run_all || suite == "recovery") {
    run_recovery_tests();
  }
  if (run_all || suite == "context_extractor") {
    run_context_extractor_tests();
  }
  if (run_all || suite == "repair_validator") {
    run_repair_validator_tests();
  }
  if (run_all || suite == "repair_applier") {
    run_repair_applier_tests();
  }
  if (run_all || suite == "neural_recovery") {
    run_neural_recovery_tests();
  }
  if (run_all || suite == "fuzz") {
    run_fuzz_unit_tests();
  }

  if (neuroparse::test::g_failures != 0) {
    std::cerr << neuroparse::test::g_failures << " test(s) failed in suite '" << suite << "'\n";
    return EXIT_FAILURE;
  }
  std::cout << "Unit test suite '" << suite << "' passed cleanly\n";
  return EXIT_SUCCESS;
}
