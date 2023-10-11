#include "cpu/decode.h"

#include <cassert>
#include <cstdint>
#include <type_traits>

namespace rv64_emulator::cpu::decode {

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
    0,  // aboves are invalid
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

template <typename T>
constexpr __attribute__((always_inline)) int32_t GetImm(T desc) {
  if constexpr (std::is_same_v<T, BTypeDesc>) {
    return (desc.imm12 << 12) | (desc.imm11 << 11) | (desc.imm10_5 << 5) |
           (desc.imm4_1 << 1);
  } else if constexpr (std::is_same_v<T, JTypeDesc>) {
    return (desc.imm20 << 20) | (desc.imm19_12 << 12) | (desc.imm11 << 11) |
           (desc.imm10_1 << 1);
  } else if constexpr (std::is_same_v<T, ITypeDesc>) {
    return desc.imm;
  } else if constexpr (std::is_same_v<T, STypeDesc>) {
    return desc.imm11_5 << 5 | desc.imm4_0;
  } else if constexpr (std::is_same_v<T, UTypeDesc>) {
    return desc.imm_31_12 << 12;
  } else if constexpr (std::is_same_v<T, CsrTypeDesc>) {
    return (uint32_t)desc.imm;
  }
  return INT32_MIN;
}

static InstToken GetToken(uint32_t word) {
  InstToken token = InstToken::UNKNOWN;
  for (const auto& inst : kInstTable) {
    if ((word & inst.mask) == inst.signature) {
      token = inst.token;
      break;
    }
  }
  return token;
}

DecodeInfo::DecodeInfo(uint32_t inst_word, uint64_t addr)
    : word(inst_word),
      pc(addr),
      token(GetToken(inst_word)),
      // size(!(inst_word & 0b11) ? 2 : 4),
      size(4),
      br_target(0) {
  const auto kCommDesc = *reinterpret_cast<const RTypeDesc*>(&word);
  op = static_cast<OpCode>(kCommDesc.opcode);
  rd = kCommDesc.rd;
  rs1 = kCommDesc.rs1;
  rs2 = kCommDesc.rs2;

  union ImmProxy {
    BTypeDesc b;
    JTypeDesc j;
    ITypeDesc i;
    STypeDesc s;
    UTypeDesc u;
    CsrTypeDesc csr;
  } proxy = *reinterpret_cast<ImmProxy*>(&inst_word);

  // get imm field
  switch (op) {
    case OpCode::kStore:
      imm = GetImm(proxy.s);
      break;
    case OpCode::kBranch:
      imm = GetImm(proxy.b);
      break;
    case OpCode::kAuipc:
    case OpCode::kLui:
      imm = GetImm(proxy.u);
      break;
    case OpCode::kJal:
      imm = GetImm(proxy.j);
      break;
    case OpCode::kImm:
    case OpCode::kImm32:
    case OpCode::kJalr:
    case OpCode::kLoad:
      imm = GetImm(proxy.i);
      switch (token) {
        case InstToken::SLLI:
          imm = GetShamt(proxy.i, false);
          break;
        case InstToken::SRLI:
        case InstToken::SRAI:
          imm = (int32_t)(uint32_t)GetShamt(proxy.i, false);
          break;
        case InstToken::SLLIW:
        case InstToken::SRLIW:
        case InstToken::SRAIW:
          imm = (int32_t)(uint32_t)GetShamt(proxy.i, true);
          break;
        default:
          break;
      }
      break;
    case OpCode::kSystem:
      switch (token) {
        case InstToken::CSRRW:
        case InstToken::CSRRS:
        case InstToken::CSRRC:
        case InstToken::CSRRWI:
        case InstToken::CSRRSI:
        case InstToken::CSRRCI:
          imm = GetImm(proxy.csr);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  // get memory access bytes
  switch (op) {
    case OpCode::kAmo:
      mem_size = 1 << kCommDesc.funct3;
      break;
    case OpCode::kLoad:
    case OpCode::kStore:
      mem_size = kAccessMemBytes[static_cast<uint64_t>(token)];
      break;
    default:
      break;
  }
}

uint8_t GetShamt(ITypeDesc desc, bool is_rv32) {
  // rv64: shamt[5:0] = imm[5:0]
  // rv32: shamt[4:0] = imm[4:0]
  constexpr uint32_t kShamtImm64Mask = 0x3f;
  constexpr uint32_t kShamtImm32Mask = 0x1f;
  const uint32_t kShamtMask = is_rv32 ? kShamtImm32Mask : kShamtImm64Mask;
  return static_cast<uint8_t>(desc.imm & kShamtMask);
}

}  // namespace rv64_emulator::cpu::decode
