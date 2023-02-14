#pragma once

#include <cstdint>

// dram config
constexpr uint64_t kDramSize     = 8 * 1024 * 1024;
constexpr uint64_t kDramBaseAddr = 0x80000000;

// plic config
constexpr uint64_t kPlicBase = 0xc000000;
