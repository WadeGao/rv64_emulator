#pragma once

#include <cstdint>

// dram config
constexpr uint64_t kDramSize = 8 * 1024 * 1024;
constexpr uint64_t kDramBaseAddr = 0x80000000;

constexpr uint64_t kPlicMaxDevices = 1024;
constexpr uint64_t kPlicBase = 0xc000000;

constexpr uint64_t kClintBase = 0x02000000;
constexpr uint64_t kClintAddrSpaceRange = 0xc000;

// cpu config
constexpr uint64_t kDecodeCacheEntryNum = 4096;
