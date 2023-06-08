#include "cpu/executor.h"

#include <cstdint>
#include <cstdlib>

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

namespace rv64_emulator::cpu::executor {

using rv64_emulator::cpu::decode::OpCode;
using rv64_emulator::libs::arithmetic::MulUnsignedHi;

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
  const decode::RTypeDesc r_desc =
      *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const uint64_t u64_rs1_val = cpu_->GetReg(r_desc.rs1);
  const uint64_t u64_rs2_val = cpu_->GetReg(r_desc.rs2);
  const int64_t rs1_val = (int64_t)u64_rs1_val;
  const int64_t rs2_val = (int64_t)u64_rs2_val;

  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADD:
      val = rs1_val + rs2_val;
      break;
    case decode::InstToken::SUB:
      val = rs1_val - rs2_val;
      break;
    case decode::InstToken::SLL:
      val = rs1_val << rs2_val;
      break;
    case decode::InstToken::SLT:
      val = rs1_val < rs2_val ? 1 : 0;
      break;
    case decode::InstToken::SLTU:
      val = u64_rs1_val < u64_rs2_val ? 1 : 0;
      break;
    case decode::InstToken::XOR:
      val = rs1_val ^ rs2_val;
      break;
    case decode::InstToken::SRL:
      val = u64_rs1_val >> u64_rs2_val;
      break;
    case decode::InstToken::SRA:
      val = rs1_val >> rs2_val;
      break;
    case decode::InstToken::OR:
      val = rs1_val | rs2_val;
      break;
    case decode::InstToken::AND:
      val = rs1_val & rs2_val;
      break;
    case decode::InstToken::MUL:
      val = rs1_val * rs2_val;
      break;
    case decode::InstToken::MULH: {
      const bool kNegativeRes = (rs1_val < 0) ^ (rs2_val < 0);
      const uint64_t abs_rs1_val = std::abs(rs1_val);
      const uint64_t abs_rs2_val = std::abs(rs1_val);
      const uint64_t res = MulUnsignedHi(abs_rs1_val, abs_rs2_val);
      val = kNegativeRes ? (~res + (rs2_val * rs2_val == 0)) : res;
    } break;
    case decode::InstToken::MULHSU: {
      const bool kNegativeRes = rs1_val < 0;
      const uint64_t abs_rs1_val = std::abs(rs1_val);
      const uint64_t res = MulUnsignedHi(abs_rs1_val, u64_rs2_val);
      val = kNegativeRes ? (~res + (rs1_val * u64_rs2_val == 0)) : res;
    } break;
    case decode::InstToken::MULHU:
      val = MulUnsignedHi(u64_rs1_val, u64_rs2_val);
      break;
    case decode::InstToken::DIV:
      if (rs2_val == 0) {
        val = UINT64_MAX;
      } else if (rs1_val == INT64_MIN && rs2_val == -1) {
        val = INT64_MIN;
      } else {
        val = rs1_val / rs2_val;
      }
      break;
    case decode::InstToken::DIVU:
      val = u64_rs2_val == 0 ? UINT64_MAX : u64_rs1_val / u64_rs2_val;
      break;
    case decode::InstToken::REM:
      if (rs2_val == 0) {
        val = rs1_val;
      } else if (rs1_val == INT64_MIN && rs2_val == -1) {
        val = 0;
      } else {
        val = rs1_val % rs2_val;
      }
      break;
    case decode::InstToken::REMU:
      val = u64_rs2_val == 0 ? u64_rs1_val : u64_rs1_val % u64_rs2_val;
      break;
    default:
      break;
  }

  cpu_->SetReg(r_desc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::Rv32TypeExec(const decode::DecodeResDesc desc) {
  const decode::RTypeDesc r_desc =
      *reinterpret_cast<const decode::RTypeDesc*>(&desc.word);
  const int32_t rs1_val = (int32_t)cpu_->GetReg(r_desc.rs1);
  const int32_t rs2_val = (int32_t)cpu_->GetReg(r_desc.rs2);
  int64_t val = 0;

  switch (desc.token) {
    case decode::InstToken::ADDW:
      val = rs1_val + rs2_val;
      break;
    case decode::InstToken::SUBW:
      val = rs1_val - rs2_val;
      break;
    case decode::InstToken::SLLW:
      val = rs1_val << rs2_val;
      break;
    case decode::InstToken::SRLW:
      val = (int64_t)(int32_t)((uint32_t)rs1_val >> (uint32_t)rs2_val);
      break;
    case decode::InstToken::SRAW:
      val = rs1_val >> rs2_val;
      break;
    case decode::InstToken::MULW:
      val = rs1_val * rs2_val;
      break;
    case decode::InstToken::DIVW:
      if (rs2_val == 0) {
        val = UINT64_MAX;
      } else if (rs1_val == INT32_MIN && rs2_val == -1) {
        val = INT32_MIN;
      } else {
        val = (int64_t)(rs1_val / rs2_val);
      }
      break;
    case decode::InstToken::DIVUW: {
      const uint32_t a = (uint32_t)rs1_val;
      const uint32_t b = (uint32_t)rs2_val;
      if (b == 0) {
        val = UINT64_MAX;
      } else {
        val = (int32_t)(a / b);
      }
    } break;
    case decode::InstToken::REMW:
      if (rs2_val == 0) {
        val = rs1_val;
      } else if (rs1_val == INT32_MIN && rs2_val == -1) {
        val = 0;
      } else {
        val = (int64_t)((int32_t)(rs1_val % rs2_val));
      }
      break;
    case decode::InstToken::REMUW: {
      const uint32_t a = (uint32_t)rs1_val;
      const uint32_t b = (uint32_t)rs2_val;
      val = (int32_t)(b == 0 ? a : a % b);
    } break;
    default:
      break;
  }
  cpu_->SetReg(r_desc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::ImmTypeExec(const decode::DecodeResDesc desc) {
  const decode::ITypeDesc i_desc =
      *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int64_t rs1_val = (int64_t)cpu_->GetReg(i_desc.rs1);
  const int32_t imm = i_desc.imm;
  const uint8_t kShamt = decode::GetShamt(i_desc, false);
  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADDI:
      val = rs1_val + imm;
      break;
    case decode::InstToken::SLTI:
      val = rs1_val < imm ? 1 : 0;
      break;
    case decode::InstToken::SLTIU:
      val = (uint64_t)rs1_val < (uint64_t)imm ? 1 : 0;
      break;
    case decode::InstToken::XORI:
      val = rs1_val ^ imm;
      break;
    case decode::InstToken::ORI:
      val = rs1_val | imm;
      break;
    case decode::InstToken::ANDI:
      val = rs1_val & imm;
      break;
    case decode::InstToken::SLLI:
      val = rs1_val << (int64_t)kShamt;
      break;
    case decode::InstToken::SRLI:
      val = (uint64_t)rs1_val >> kShamt;
      break;
    case decode::InstToken::SRAI:
      val = rs1_val >> kShamt;
      break;
    default:
      break;
  }

  cpu_->SetReg(i_desc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::Imm32TypeExec(const decode::DecodeResDesc desc) {
  const decode::ITypeDesc i_desc =
      *reinterpret_cast<const decode::ITypeDesc*>(&desc.word);
  const int32_t rs1_val = (int32_t)cpu_->GetReg(i_desc.rs1);
  const int32_t imm = i_desc.imm;
  const uint8_t kShamt = decode::GetShamt(i_desc, true);
  int64_t val = 0;
  switch (desc.token) {
    case decode::InstToken::ADDIW:
      val = rs1_val + imm;
      break;
    case decode::InstToken::SLLIW:
      val = rs1_val << kShamt;
      break;
    case decode::InstToken::SRLIW:
      val = (int64_t)(int32_t)((uint32_t)rs1_val >> kShamt);
      break;
    case decode::InstToken::SRAIW:
      val = rs1_val >> kShamt;
      break;
    default:
      break;
  }

  cpu_->SetReg(i_desc.rd, val);
  return trap::kNoneTrap;
}

trap::Trap Executor::LuiTypeExec(const decode::DecodeResDesc desc) {
  const decode::UTypeDesc u_desc =
      *reinterpret_cast<const decode::UTypeDesc*>(&desc.word);
  cpu_->SetReg(u_desc.rd, GetImm(u_desc));
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
      break;
    case OpCode::kStore:
      break;
    case OpCode::kLoad:
      break;
    case OpCode::kSystem:
      break;
    case OpCode::kAuipc:
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
