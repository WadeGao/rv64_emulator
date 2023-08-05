#pragma once

#include <cstdint> 

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
  kEnvironmentCallFromMMode,

  kInstructionPageFault,
  kLoadPageFault,
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

constexpr uint64_t kTrapToCauseTable[] = {
    [uint64_t(TrapType::kInstructionAddressMisaligned)] = 0,
    [uint64_t(TrapType::kInstructionAccessFault)] = 1,
    [uint64_t(TrapType::kIllegalInstruction)] = 2,
    [uint64_t(TrapType::kBreakpoint)] = 3,
    [uint64_t(TrapType::kLoadAddressMisaligned)] = 4,
    [uint64_t(TrapType::kLoadAccessFault)] = 5,
    [uint64_t(TrapType::kStoreAddressMisaligned)] = 6,
    [uint64_t(TrapType::kStoreAccessFault)] = 7,
    [uint64_t(TrapType::kEnvironmentCallFromUMode)] = 8,
    [uint64_t(TrapType::kEnvironmentCallFromSMode)] = 9,
    [uint64_t(TrapType::kEnvironmentCallFromMMode)] = 11,
    [uint64_t(TrapType::kInstructionPageFault)] = 12,
    [uint64_t(TrapType::kLoadPageFault)] = 13,
    [uint64_t(TrapType::kStorePageFault)] = 15,
    /* ----------- belows are interrupts ----------- */
    [uint64_t(TrapType::kUserSoftwareInterrupt)] = 0,
    [uint64_t(TrapType::kSupervisorSoftwareInterrupt)] = 1,
    [uint64_t(TrapType::kMachineSoftwareInterrupt)] = 3,
    [uint64_t(TrapType::kUserTimerInterrupt)] = 4,
    [uint64_t(TrapType::kSupervisorTimerInterrupt)] = 5,
    [uint64_t(TrapType::kMachineTimerInterrupt)] = 7,
    [uint64_t(TrapType::kUserExternalInterrupt)] = 8,
    [uint64_t(TrapType::kSupervisorExternalInterrupt)] = 9,
    [uint64_t(TrapType::kMachineExternalInterrupt)] = 11,
};

using Trap = struct Trap {
  TrapType type;
  uint64_t val;
};

constexpr Trap kNoneTrap = {
    .type = trap::TrapType::kNone,
    .val = 0,
};

uint64_t GetTrapPC(uint64_t tvec_val, uint64_t cause);

}  // namespace rv64_emulator::cpu::trap
