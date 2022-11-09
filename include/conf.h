#ifndef RV64_EMULATOR_SRC_CONF_H_
#define RV64_EMULATOR_SRC_CONF_H_

#include <cstdint>

constexpr uint64_t RV64_REGISTER_NUM  = 32;
constexpr uint64_t RV64_INST_BIT_SIZE = 32;
constexpr uint64_t DRAM_SIZE          = 8 * 1024 * 1024;
constexpr uint64_t DRAM_BASE          = 0x80000000;

#endif