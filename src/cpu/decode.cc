#include "cpu/decode.h"

#include <cassert>
#include <cstdint>

namespace rv64_emulator::cpu::decode {

uint8_t GetShamt(const ITypeDesc desc, const bool is_rv32) {
  // rv64: shamt[5:0] = imm[5:0]
  // rv32: shamt[4:0] = imm[4:0]
  constexpr uint32_t kShamtImm64Mask = 0x3f;
  constexpr uint32_t kShamtImm32Mask = 0x1f;
  const uint32_t kShamtMask = is_rv32 ? kShamtImm32Mask : kShamtImm64Mask;
  const uint8_t kShamt = static_cast<uint8_t>(desc.imm & kShamtMask);
  const uint8_t kMaxShamt = is_rv32 ? 0b11111 : 0b111111;
  return kShamt;
}

}  // namespace rv64_emulator::cpu::decode
