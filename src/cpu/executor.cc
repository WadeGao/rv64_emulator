#include "cpu/executor.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "fmt/core.h"
#include "libs/arithmetic.h"
#include "libs/utils.h"

#define MAKE_TRAP(etype, value) \
  { .type = (etype), .val = (value), }

#define ILL_TRAP(val) MAKE_TRAP(trap::TrapType::kIllegalInstruction, (val))

#define INSTR_MISALIGN_TRAP(val) \
  MAKE_TRAP(trap::TrapType::kInstructionAddressMisaligned, (val))

#define LOAD_MISALIGN_TRAP(val) \
  MAKE_TRAP(trap::TrapType::kLoadAddressMisaligned, (val))

#define STORE_MISALIGN_TRAP(val) \
  MAKE_TRAP(trap::TrapType::kStoreAddressMisaligned, (val))

#define BREAK_TRAP(val) MAKE_TRAP(trap::TrapType::kBreakpoint, (val))

#define CHECK_MISALIGN_INSTRUCTION(pc, proc)     \
  using rv64_emulator::libs::util::CheckPcAlign; \
  auto isa = (proc)->state_.Read(csr::kCsrMisa); \
  bool align = CheckPcAlign((pc), isa);          \
  if (!align) {                                  \
    return INSTR_MISALIGN_TRAP((pc));            \
  }

#define CHECK_CSR_ACCESS_PRIVILEGE(index, write, mode, word) \
  bool is_privileged = false;                                \
  if (cpu::PrivilegeMode(((index) >> 8) & 0b11) <= (mode)) { \
    is_privileged = true;                                    \
  }                                                          \
  bool read_only = ((index) >> 10) == 0b11;                  \
  if (!is_privileged || ((write) && read_only)) {            \
    return ILL_TRAP((word));                                 \
  }

