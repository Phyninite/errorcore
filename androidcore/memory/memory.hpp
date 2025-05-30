#pragma once
#include <cstdint>

namespace memory {
    auto rebaseAddress(std::uint64_t relativeAddr) -> std::uint64_t;
    auto findLibrary(const char* library) -> std::uint64_t;
}
