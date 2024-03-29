#include "cpu/trap.h"

#include "fmt/core.h"

namespace rv64_emulator::cpu::trap {

uint64_t GetTrapPC(uint64_t tvec_val, uint64_t cause) {
  const uint8_t kMode = tvec_val & 0x3;
  const uint64_t kNewPc = ((tvec_val >> 2) << 2) + 4 * cause * kMode;
  return kNewPc;
}

}  // namespace rv64_emulator::cpu::trap
