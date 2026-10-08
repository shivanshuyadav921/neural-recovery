#pragma once

#include "neuroparse/recovery/recovery_result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace neuroparse::cli {

using recovery::RecoveryMode;

enum class Command {
  Help,
  Version,
  Compile,
  Check,
  DumpTokens,
  DumpAst,
  DumpContext,
  ShowRepair,
  Explain,
  Benchmark,
};

struct Options {
  Command command = Command::Compile;
  RecoveryMode recovery = RecoveryMode::Auto;
  bool explain_recovery = false;
  bool trace_recovery = false;
  bool output_json = false;
  std::string input_path;
  std::string benchmark_dir;
};

struct CliResult {
  int exit_code = 0;
  std::string stdout_text;
  std::string stderr_text;
};

Options parse_args(const std::vector<std::string_view>& args);
CliResult run(const Options& options);
std::string help_text();

}  // namespace neuroparse::cli
