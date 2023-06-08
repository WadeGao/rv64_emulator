#include "cpu/executor.h"

#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#include "cpu/cpu.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "libs/arithmetic.hpp"
#include "libs/utils.h"

#define CHECK_MISALIGN_INSTRUCTION(pc, proc)                    \
  using rv64_emulator::libs::util::CheckPcAlign;                \
  const uint64_t kMisaVal = (proc)->state_.Read(csr::kCsrMisa); \
  const bool kNewPcAlign = CheckPcAlign((pc), kMisaVal);        \
  if (!kNewPcAlign) {                                           \
    return {                                                    \
        .type = trap::TrapType::kInstructionAddressMisaligned,  \
        .val = (pc),                                            \
    };                                                          \
  }

#define LOAD_VIRTUAL_MEMORY(size, vaddr, data, proc)               \
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&(data));              \
  const trap::Trap kLoadTrap = (proc)->Load((vaddr), (size), ptr); \
  if (kLoadTrap.type != trap::TrapType::kNone) {                   \
    return kLoadTrap;                                              \
  }

#define STORE_VIRTUAL_MEMORY(size, vaddr, data, proc)                 \
  const uint8_t* kPtr = reinterpret_cast<const uint8_t*>(&(data));    \
  const trap::Trap kStoreTrap = (proc)->Store((vaddr), (size), kPtr); \
  if (kStoreTrap.type != trap::TrapType::kNone) {                     \
    return kStoreTrap;                                                \
  }

#define CHECK_CSR_ACCESS_PRIVILEGE(csr_num, write, proc) \
  bool is_privileged = false;                            \
  if (cpu::PrivilegeMode(((csr_num) >> 8) & 0b11) <=     \
      (proc)->GetPrivilegeMode()) {                      \
    is_privileged = true;                                \
  }                                                      \
  const bool kReadOnly = ((csr_num) >> 10) == 0b11;      \
  if (!is_privileged || ((write) && kReadOnly)) {        \
    return {                                             \
        .type = trap::TrapType::kIllegalInstruction,     \
        .val = (proc)->GetPC() - 4,                      \
    };                                                   \
  }

namespace rv64_emulator::cpu::executor {

using rv64_emulator::cpu::decode::OpCode;
using rv64_emulator::libs::arithmetic::MulUnsignedHi;

const std::unordered_map<decode::InstToken, uint64_t> kAccessMemBytes{
    {decode::InstToken::LB, sizeof(int8_t)},
    {decode::InstToken::LH, sizeof(int16_t)},
    {decode::InstToken::LW, sizeof(int32_t)},
    {decode::InstToken::LD, sizeof(int64_t)},
    {decode::InstToken::LBU, sizeof(uint8_t)},
    {decode::InstToken::LHU, sizeof(uint16_t)},
    {decode::InstToken::LWU, sizeof(uint32_t)},

    {decode::InstToken::SB, sizeof(int8_t)},
    {decode::InstToken::SH, sizeof(int16_t)},
    {decode::InstToken::SW, sizeof(int32_t)},
    {decode::InstToken::SD, sizeof(int64_t)},
};

template <typename T>
static inline constexpr int64_t GetImm(const T desc) {
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

Executor::Executor(CPU* cpu) : cpu_(cpu) {}

trap::Trap Executor::RegTypeExec(const decode::DecodeResDesc desc) {
  const auto kRegDesc = *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->GetReg(kRegDesc.rs1);
  const uint64_t kU64Rs2Val = cpu_->GetReg(kRegDesc.rs2);
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
      const uint64_t kAbsRs2Val = std::abs(kRs1Val);
      const uint64_t kRes = MulUnsignedHi(kAbsRs1Val, kAbsRs2Val);
      val = kNegativeRes ? (~kRes + (kRs2Val * kRs2Val == 0)) : kRes;
    } break;
    case decode::InstToken::MULHSU: {
      const bool kNegativeRes = kRs1Val < 0;
      const uint64_t kAbsRs1Val = std::abs(kRs1Val);
      const uint64_t kRes = MulUnsignedHi(kAbsRs1Val, kU64Rs2Val);
      val = kNegativeRes ? (~kRes + (kRs1Val * kU64Rs2Val == 0)) : kRes;
    } break;
    case decode::InstToken::MULHU:
      val = MulUnsignedHi(kU64Rs1Val, kU64Rs2Val);
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
      break;
  }

