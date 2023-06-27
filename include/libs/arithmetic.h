#pragma once

#include <cstdint>
#include <type_traits>

namespace rv64_emulator::libs::arithmetic {

template <typename T>
std::enable_if_t<(std::is_unsigned_v<T> && sizeof(T) > sizeof(uint8_t) &&
                  sizeof(T) <= sizeof(uint64_t)),
                 T>
PortableMulUnsignedHi(const T a, const T b) {
  using type = std::conditional_t<
      (std::is_same_v<T, uint64_t>), uint32_t,
      std::conditional_t<(std::is_same_v<T, uint32_t>), uint16_t, uint8_t>>;

  constexpr uint32_t kTypeBits = sizeof(T) * 4;

  T a_lo = (type)a;
  T a_hi = a >> kTypeBits;
  T b_lo = (type)b;
  T b_hi = b >> kTypeBits;

  T a_x_b_hi = a_hi * b_hi;
  T a_x_b_mid = a_hi * b_lo;
  T b_x_a_mid = b_hi * a_lo;
  T a_x_b_lo = a_lo * b_lo;

  const T kCarryBit =
      ((T)(type)a_x_b_mid + (T)(type)b_x_a_mid + (a_x_b_lo >> kTypeBits)) >>
      kTypeBits;

  const T kRes = a_x_b_hi + (a_x_b_mid >> kTypeBits) +
                 (b_x_a_mid >> kTypeBits) + kCarryBit;

  return kRes;
}

uint64_t MulUint64Hi(uint64_t a, uint64_t b);

}  // namespace rv64_emulator::libs::arithmetic
