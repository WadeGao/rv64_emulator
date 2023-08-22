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

void Executor::SetProcessor(CPU* cpu) { cpu_ = cpu; }

trap::Trap Executor::RegTypeExec(decode::DecodeInfo info) {
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];
  const int64_t kRs1Val = (int64_t)kU64Rs1Val;
  const int64_t kRs2Val = (int64_t)kU64Rs2Val;

  int64_t val = 0;
  switch (info.token) {
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
      if (kRs1Val == 0 || kU64Rs2Val == 0) {
        val = 0;
      } else if (kRs1Val < 0) {
        const uint64_t kAbsRs1Val = std::abs(kRs1Val);
        const uint64_t kRes = MulUint64Hi(kAbsRs1Val, kRs2Val);
        val = (~kRes);
      } else {
        val = MulUint64Hi(kRs1Val, kRs2Val);
      }
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
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::Rv32TypeExec(decode::DecodeInfo info) {
  const int32_t kRs1Val = (int32_t)cpu_->reg_file_.xregs[info.rs1];
  const int32_t kRs2Val = (int32_t)cpu_->reg_file_.xregs[info.rs2];

  int64_t val = 0;

  switch (info.token) {
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
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::ImmTypeExec(decode::DecodeInfo info) {
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[info.rs1];
  const int32_t kImm = info.imm;
  int64_t val = 0;
  switch (info.token) {
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
      val = kRs1Val << kImm;
      break;
    case decode::InstToken::SRLI:
      val = (uint64_t)kRs1Val >> (uint32_t)kImm;
      break;
    case decode::InstToken::SRAI:
      val = kRs1Val >> kImm;
      break;
    default:
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::Imm32TypeExec(decode::DecodeInfo info) {
  const int32_t kRs1Val = (int32_t)cpu_->reg_file_.xregs[info.rs1];
  const int32_t kImm = info.imm;
  int64_t val = 0;
  switch (info.token) {
    case decode::InstToken::ADDIW:
      val = kRs1Val + kImm;
      break;
    case decode::InstToken::SLLIW:
      val = kRs1Val << kImm;
      break;
    case decode::InstToken::SRLIW:
      val = (int64_t)(int32_t)((uint32_t)kRs1Val >> kImm);
      break;
    case decode::InstToken::SRAIW:
      val = kRs1Val >> kImm;
      break;
    default:
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::LuiTypeExec(decode::DecodeInfo info) {
  cpu_->reg_file_.xregs[info.rd] = info.imm;
  return trap::kNoneTrap;
}

trap::Trap Executor::JalTypeExec(decode::DecodeInfo info) {
  const uint64_t kNewPC = (int64_t)info.pc + info.imm;
#ifdef UNIT_TEST
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
#endif
  cpu_->reg_file_.xregs[info.rd] = info.pc + 4;
  cpu_->pc_ = kNewPC;
  return trap::kNoneTrap;
}

trap::Trap Executor::JalrTypeExec(decode::DecodeInfo info) {
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kNewPC = (info.imm + kRs1Val) & 0xfffffffffffffffe;
#ifdef UNIT_TEST
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
#endif
  cpu_->pc_ = kNewPC;
  cpu_->reg_file_.xregs[info.rd] = info.pc + 4;
  return trap::kNoneTrap;
}

trap::Trap Executor::AuipcTypeExec(decode::DecodeInfo info) {
  const int64_t kVal = info.pc + info.imm;
  cpu_->reg_file_.xregs[info.rd] = kVal;
  return trap::kNoneTrap;
}

trap::Trap Executor::BranchTypeExec(decode::DecodeInfo info) {
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];
  const int64_t kRs1Val = (int64_t)kU64Rs1Val;
  const int64_t kRs2Val = (int64_t)kU64Rs2Val;
  const int64_t kImm = info.imm;

  uint64_t new_pc = info.pc + 4;
  const uint64_t kPredictedPC = info.pc + kImm;

  switch (info.token) {
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
      return ILL_TRAP(info.word);
  }

#ifdef UNIT_TEST
  CHECK_MISALIGN_INSTRUCTION(new_pc, cpu_);
#endif

  cpu_->pc_ = new_pc;
  return trap::kNoneTrap;
}

trap::Trap Executor::LoadTypeExec(decode::DecodeInfo info) {
  const uint64_t kTargetAddr =
      (int64_t)cpu_->reg_file_.xregs[info.rs1] + info.imm;

  uint64_t data = 0;
  const auto kLoadTrap =
      cpu_->Load(kTargetAddr, info.mem_size, reinterpret_cast<uint8_t*>(&data));
  if (kLoadTrap.type != trap::TrapType::kNone) {
    return kLoadTrap;
  }

  int64_t val = 0;
  switch (info.token) {
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
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  return trap::kNoneTrap;
}

trap::Trap Executor::StoreTypeExec(decode::DecodeInfo info) {
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];
  const uint64_t kTargetAddr = kRs1Val + info.imm;

  const auto kStoreTrap =
      cpu_->Store(kTargetAddr, info.mem_size,
                  reinterpret_cast<const uint8_t*>(&kU64Rs2Val));
  if (kStoreTrap.type != trap::TrapType::kNone) {
    return kStoreTrap;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::CsrTypeExec(decode::DecodeInfo info) {
  const uint32_t kImm = info.imm;

#ifndef UNIT_TEST
  if (0 == kAllowedCsrs.count(kImm)) {
    return ILL_TRAP(info.word);
  }
#endif

  if (kImm == csr::kCsrSatp) {
    const uint64_t kMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
    const auto kMstatusDesc =
        *reinterpret_cast<const csr::MstatusDesc*>(&kMstatusVal);
    if (cpu_->priv_mode_ == PrivilegeMode::kSupervisor && kMstatusDesc.tvm) {
      return ILL_TRAP(info.word);
    }
  }

  bool writable = info.rs1 != 0;
  if (info.token == decode::InstToken::CSRRW ||
      info.token == decode::InstToken::CSRRWI) {
    writable = true;
  }

#ifdef UNIT_TEST
  CHECK_CSR_ACCESS_PRIVILEGE(kImm, writable, cpu_->priv_mode_, info.word);
#endif

  const uint64_t kCsrVal = cpu_->state_.Read(kImm);
  const int64_t kRs1Val = (int64_t)cpu_->reg_file_.xregs[info.rs1];
  uint64_t new_csr_val = 0;
  switch (info.token) {
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
      new_csr_val = info.rs1;
      break;
    case decode::InstToken::CSRRSI:
      new_csr_val = (int64_t)kCsrVal | (int64_t)info.rs1;
      break;
    case decode::InstToken::CSRRCI:
      new_csr_val = (int64_t)kCsrVal & (~((int64_t)info.rs1));
      break;
    default:
      return ILL_TRAP(info.word);
  }

  cpu_->reg_file_.xregs[info.rd] = kCsrVal;
  cpu_->state_.Write(kImm, new_csr_val);
  return trap::kNoneTrap;
}

trap::Trap Executor::ECallExec(decode::DecodeInfo info) {
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

  return MAKE_TRAP(etype, info.pc + 4);
}

trap::Trap Executor::SfenceVmaExec(decode::DecodeInfo info) {
  const uint64_t kMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
  const auto kMstatusDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kMstatusVal);
  if (cpu_->priv_mode_ < PrivilegeMode::kSupervisor ||
      (cpu_->priv_mode_ == PrivilegeMode::kSupervisor && kMstatusDesc.tvm)) {
    return ILL_TRAP(info.word);
  }

  const auto kVirtAddr = (int64_t)cpu_->reg_file_.xregs[info.rs1];
  const auto kAsid = (int64_t)cpu_->reg_file_.xregs[info.rs2];
  cpu_->FlushTlb(kVirtAddr, kAsid & 0xffff);
  return trap::kNoneTrap;
}

trap::Trap Executor::MRetExec(decode::DecodeInfo info) {
  const uint64_t kOriginMstatusVal = cpu_->state_.Read(csr::kCsrMstatus);
  const auto kOriginMstatusDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kOriginMstatusVal);
  if (cpu_->priv_mode_ < PrivilegeMode::kMachine) {
    return ILL_TRAP(info.word);
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

trap::Trap Executor::SRetExec(decode::DecodeInfo info) {
  const uint64_t kOriginSstatusVal = cpu_->state_.Read(csr::kCsrSstatus);
  const auto kOriginSsDesc =
      *reinterpret_cast<const csr::MstatusDesc*>(&kOriginSstatusVal);

  // 当TSR=1时，尝试在s模式下执行SRET将引发非法的指令异常
  if (cpu_->priv_mode_ < PrivilegeMode::kSupervisor || kOriginSsDesc.tsr) {
    return ILL_TRAP(info.word);
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

trap::Trap Executor::SystemTypeExec(decode::DecodeInfo info) {
  trap::Trap ret = trap::kNoneTrap;
  switch (info.token) {
    case decode::InstToken::CSRRW:
    case decode::InstToken::CSRRS:
    case decode::InstToken::CSRRC:
    case decode::InstToken::CSRRWI:
    case decode::InstToken::CSRRSI:
    case decode::InstToken::CSRRCI:
      ret = CsrTypeExec(info);
      break;
    case decode::InstToken::EBREAK:
      ret = BREAK_TRAP(0);
      break;
    case decode::InstToken::ECALL:
      ret = ECallExec(info);
      break;
    case decode::InstToken::WFI:
      // cpu_->state_.SetWfi(true);
      break;
    case decode::InstToken::SFENCE_VMA:
      ret = SfenceVmaExec(info);
      break;
    case decode::InstToken::MRET:
      ret = MRetExec(info);
      break;
    case decode::InstToken::SRET:
      ret = SRetExec(info);
      break;
    default:
      return ILL_TRAP(info.word);
  }
  return ret;
}

trap::Trap Executor::FenceTypeExec(decode::DecodeInfo info) {
  // TODO(wade): implement fence
  return trap::kNoneTrap;
}

trap::Trap Executor::AmoLrExec(decode::DecodeInfo info) {
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];

  // xreg[rs2] != 0
  if (kU64Rs2Val != 0) {
    return ILL_TRAP(info.word);
  }

  // address not align to 4 or 8 bytes
  if (kU64Rs1Val % info.mem_size != 0) {
    return LOAD_MISALIGN_TRAP(info.pc);
  }

  reservation_ = {
      .addr = kU64Rs1Val,
      .size = info.mem_size,
      .valid = 1,
      .hart_id = cpu_->hart_id_,
  };

  int64_t val = 0;
  const auto kLrTrap =
      cpu_->Load(kU64Rs1Val, info.mem_size, reinterpret_cast<uint8_t*>(&val));
  if (kLrTrap.type != trap::TrapType::kNone) {
    return kLrTrap;
  }

  cpu_->reg_file_.xregs[info.rd] = val;
  if (info.token == decode::InstToken::LR_W) {
    cpu_->reg_file_.xregs[info.rd] = (int64_t)(int32_t)(uint32_t)val;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::AmoScExec(decode::DecodeInfo info) {
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];

  // address not align to 4 or 8 bytes
  if (kU64Rs1Val % info.mem_size != 0) {
    return STORE_MISALIGN_TRAP(info.pc);
  }

  if (reservation_.addr == kU64Rs1Val && reservation_.size == info.mem_size &&
      reservation_.valid && reservation_.hart_id == cpu_->hart_id_) {
    reservation_.valid = 0;
    const auto kScTrap =
        cpu_->Store(kU64Rs1Val, info.mem_size,
                    reinterpret_cast<const uint8_t*>(&kU64Rs2Val));
    if (kScTrap.type != trap::TrapType::kNone) {
      return kScTrap;
    }
    cpu_->reg_file_.xregs[info.rd] = 0;
  } else {
    // 同一hart的非对应操作
    if (reservation_.hart_id == cpu_->hart_id_) {
      reservation_.valid = 0;
    }
    cpu_->reg_file_.xregs[info.rd] = 1;
  }

  return trap::kNoneTrap;
}

trap::Trap Executor::AmoTypeExec(decode::DecodeInfo info) {
  if (info.token == decode::InstToken::LR_W ||
      info.token == decode::InstToken::LR_D) {
    return AmoLrExec(info);
  }

  if (info.token == decode::InstToken::SC_W ||
      info.token == decode::InstToken::SC_D) {
    return AmoScExec(info);
  }

  // An SC can only pair with the most recent LR in program order
  trap::Trap ret = trap::kNoneTrap;
  const uint64_t kU64Rs1Val = cpu_->reg_file_.xregs[info.rs1];
  const uint64_t kU64Rs2Val = cpu_->reg_file_.xregs[info.rs2];

  if (kU64Rs1Val % info.mem_size != 0) {
    return LOAD_MISALIGN_TRAP(info.pc);
  }

  uint64_t mem_rs1 = 0;
  const auto kLoadTrap = cpu_->Load(kU64Rs1Val, info.mem_size,
                                    reinterpret_cast<uint8_t*>(&mem_rs1));
  if (kLoadTrap.type != trap::TrapType::kNone) {
    return kLoadTrap;
  }

  int64_t target_val = 0;
  const int64_t kI32Rs2Val = (int64_t)(int32_t)(uint32_t)kU64Rs2Val;
  const int64_t kI32MemRs1Val = (int64_t)(int32_t)(uint32_t)mem_rs1;
  switch (info.token) {
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
      return ILL_TRAP(info.word);
  }
  const auto kStoreTrap = cpu_->Store(
      kU64Rs1Val, info.mem_size, reinterpret_cast<const uint8_t*>(&target_val));
  if (kStoreTrap.type != trap::TrapType::kNone) {
    return kStoreTrap;
  }

  cpu_->reg_file_.xregs[info.rd] = info.mem_size == 8 ? mem_rs1 : kI32MemRs1Val;
  return ret;
}

trap::Trap Executor::Exec(decode::DecodeInfo info) {
  trap::Trap ret = trap::kNoneTrap;
  switch (info.op) {
    case OpCode::kReg:
      ret = RegTypeExec(info);
      break;
    case OpCode::kImm:
      ret = ImmTypeExec(info);
      break;
    case OpCode::kLui:
      ret = LuiTypeExec(info);
      break;
    case OpCode::kBranch:
      ret = BranchTypeExec(info);
      break;
    case OpCode::kStore:
      ret = StoreTypeExec(info);
      break;
    case OpCode::kLoad:
      ret = LoadTypeExec(info);
      break;
    case OpCode::kSystem:
      ret = SystemTypeExec(info);
      break;
    case OpCode::kAuipc:
      ret = AuipcTypeExec(info);
      break;
    case OpCode::kJal:
      ret = JalTypeExec(info);
      break;
    case OpCode::kJalr:
      ret = JalrTypeExec(info);
      break;
    case OpCode::kImm32:
      ret = Imm32TypeExec(info);
      break;
    case OpCode::kRv32:
      ret = Rv32TypeExec(info);
      break;
    case OpCode::kFence:
      ret = FenceTypeExec(info);
      break;
    case OpCode::kAmo:
      ret = AmoTypeExec(info);
      break;
    default:
      ret = ILL_TRAP(info.word);
      break;
  }
  return ret;
}

}  // namespace rv64_emulator::cpu::executor