  cpu_->SetReg(kRegDesc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::Rv32TypeExec(const decode::DecodeResDesc desc) {
  const auto kRegDesc = *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const int32_t kRs1Val = (int32_t)cpu_->GetReg(kRegDesc.rs1);
  const int32_t kRs2Val = (int32_t)cpu_->GetReg(kRegDesc.rs2);
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
      break;
  }
  cpu_->SetReg(kRegDesc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::ImmTypeExec(const decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->GetReg(kImmDesc.rs1);
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
      break;
  }

  cpu_->SetReg(kImmDesc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::Imm32TypeExec(const decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int32_t kRs1Val = (int32_t)cpu_->GetReg(kImmDesc.rs1);
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
      break;
  }

  cpu_->SetReg(kImmDesc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::LuiTypeExec(const decode::DecodeResDesc desc) {
  const auto kUDesc = *reinterpret_cast<const decode::UTypeDesc*>(&desc.word);
  cpu_->SetReg(kUDesc.rd, GetImm(kUDesc));
  return trap::kNoneTrap;
}

trap::Trap Executor::JalTypeExec(const decode::DecodeResDesc desc) {
  const auto kJDesc = *reinterpret_cast<const decode::JTypeDesc*>(&desc.word);
  const uint64_t kNewPC = (int64_t)desc.addr + GetImm(kJDesc);
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
  cpu_->SetReg(kJDesc.rd, desc.addr + 4);
  cpu_->SetPC(kNewPC);
  return trap::kNoneTrap;
}

trap::Trap Executor::JalrTypeExec(const decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->GetReg(kImmDesc.rs1);
  const uint64_t kNewPC = (kImmDesc.imm + kRs1Val) & 0xfffffffffffffffe;
  CHECK_MISALIGN_INSTRUCTION(kNewPC, cpu_);
  cpu_->SetPC(kNewPC);
  cpu_->SetReg(kImmDesc.rd, desc.addr + 4);
  return trap::kNoneTrap;
}

trap::Trap Executor::AuipcTypeExec(const decode::DecodeResDesc desc) {
  const auto kUDesc = *reinterpret_cast<const decode::UTypeDesc*>(&desc.word);
  const int64_t kVal = desc.addr + GetImm(kUDesc);
  cpu_->SetReg(kUDesc.rd, kVal);
  return trap::kNoneTrap;
}

trap::Trap Executor::BranchTypeExec(const decode::DecodeResDesc desc) {
  const auto kBrDesc = *reinterpret_cast<const decode::BTypeDesc*>(&desc.word);
  const uint64_t kU64Rs1Val = cpu_->GetReg(kBrDesc.rs1);
  const uint64_t kU64Rs2Val = cpu_->GetReg(kBrDesc.rs2);
  const int64_t kRs1Val = (int64_t)kU64Rs1Val;
  const int64_t kRs2Val = (int64_t)kU64Rs2Val;
  const int64_t kImm = GetImm(kBrDesc);

  uint64_t new_pc = 0;
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
      break;
  }

  CHECK_MISALIGN_INSTRUCTION(new_pc, cpu_);
  cpu_->SetPC(new_pc);
  return trap::kNoneTrap;
}

trap::Trap Executor::LoadTypeExec(const decode::DecodeResDesc desc) {
  const auto kImmDesc = *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const uint64_t kTargetAddr = desc.addr + 4 + kImmDesc.imm;

  const uint64_t kBytes = kAccessMemBytes.at(desc.token);
  uint64_t data = 0;
  LOAD_VIRTUAL_MEMORY(kBytes, kTargetAddr, data, cpu_);

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
      break;
  }

  cpu_->SetReg(kImmDesc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::StoreTypeExec(const decode::DecodeResDesc desc) {
  const auto kSDesc = *reinterpret_cast<const decode::STypeDesc*>(&desc.word);
  const int64_t kRs1Val = (int64_t)cpu_->GetReg(kSDesc.rs1);
  const uint64_t kU64Rs2Val = cpu_->GetReg(kSDesc.rs2);
  const uint64_t kTargetAddr = kRs1Val + GetImm(kSDesc);

  const uint64_t kBytes = kAccessMemBytes.at(desc.token);
  STORE_VIRTUAL_MEMORY(kBytes, kTargetAddr, kU64Rs2Val, cpu_);
  return trap::kNoneTrap;
}

trap::Trap Executor::CsrTypeExec(const decode::DecodeResDesc desc) {
  const auto kCsrDesc =
      *reinterpret_cast<const decode::CsrTypeDesc*>(&desc.word);

  bool writable = kCsrDesc.rs1 != 0;
  if (desc.token == decode::InstToken::CSRRW ||
      desc.token == decode::InstToken::CSRRWI) {
    writable = true;
  }
  CHECK_CSR_ACCESS_PRIVILEGE(kCsrDesc.imm, writable, cpu_);

  const uint64_t kCsrVal = cpu_->state_.Read(kCsrDesc.imm);
  cpu_->SetReg(kCsrDesc.rd, kCsrVal);

  const int64_t kRs1Val = (int64_t)cpu_->GetReg(kCsrDesc.rs1);
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
      break;
  }

  cpu_->state_.Write(kCsrDesc.imm, new_csr_val);
  return trap::kNoneTrap;
}

trap::Trap Executor::SystemTypeExec(const decode::DecodeResDesc desc) {
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
    default:
      break;
  }
  return ret;
}

trap::Trap Executor::Exec(const decode::DecodeResDesc desc) {
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
    default:
      break;
  }
  return ret;
}

}  // namespace rv64_emulator::cpu::executor
