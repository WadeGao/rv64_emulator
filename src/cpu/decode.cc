#include "cpu/decode.h"

#include <cassert>
#include <cstdint>

#include "conf.h"
#include "error_code.h"
#include "fmt/core.h"

namespace rv64_emulator::cpu::decode {

enum class RV64InstructionFormatType : uint8_t {
  kTypeB,
};

// masks
static constexpr uint32_t kRegMask = 0x1f;
static constexpr uint32_t kShamtImm64Mask = 0x3f;
static constexpr uint32_t kShamtImm32Mask = 0x1f;

static uint8_t GetRd(const uint32_t inst_word) {
  const uint8_t rd = static_cast<uint8_t>(inst_word >> 7 & kRegMask);
  return rd;
}

static uint8_t GetRs1(const uint32_t inst_word) {
  const uint8_t rd = static_cast<uint8_t>(inst_word >> 15 & kRegMask);
  return rd;
}

static uint8_t GetRs2(const uint32_t inst_word) {
  const uint8_t rd = static_cast<uint8_t>(inst_word >> 20 & kRegMask);
  return rd;
}

static int32_t GetImm(const uint32_t inst_word,
                      const RV64InstructionFormatType type) {
  int32_t imm = 0;
  uint8_t imm_width = 12;

  switch (type) {
    case RV64InstructionFormatType::kTypeB:
      // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
      imm = (((inst_word & 0x80000000) >> 19) | ((inst_word & 0x80) << 4) |
             ((inst_word >> 20) & 0x7e0) | ((inst_word >> 7) & 0x1e));
      break;
    default:
      break;
  }

  bool is_negative = (inst_word >= 0x80000000);

  if (is_negative && imm_width == 12) {
    imm |= 0xfffff000;
    return imm;
  }

  return imm;
}

uint8_t GetShamt(const ITypeDesc desc, const bool is_rv32) {
  // rv64: shamt[5:0] = imm[5:0]
  // rv32: shamt[4:0] = imm[4:0]
  const uint32_t shamt_mask = is_rv32 ? kShamtImm32Mask : kShamtImm64Mask;
  const uint8_t shamt = static_cast<uint8_t>(desc.imm & shamt_mask);
  const uint8_t max_shamt = is_rv32 ? 0b11111 : 0b111111;
  assert(shamt <= max_shamt);
  return shamt;
}

FormatB ParseFormatB(const uint32_t inst_word) {
  return {
      .rs1 = GetRs1(inst_word),
      .rs2 = GetRs2(inst_word),
      .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeB),
  };
}

}  // namespace rv64_emulator::cpu::decode
