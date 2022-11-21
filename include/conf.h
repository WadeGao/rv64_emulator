#ifndef RV64_EMULATOR_INCLUDE_CONF_H_
#define RV64_EMULATOR_INCLUDE_CONF_H_

#include <cstdint>

// dram config
constexpr uint64_t DRAM_SIZE = 8 * 1024 * 1024;
constexpr uint64_t DRAM_BASE = 0x80000000;

// cpu config
constexpr uint64_t RV64_GENERAL_PURPOSE_REG_NUM = 32;
constexpr uint64_t RV64_INSTRUCTION_BIT_SIZE    = 32;

#endif