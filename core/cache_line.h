#pragma once

#include <cstddef>

namespace hive::core {

// Separation so two atomics do not share a cache line (false sharing).
// std::hardware_destructive_interference_size is not portable: libstdc++ does
// not define it for Clang (GCC PR 89445), so clang-tidy fails. 64 is the line
// size on the x86-64 hosts this engine targets, and it matches MSVC and GCC.
inline constexpr std::size_t kCacheLineBytes = 64;

} // namespace hive::core
