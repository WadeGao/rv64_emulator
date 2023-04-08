#include "cpu/decode.h"

#include <cassert>
#include <cstdint>

namespace rv64_emulator::cpu::decode {

// masks
static constexpr uint32_t kShamtImm64Mask = 0x3f;
static constexpr uint32_t kShamtImm32Mask = 0x1f;

uint8_t GetShamt(const ITypeDesc desc, const bool is_rv32) {
  // rv64: shamt[5:0] = imm[5:0]
  // rv32: shamt[4:0] = imm[4:0]
  const uint32_t shamt_mask = is_rv32 ? kShamtImm32Mask : kShamtImm64Mask;
  const uint8_t shamt = static_cast<uint8_t>(desc.imm & shamt_mask);
  const uint8_t max_shamt = is_rv32 ? 0b11111 : 0b111111;
  assert(shamt <= max_shamt);
  return shamt;
}

}  // namespace rv64_emulator::cpu::decode
