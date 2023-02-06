#ifndef RV64_EMULATOR_INCLUDE_CONF_H_
#define RV64_EMULATOR_INCLUDE_CONF_H_

#include <cstdint>

// dram config
constexpr uint64_t kDramSize     = 8 * 1024 * 1024;
constexpr uint64_t kDramBaseAddr = 0x80000000;

// plic config
constexpr uint64_t kPlicBase = 0xc000000;

#endif