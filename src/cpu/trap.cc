#include "cpu/trap.h"
#include "error_code.h"

#include "fmt/core.h"

namespace rv64_emulator::cpu::trap {

uint64_t GetTrapPC(const uint64_t csr_tvec_val, const uint64_t cause) {
    const uint8_t  kMode  = csr_tvec_val & 0x3;
    const uint64_t kNewPc = (csr_tvec_val & (~0x3)) + 4 * cause * kMode;
    return kNewPc;
}

} // namespace rv64_emulator::cpu::trap