namespace rv64_emulator::cpu::executor {

using rv64_emulator::cpu::decode::OpCode;
using rv64_emulator::libs::arithmetic::MulUint64Hi;

constexpr uint8_t kAccessMemBytes[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,  // belows are invalid
    [uint64_t(decode::InstToken::LB)] = sizeof(int8_t),
    [uint64_t(decode::InstToken::LH)] = sizeof(int16_t),
    [uint64_t(decode::InstToken::LW)] = sizeof(int32_t),
    [uint64_t(decode::InstToken::LBU)] = sizeof(uint8_t),
    [uint64_t(decode::InstToken::LHU)] = sizeof(uint16_t),
    [uint64_t(decode::InstToken::SB)] = sizeof(int8_t),
    [uint64_t(decode::InstToken::SH)] = sizeof(int16_t),
    [uint64_t(decode::InstToken::SW)] = sizeof(int32_t),
    [uint64_t(decode::InstToken::LWU)] = sizeof(uint32_t),
    [uint64_t(decode::InstToken::LD)] = sizeof(int64_t),
    [uint64_t(decode::InstToken::SD)] = sizeof(int64_t),
};
const std::unordered_set<uint64_t> kAllowedCsrs = {
    csr::kCsrMVendorId,  csr::kCsrMArchId,  csr::kCsrMcause,
    csr::kCsrMVendorId,  csr::kCsrMCycle,   csr::kCsrMedeleg,
    csr::kCsrMepc,       csr::kCsrMHartId,  csr::kCsrMideleg,
    csr::kCsrMie,        csr::kCsrMip,      csr::kCsrMisa,
    csr::kCsrMstatus,    csr::kCsrMtval,    csr::kCsrMtvec,
    csr::kCsrMscratch,   csr::kCsrMImpId,   csr::kCsrMinstret,
    csr::kCsrMcounteren, csr::kCsrSatp,     csr::kCsrScause,
    csr::kCsrSepc,       csr::kCsrSie,      csr::kCsrSip,
    csr::kCsrSstatus,    csr::kCsrStval,    csr::kCsrStvec,
    csr::kCsrTdata1,     csr::kCsrTselect,  csr::kCsrMconfigPtr,
    csr::kCsrScounteren, csr::kCsrSscratch,
};

template <typename T>
constexpr __attribute__((always_inline)) int64_t GetImm(T desc) {
  if constexpr (std::is_same_v<T, decode::BTypeDesc>) {
    return (desc.imm12 << 12) | (desc.imm11 << 11) | (desc.imm10_5 << 5) |
           (desc.imm4_1 << 1);
  } else if constexpr (std::is_same_v<T, decode::JTypeDesc>) {
    return (desc.imm20 << 20) | (desc.imm19_12 << 12) | (desc.imm11 << 11) |
           (desc.imm10_1 << 1);
  } else if constexpr (std::is_same_v<T, decode::ITypeDesc>) {
    return desc.imm;
  } else if constexpr (std::is_same_v<T, decode::STypeDesc>) {
    return desc.imm11_5 << 5 | desc.imm4_0;
  } else if constexpr (std::is_same_v<T, decode::UTypeDesc>) {
    return desc.imm_31_12 << 12;
  }
  return INT64_MIN;
}

void Executor::SetProcessor(CPU* cpu) { cpu_ = cpu; }

trap::Trap Executor::RegTypeExec(decode::DecodeResDesc desc) {
  const auto kRegDesc = *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[kRegDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kRegDesc.rs2];
  const int64_t kRs1Val = (int64_t)kU64Rs1Val;
  const int64_t kRs2Val = (int64_t)kU64Rs2Val;

  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADD:
      val = kRs1Val + kRs2Val;
      break;
    case decode::InstToken::SUB:
      val = kRs1Val - kRs2Val;
      break;
    case decode::InstToken::SLL:
      val = kRs1Val << kRs2Val;
      break;
    case decode::InstToken::SLT:
      val = kRs1Val < kRs2Val ? 1 : 0;
      break;
    case decode::InstToken::SLTU:
      val = kU64Rs1Val < kU64Rs2Val ? 1 : 0;
      break;
    case decode::InstToken::XOR:
      val = kRs1Val ^ kRs2Val;
      break;
    case decode::InstToken::SRL:
      val = kU64Rs1Val >> kU64Rs2Val;
      break;
    case decode::InstToken::SRA:
      val = kRs1Val >> kRs2Val;
      break;
    case decode::InstToken::OR:
      val = kRs1Val | kRs2Val;
      break;
    case decode::InstToken::AND:
      val = kRs1Val & kRs2Val;
      break;
    case decode::InstToken::MUL:
      val = kRs1Val * kRs2Val;
      break;
    case decode::InstToken::MULH: {
      const bool kNegativeRes = (kRs1Val < 0) ^ (kRs2Val < 0);
      const uint64_t kAbsRs1Val = std::abs(kRs1Val);
      const uint64_t kAbsRs2Val = std::abs(kRs2Val);
      const uint64_t kRes = MulUint64Hi(kAbsRs1Val, kAbsRs2Val);
      val = kNegativeRes ? (~kRes + (kRs1Val * kRs2Val == 0)) : kRes;
    } break;
    case decode::InstToken::MULHSU: {
      const bool kNegativeRes = kRs1Val < 0;
      const uint64_t kAbsRs1Val = std::abs(kRs1Val);
      const uint64_t kRes = MulUint64Hi(kAbsRs1Val, kU64Rs2Val);
      val = kNegativeRes ? (~kRes + (kRs1Val * kU64Rs2Val == 0)) : kRes;
    } break;
    case decode::InstToken::MULHU:
      val = MulUint64Hi(kU64Rs1Val, kU64Rs2Val);
      break;
    case decode::InstToken::DIV:
      if (kRs2Val == 0) {
        val = UINT64_MAX;
      } else if (kRs1Val == INT64_MIN && kRs2Val == -1) {
        val = INT64_MIN;
      } else {
        val = kRs1Val / kRs2Val;
      }
      break;
    case decode::InstToken::DIVU:
      val = kU64Rs2Val == 0 ? UINT64_MAX : kU64Rs1Val / kU64Rs2Val;
      break;
    case decode::InstToken::REM:
      if (kRs2Val == 0) {
        val = kRs1Val;
      } else if (kRs1Val == INT64_MIN && kRs2Val == -1) {
        val = 0;
      } else {
        val = kRs1Val % kRs2Val;
      }
      break;
    case decode::InstToken::REMU:
      val = kU64Rs2Val == 0 ? kU64Rs1Val : kU64Rs1Val % kU64Rs2Val;
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kRegDesc.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::Rv32TypeExec(decode::DecodeResDesc desc) {
  const auto kRegDesc = *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const int32_t kRs1Val = (int32_t)cpu_->reg_file_.xregs[kRegDesc.rs1];
  const int32_t kRs2Val = (int32_t)cpu_->reg_file_.xregs[kRegDesc.rs2];

  int64_t val = 0;

  switch (desc.token) {
    case decode::InstToken::ADDW:
      val = kRs1Val + kRs2Val;
      break;
    case decode::InstToken::SUBW:
      val = kRs1Val - kRs2Val;
      break;
    case decode::InstToken::SLLW:
      val = kRs1Val << kRs2Val;
      break;
    case decode::InstToken::SRLW:
      val = (int64_t)(int32_t)((uint32_t)kRs1Val >> (uint32_t)kRs2Val);
      break;
    case decode::InstToken::SRAW:
      val = kRs1Val >> kRs2Val;
      break;
    case decode::InstToken::MULW:
      val = kRs1Val * kRs2Val;
      break;
    case decode::InstToken::DIVW:
      if (kRs2Val == 0) {
        val = UINT64_MAX;
      } else if (kRs1Val == INT32_MIN && kRs2Val == -1) {
        val = INT32_MIN;
      } else {
        val = (int64_t)(kRs1Val / kRs2Val);
      }
      break;
    case decode::InstToken::DIVUW: {
      const uint32_t kArg1 = (uint32_t)kRs1Val;
      const uint32_t kArg2 = (uint32_t)kRs2Val;
      if (kArg2 == 0) {
        val = UINT64_MAX;
      } else {
        val = (int32_t)(kArg1 / kArg2);
      }
    } break;
    case decode::InstToken::REMW:
      if (kRs2Val == 0) {
        val = kRs1Val;
      } else if (kRs1Val == INT32_MIN && kRs2Val == -1) {
        val = 0;
      } else {
        val = (int64_t)((int32_t)(kRs1Val % kRs2Val));
      }
      break;
    case decode::InstToken::REMUW: {
      const uint32_t kArg1 = (uint32_t)kRs1Val;
      const uint32_t kArg2 = (uint32_t)kRs2Val;
      val = (int32_t)(kArg2 == 0 ? kArg1 : kArg1 % kArg2);
    } break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kRegDesc.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::ImmTypeExec(decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[kImmDesc.rs1];
  const int32_t kImm = kImmDesc.imm;
  const uint8_t kShamt = decode::GetShamt(kImmDesc, false);
  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADDI:
      val = kRs1Val + kImm;
      break;
    case decode::InstToken::SLTI:
      val = kRs1Val < kImm ? 1 : 0;
      break;
    case decode::InstToken::SLTIU:
      val = (uint64_t)kRs1Val < (uint64_t)kImm ? 1 : 0;
      break;
    case decode::InstToken::XORI:
      val = kRs1Val ^ kImm;
      break;
    case decode::InstToken::ORI:
      val = kRs1Val | kImm;
      break;
    case decode::InstToken::ANDI:
      val = kRs1Val & kImm;
      break;
    case decode::InstToken::SLLI:
      val = kRs1Val << (int64_t)kShamt;
      break;
    case decode::InstToken::SRLI:
      val = (uint64_t)kRs1Val >> kShamt;
      break;
    case decode::InstToken::SRAI:
      val = kRs1Val >> kShamt;
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kImmDesc.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::Imm32TypeExec(decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int32_t kRs1Val = (int32_t)cpu_->reg_file_.xregs[kImmDesc.rs1];
  const int32_t kImm = kImmDesc.imm;
  const uint8_t kShamt = decode::GetShamt(kImmDesc, true);
  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADDIW:
      val = kRs1Val + kImm;
      break;
    case decode::InstToken::SLLIW:
      val = kRs1Val << kShamt;
      break;
    case decode::InstToken::SRLIW:
      val = (int64_t)(int32_t)((uint32_t)kRs1Val >> kShamt);
      break;
    case decode::InstToken::SRAIW:
      val = kRs1Val >> kShamt;
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kImmDesc.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::LuiTypeExec(decode::DecodeResDesc desc) {
  const auto kUDesc = *reinterpret_cast<const decode::UTypeDesc*>(&desc.word);
  cpu_->reg_file_.xregs[kUDesc.rd] = GetImm(kUDesc);
  return trap::kNoneTrap;
}

trap::Trap Executor::JalTypeExec(decode::DecodeResDesc desc) {
  const auto kJDesc = *reinterpret_cast<const decode::JTypeDesc*>(&desc.word);
  const uint64_t kNewPC = (int64_t)desc.addr + GetImm(kJDesc);
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
  cpu_->reg_file_.xregs[kJDesc.rd] = desc.addr + 4;
  cpu_->pc_ = kNewPC;
  return trap::kNoneTrap;
}

trap::Trap Executor::JalrTypeExec(decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[kImmDesc.rs1];
  const uint64_t kNewPC = (kImmDesc.imm + kRs1Val) & 0xfffffffffffffffe;
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
  cpu_->pc_ = kNewPC;
  cpu_->reg_file_.xregs[kImmDesc.rd] = desc.addr + 4;
  return trap::kNoneTrap;
}

trap::Trap Executor::AuipcTypeExec(decode::DecodeResDesc desc) {
  const auto kUDesc = *reinterpret_cast<const decode::UTypeDesc*>(&desc.word);
  const int64_t kVal = desc.addr + GetImm(kUDesc);
  cpu_->reg_file_.xregs[kUDesc.rd] = kVal;
  return trap::kNoneTrap;
}

trap::Trap Executor::BranchTypeExec(decode::DecodeResDesc desc) {
  const auto kBrDesc = *reinterpret_cast<const decode::BTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[kBrDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kBrDesc.rs2];
  const int64_t kRs1Val = (int64_t)kU64Rs1Val;
  const int64_t kRs2Val = (int64_t)kU64Rs2Val;
  const int64_t kImm = GetImm(kBrDesc);

  uint64_t new_pc = desc.addr + 4;
  const uint64_t kPredictedPC = desc.addr + kImm;

  switch (desc.token) {
    case decode::InstToken::BEQ:
      if (kRs1Val == kRs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    case decode::InstToken::BNE:
      if (kRs1Val != kRs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    case decode::InstToken::BLT:
      if (kRs1Val < kRs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    case decode::InstToken::BGE:
      if (kRs1Val >= kRs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    case decode::InstToken::BLTU:
      if (kU64Rs1Val < kU64Rs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    case decode::InstToken::BGEU:
      if (kU64Rs1Val >= kU64Rs2Val) {
        new_pc = kPredictedPC;
      }
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  CHECK_MISALIGN_INSTRUCTION(new_pc, cpu_);
  cpu_->pc_ = new_pc;
  return trap::kNoneTrap;
}

trap::Trap Executor::LoadTypeExec(decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const uint64_t kTargetAddr =
      (int64_t)cpu_->reg_file_.xregs[kImmDesc.rs1] + kImmDesc.imm;
  const uint64_t kBytes = kAccessMemBytes[static_cast<uint64_t>(desc.token)];

  uint64_t data = 0;
  const auto kLoadTrap =
      cpu_->Load(kTargetAddr, kBytes, reinterpret_cast<uint8_t*>(&data));
  if (kLoadTrap.type != trap::TrapType::kNone) {
    return kLoadTrap;
  }

  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::LB:
      val = (int64_t)(int8_t)(uint8_t)data;
      break;
    case decode::InstToken::LH:
      val = (int64_t)(int16_t)(uint16_t)data;
      break;
    case decode::InstToken::LW:
      val = (int64_t)(int32_t)(uint32_t)data;
      break;
    case decode::InstToken::LD:
      val = (int64_t)data;
      break;
    case decode::InstToken::LBU:
    case decode::InstToken::LHU:
    case decode::InstToken::LWU:
      val = (int64_t)(uint64_t)data;
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kImmDesc.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::StoreTypeExec(decode::DecodeResDesc desc) {
  const auto kSDesc = *reinterpret_cast<const decode::STypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[kSDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kSDesc.rs2];
  const uint64_t kTargetAddr = kRs1Val + GetImm(kSDesc);
  const uint64_t kBytes = kAccessMemBytes[static_cast<uint64_t>(desc.token)];

  const auto kStoreTrap = cpu_->Store(
      kTargetAddr, kBytes, reinterpret_cast<const uint8_t*>(&kU64Rs2Val));
  if (kStoreTrap.type != trap::TrapType::kNone) {
    return kStoreTrap;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::CsrTypeExec(decode::DecodeResDesc desc) {
  const auto kCsrDesc =
      *reinterpret_cast<const decode::CsrTypeDesc*>(&desc.word);

  if (0 == kAllowedCsrs.count(kCsrDesc.imm)) {
    return ILL_TRAP(desc.word);
  }

  if (kCsrDesc.imm == csr::kCsrSatp) {
    const uint64_t kMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
    const auto kMstatusDesc =
        *reinterpret_cast<const csr::MstatusDesc*>(&kMstatusVal);
    if (cpu_->priv_mode_ == PrivilegeMode::kSupervisor && kMstatusDesc.tvm) {
      return ILL_TRAP(desc.word);
    }
  }

  bool writable = kCsrDesc.rs1 != 0;
  if (desc.token == decode::InstToken::CSRRW ||
      desc.token == decode::InstToken::CSRRWI) {
    writable = true;
  }
  CHECK_CSR_ACCESS_PRIVILEGE(kCsrDesc.imm, writable, cpu_->priv_mode_,
                             desc.word);

  const uint64_t kCsrVal = cpu_->state_.Read(kCsrDesc.imm);
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[kCsrDesc.rs1];
  uint64_t new_csr_val = 0;
  switch (desc.token) {
    case decode::InstToken::CSRRW:
      new_csr_val = kRs1Val;
      break;
    case decode::InstToken::CSRRS:
      new_csr_val = kRs1Val | (int64_t)kCsrVal;
      break;
    case decode::InstToken::CSRRC:
      new_csr_val = (int64_t)kCsrVal & (~kRs1Val);
      break;
    case decode::InstToken::CSRRWI:
      new_csr_val = kCsrDesc.rs1;
      break;
    case decode::InstToken::CSRRSI:
      new_csr_val = (int64_t)kCsrVal | (int64_t)kCsrDesc.rs1;
      break;
    case decode::InstToken::CSRRCI:
      new_csr_val = (int64_t)kCsrVal & (~((int64_t)kCsrDesc.rs1));
      break;
    default:
      return ILL_TRAP(desc.word);
  }

  cpu_->reg_file_.xregs[kCsrDesc.rd] = kCsrVal;
  cpu_->state_.Write(kCsrDesc.imm, new_csr_val);
  return trap::kNoneTrap;
}

trap::Trap Executor::ECallExec(decode::DecodeResDesc desc) {
  trap::TrapType etype = trap::TrapType::kNone;
  switch (cpu_->priv_mode_) {
    case PrivilegeMode::kUser:
      etype = trap::TrapType::kEnvironmentCallFromUMode;
      break;
    case PrivilegeMode::kSupervisor:
      etype = trap::TrapType::kEnvironmentCallFromSMode;
      break;
    case PrivilegeMode::kMachine:
      etype = trap::TrapType::kEnvironmentCallFromMMode;
      break;
    case PrivilegeMode::kReserved:
    default:
      break;
  }

  return MAKE_TRAP(etype, desc.addr + 4);
}

trap::Trap Executor::SfenceVmaExec(decode::DecodeResDesc desc) {
  const uint64_t kMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
  const auto kMstatusDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kMstatusVal);
  if (cpu_->priv_mode_ < PrivilegeMode::kSupervisor ||
      (cpu_->priv_mode_ == PrivilegeMode::kSupervisor && kMstatusDesc.tvm)) {
    return ILL_TRAP(desc.word);
  }

  const auto kDesc = *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const auto kVirtAddr = (int64_t)cpu_->reg_file_.xregs[kDesc.rs1];
  const auto kAsid = (int64_t)cpu_->reg_file_.xregs[kDesc.rs2];
  cpu_->FlushTlb(kVirtAddr, kAsid & 0xffff);
  return trap::kNoneTrap;
}

trap::Trap Executor::MRetExec(decode::DecodeResDesc desc) {
  const uint64_t kOriginMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
  const auto kOriginMstatusDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kOriginMstatusVal);
  if (cpu_->priv_mode_ < PrivilegeMode::kMachine) {
    return ILL_TRAP(desc.word);
  }

  csr::MstatusDesc new_mstatus_desc = kOriginMstatusDesc;
  new_mstatus_desc.mpp = 0;
  new_mstatus_desc.mpie = 1;
  new_mstatus_desc.mie = kOriginMstatusDesc.mpie;
  new_mstatus_desc.mprv = (static_cast<PrivilegeMode>(kOriginMstatusDesc.mpp) <
                                   PrivilegeMode::kMachine
                               ? 0
                               : kOriginMstatusDesc.mprv);

  const uint64_t kNewMstatusVal =
      *reinterpret_cast<const uint64_t*>(&new_mstatus_desc);
  cpu_->state_.Write(csr::kCsrMstatus, kNewMstatusVal);
  cpu_->priv_mode_ = static_cast<PrivilegeMode>(kOriginMstatusDesc.mpp);
  cpu_->pc_ = cpu_->state_.Read(csr::kCsrMepc);

  return trap::kNoneTrap;
}

trap::Trap Executor::SRetExec(decode::DecodeResDesc desc) {
  const uint64_t kOriginSstatusVal = cpu_->state_.Read(csr::kCsrSstatus);
  const auto kOriginSsDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kOriginSstatusVal);

  // 当TSR=1时，尝试在s模式下执行SRET将引发非法的指令异常
  if (cpu_->priv_mode_ < PrivilegeMode::kSupervisor || kOriginSsDesc.tsr) {
    return ILL_TRAP(desc.word);
  }

  csr::MstatusDesc new_status_desc = kOriginSsDesc;
  new_status_desc.spp = 0;
  new_status_desc.spie = 1;
  new_status_desc.sie = kOriginSsDesc.spie;
  new_status_desc.mprv =
      (static_cast<PrivilegeMode>(kOriginSsDesc.spp) < PrivilegeMode::kMachine)
          ? 0
          : kOriginSsDesc.mprv;

  const uint64_t kNewSstatusVal =
      *reinterpret_cast<const uint64_t*>(&new_status_desc);
  cpu_->state_.Write(csr::kCsrSstatus, kNewSstatusVal);
  cpu_->priv_mode_ = static_cast<PrivilegeMode>(kOriginSsDesc.spp);
  cpu_->pc_ = cpu_->state_.Read(csr::kCsrSepc);

  return trap::kNoneTrap;
}

trap::Trap Executor::SystemTypeExec(decode::DecodeResDesc desc) {
  trap::Trap ret = trap::kNoneTrap;
  switch (desc.token) {
    case decode::InstToken::CSRRW:
    case decode::InstToken::CSRRS:
    case decode::InstToken::CSRRC:
    case decode::InstToken::CSRRWI:
    case decode::InstToken::CSRRSI:
    case decode::InstToken::CSRRCI:
      ret = CsrTypeExec(desc);
      break;
    case decode::InstToken::EBREAK:
      ret = BREAK_TRAP(0);
      break;
    case decode::InstToken::ECALL:
      ret = ECallExec(desc);
      break;
    case decode::InstToken::WFI:
      // cpu_->state_.SetWfi(true);
      break;
    case decode::InstToken::SFENCE_VMA:
      ret = SfenceVmaExec(desc);
      break;
    case decode::InstToken::MRET:
      ret = MRetExec(desc);
      break;
    case decode::InstToken::SRET:
      ret = SRetExec(desc);
      break;
    default:
      return ILL_TRAP(desc.word);
  }
  return ret;
}

trap::Trap Executor::FenceTypeExec(decode::DecodeResDesc desc) {
  // TODO(wade): implement fence
  return trap::kNoneTrap;
}

trap::Trap Executor::AmoLrExec(decode::DecodeResDesc desc) {
  const auto kDesc = *reinterpret_cast<const decode::AmoTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[kDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kDesc.rs2];

  // xreg[rs2] != 0
  if (kU64Rs2Val != 0) {
    return ILL_TRAP(desc.word);
  }

  // address not align to 4 or 8 bytes
  const uint32_t kBytes = 1 << kDesc.funct3;
  if (kU64Rs1Val % kBytes != 0) {
    return LOAD_MISALIGN_TRAP(desc.addr);
  }

  reservation_ = {
      .addr = kU64Rs1Val,
      .size = kBytes,
      .valid = 1,
      .hart_id = cpu_->hart_id_,
  };

  int64_t val = 0;
  const auto kLrTrap =
      cpu_->Load(kU64Rs1Val, kBytes, reinterpret_cast<uint8_t*>(&val));
  if (kLrTrap.type != trap::TrapType::kNone) {
    return kLrTrap;
  }

  cpu_->reg_file_.xregs[kDesc.rd] = val;
  if (desc.token == decode::InstToken::LR_W) {
    cpu_->reg_file_.xregs[kDesc.rd] = (int64_t)(int32_t)(uint32_t)val;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::AmoScExec(decode::DecodeResDesc desc) {
  const auto kDesc = *reinterpret_cast<const decode::AmoTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[kDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kDesc.rs2];
  const uint32_t kBytes = 1 << kDesc.funct3;

  // address not align to 4 or 8 bytes
  if (kU64Rs1Val % kBytes != 0) {
    return STORE_MISALIGN_TRAP(desc.addr);
  }

  if (reservation_.addr == kU64Rs1Val && reservation_.size == kBytes &&
      reservation_.valid && reservation_.hart_id == cpu_->hart_id_) {
    reservation_.valid = 0;
    const auto kScTrap = cpu_->Store(
        kU64Rs1Val, kBytes, reinterpret_cast<const uint8_t*>(&kU64Rs2Val));
    if (kScTrap.type != trap::TrapType::kNone) {
      return kScTrap;
    }
    cpu_->reg_file_.xregs[kDesc.rd] = 0;
  } else {
    // 同一hart的非对应操作
    if (reservation_.hart_id == cpu_->hart_id_) {
      reservation_.valid = 0;
    }
    cpu_->reg_file_.xregs[kDesc.rd] = 1;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::AmoTypeExec(decode::DecodeResDesc desc) {
  if (desc.token == decode::InstToken::LR_W ||
      desc.token == decode::InstToken::LR_D) {
    return AmoLrExec(desc);
  }

  if (desc.token == decode::InstToken::SC_W ||
      desc.token == decode::InstToken::SC_D) {
    return AmoScExec(desc);
  }

  // An SC can only pair with the most recent LR in program order
  trap::Trap ret = trap::kNoneTrap;
  const auto kDesc = *reinterpret_cast<const decode::AmoTypeDesc*>(&desc.word);
  const uint32_t kBytes = 1 << kDesc.funct3;
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[kDesc.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[kDesc.rs2];

  if (kU64Rs1Val % kBytes != 0) {
    return LOAD_MISALIGN_TRAP(desc.addr);
  }

  uint64_t mem_rs1 = 0;
  const auto kLoadTrap =
      cpu_->Load(kU64Rs1Val, kBytes, reinterpret_cast<uint8_t*>(&mem_rs1));
  if (kLoadTrap.type != trap::TrapType::kNone) {
    return kLoadTrap;
  }

  int64_t target_val = 0;
  const int64_t kI32Rs2Val = (int64_t)(int32_t)(uint32_t)kU64Rs2Val;
  const int64_t kI32MemRs1Val = (int64_t)(int32_t)(uint32_t)mem_rs1;
  switch (desc.token) {
    case decode::InstToken::AMOSWAP_W:
      target_val = kI32Rs2Val;
      break;
    case decode::InstToken::AMOSWAP_D:
      target_val = kU64Rs2Val;
      break;
    case decode::InstToken::AMOADD_W:
      target_val = kI32Rs2Val + kI32MemRs1Val;
      break;
    case decode::InstToken::AMOADD_D:
      target_val = (int64_t)kU64Rs2Val + (int64_t)mem_rs1;
      break;
    case decode::InstToken::AMOXOR_W:
      target_val = kI32Rs2Val ^ kI32MemRs1Val;
      break;
    case decode::InstToken::AMOXOR_D:
      target_val = kU64Rs2Val ^ mem_rs1;
      break;
    case decode::InstToken::AMOAND_W:
      target_val = kI32Rs2Val & kI32MemRs1Val;
      break;
    case decode::InstToken::AMOAND_D:
      target_val = kU64Rs2Val & mem_rs1;
      break;
    case decode::InstToken::AMOOR_W:
      target_val = kI32Rs2Val | kI32MemRs1Val;
      break;
    case decode::InstToken::AMOOR_D:
      target_val = kU64Rs2Val | mem_rs1;
      break;
    case decode::InstToken::AMOMIN_W:
      target_val = std::min(kI32Rs2Val, kI32MemRs1Val);
      break;
    case decode::InstToken::AMOMIN_D:
      target_val = std::min((int64_t)kU64Rs2Val, (int64_t)mem_rs1);
      break;
    case decode::InstToken::AMOMAX_W:
      target_val = std::max(kI32Rs2Val, kI32MemRs1Val);
      break;
    case decode::InstToken::AMOMAX_D:
      target_val = std::max((int64_t)kU64Rs2Val, (int64_t)mem_rs1);
      break;
    case decode::InstToken::AMOMINU_W:
      target_val = std::min((uint32_t)kU64Rs2Val, (uint32_t)mem_rs1);
      break;
    case decode::InstToken::AMOMINU_D:
      target_val = std::min(kU64Rs2Val, mem_rs1);
      break;
    case decode::InstToken::AMOMAXU_W:
      target_val = std::max((uint32_t)kU64Rs2Val, (uint32_t)mem_rs1);
      break;
    case decode::InstToken::AMOMAXU_D:
      target_val = std::max(kU64Rs2Val, mem_rs1);
      break;
    default:
      return ILL_TRAP(desc.word);
  }
  const auto kStoreTrap = cpu_->Store(
      kU64Rs1Val, kBytes, reinterpret_cast<const uint8_t*>(&target_val));
  if (kStoreTrap.type != trap::TrapType::kNone) {
    return kStoreTrap;
  }

  cpu_->reg_file_.xregs[kDesc.rd] = kBytes == 8 ? mem_rs1 : kI32MemRs1Val;
  return ret;
}

trap::Trap Executor::Exec(decode::DecodeResDesc desc) {
  trap::Trap ret = trap::kNoneTrap;
  switch (desc.opcode) {
    case OpCode::kReg:
      ret = RegTypeExec(desc);
      break;
    case OpCode::kImm:
      ret = ImmTypeExec(desc);
      break;
    case OpCode::kLui:
      ret = LuiTypeExec(desc);
      break;
    case OpCode::kBranch:
      ret = BranchTypeExec(desc);
      break;
    case OpCode::kStore:
      ret = StoreTypeExec(desc);
      break;
    case OpCode::kLoad:
      ret = LoadTypeExec(desc);
      break;
    case OpCode::kSystem:
      ret = SystemTypeExec(desc);
      break;
    case OpCode::kAuipc:
      ret = AuipcTypeExec(desc);
      break;
    case OpCode::kJal:
      ret = JalTypeExec(desc);
      break;
    case OpCode::kJalr:
      ret = JalrTypeExec(desc);
      break;
    case OpCode::kImm32:
      ret = Imm32TypeExec(desc);
      break;
    case OpCode::kRv32:
      ret = Rv32TypeExec(desc);
      break;
    case OpCode::kFence:
      ret = FenceTypeExec(desc);
      break;
    case OpCode::kAmo:
      ret = AmoTypeExec(desc);
      break;
    default:
      ret = ILL_TRAP(desc.word);
      break;
  }
  return ret;
}

}  // namespace rv64_emulator::cpu::executor
