#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace neuroparse::test {

inline int g_failures = 0;

inline void expect(bool cond, std::string_view expr, std::string_view file, int line) {
  if (!cond) {
    ++g_failures;
    std::cerr << file << ":" << line << ": expectation failed: " << expr << "\n";
  }
}

}  // namespace neuroparse::test

#define NP_EXPECT(cond) \
  ::neuroparse::test::expect(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
