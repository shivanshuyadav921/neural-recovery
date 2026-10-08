#pragma once

#include <cstddef>

namespace neuroparse::recovery {

struct ContextWindowConfig {
  std::size_t tokens_before = 50;
  std::size_t tokens_after = 10;

  constexpr bool operator==(const ContextWindowConfig&) const = default;
};

}  // namespace neuroparse::recovery
