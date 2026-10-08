#include "test_harness.hpp"

#include "neuroparse/version.hpp"

void test_version_string() {
  NP_EXPECT(neuroparse::version_string() == "0.1.0");
  NP_EXPECT(neuroparse::kVersionMajor == 0);
  NP_EXPECT(neuroparse::kVersionMinor == 1);
}
