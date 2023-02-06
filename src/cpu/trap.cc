#include "cpu/trap.h"

namespace rv64_emulator::cpu::trap {

std::tuple<bool, uint64_t> GetTrapCauseBits(const trap::Trap trap) {
    // https://dingfen.github.io/assets/img/mcause_table.png
    bool     is_interrupt = false;
    uint64_t cause_bits   = trap::kTrapToCauseTable.at(trap.m_trap_type);

    // if current trap's type is interrupt, set the msb to 1
    if (trap.m_trap_type >= trap::TrapType::kUserSoftwareInterrupt) {
        is_interrupt                 = true;
        const uint64_t interrupt_bit = static_cast<uint64_t>(1) << 63;
        cause_bits |= interrupt_bit;
    }

    return { is_interrupt, cause_bits };
}

} // namespace rv64_emulator::cpu::trap
