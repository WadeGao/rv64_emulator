#include "libs/arithmetic.h"

#include <cstdint>
#include <type_traits>

namespace rv64_emulator::libs::arithmetic {

uint64_t MulUint64Hi(uint64_t a, uint64_t b) {
  uint64_t res = 0;

#ifdef __aarch64__
  asm volatile("umulh %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
  return res;
#endif

#ifdef __x86_64__
  asm volatile(
      "mulq %2;"
      "movq %%rdx, %0;"
      : "=r"(res)
      : "a"(a), "r"(b)
      : "rdx");
  return res;
#endif

#ifdef __riscv_xlen
#if __riscv_xlen == 64
  asm volatile("mulhu %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
  return res;
#endif
#endif

  return PortableMulUnsignedHi(a, b);
}

}  // namespace rv64_emulator::libs::arithmetic
