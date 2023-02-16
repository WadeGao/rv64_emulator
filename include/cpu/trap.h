#pragma once

#include <cstdint>
#include <map>

namespace rv64_emulator::cpu::trap {

enum class TrapType {
    kInstructionAddressMisaligned = 0,
    kInstructionAccessFault,
    kIllegalInstruction,
    kBreakpoint,
    kLoadAddressMisaligned,
    kLoadAccessFault,
    kStoreAddressMisaligned,
    kStoreAccessFault,
    kEnvironmentCallFromUMode,
    kEnvironmentCallFromSMode,
    kReserved,
    kEnvironmentCallFromMMode,
    kInstructionPageFault,
    kLoadPageFault,
    kReservedForFutureStandard,
    kStorePageFault,
    /* ----------- belows are interrupts: software, timer, external----------- */
    kUserSoftwareInterrupt,
    kSupervisorSoftwareInterrupt,
    kMachineSoftwareInterrupt,
    kUserTimerInterrupt,
    kSupervisorTimerInterrupt,
    kMachineTimerInterrupt,
    kUserExternalInterrupt,
    kSupervisorExternalInterrupt,
    kMachineExternalInterrupt,
    kNone
};

const std::map<TrapType, uint64_t> kTrapToCauseTable = {
    { TrapType::kInstructionAddressMisaligned, 0 },
    { TrapType::kInstructionAccessFault, 1 },
    { TrapType::kIllegalInstruction, 2 },
    { TrapType::kBreakpoint, 3 },
    { TrapType::kLoadAddressMisaligned, 4 },
    { TrapType::kLoadAccessFault, 5 },
    { TrapType::kStoreAddressMisaligned, 6 },
    { TrapType::kStoreAccessFault, 7 },
    { TrapType::kEnvironmentCallFromUMode, 8 },
    { TrapType::kEnvironmentCallFromSMode, 9 },
    { TrapType::kEnvironmentCallFromMMode, 11 },
    { TrapType::kInstructionPageFault, 12 },
    { TrapType::kLoadPageFault, 13 },
    { TrapType::kStorePageFault, 15 },
    /* ----------- belows are interrupts ----------- */
    { TrapType::kUserSoftwareInterrupt, 0 },
    { TrapType::kSupervisorSoftwareInterrupt, 1 },
    { TrapType::kMachineSoftwareInterrupt, 3 },
    { TrapType::kUserTimerInterrupt, 4 },
    { TrapType::kSupervisorTimerInterrupt, 5 },
    { TrapType::kMachineTimerInterrupt, 7 },
    { TrapType::kUserExternalInterrupt, 8 },
    { TrapType::kSupervisorExternalInterrupt, 9 },
    { TrapType::kMachineExternalInterrupt, 11 },
};

using Trap = struct Trap {
    TrapType m_trap_type;
    uint64_t m_val;
};

constexpr Trap kNoneTrap = {
    .m_trap_type = trap::TrapType::kNone,
    .m_val       = 0,
};

uint64_t GetTrapPC(const uint64_t csr_tvec_addr, const uint64_t cause);

} // namespace rv64_emulator::cpu::trap
