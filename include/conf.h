#pragma once

#include <cstdint>

// dram config
constexpr uint64_t kDramSize = 2u * 1024 * 1024 * 1024;
constexpr uint64_t kDramBaseAddr = 0x80000000;

constexpr uint64_t kPlicMaxDevices = 1024;
constexpr uint64_t kPlicBase = 0xc000000;
constexpr uint64_t kPlicAddrSpaceRange = 0x4000000;

constexpr uint64_t kClintBase = 0x2000000;
constexpr uint64_t kClintAddrSpaceRange = 0x10000;

constexpr uint64_t kUartBase = 0x60100000;
constexpr uint64_t kUartAddrSpaceRange = 0x1000;

// cpu config
constexpr uint64_t kDecodeCacheEntryNum = 4096;
