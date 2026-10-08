#pragma once

#include <string_view>

namespace neuroparse {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

std::string_view version_string() noexcept;

}  // namespace neuroparse
