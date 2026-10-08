#include "neuroparse/cli/cli.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  std::vector<std::string_view> args;
  args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  const auto options = neuroparse::cli::parse_args(args);
  const auto result = neuroparse::cli::run(options);
  std::cout << result.stdout_text;
  std::cerr << result.stderr_text;
  return result.exit_code;
}
