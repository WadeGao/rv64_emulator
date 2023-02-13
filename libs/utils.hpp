#ifndef RV64_EMULATOR_LIBS_UTILS_H_
#define RV64_EMULATOR_LIBS_UTILS_H_

#include <cstdint>
#include <type_traits>

namespace rv64_emulator::libs {

template <typename T>
std::enable_if_t<(std::is_unsigned_v<T> && sizeof(T) > sizeof(uint8_t) && sizeof(T) <= sizeof(uint64_t)), T>
MulUnsignedHi(const T a, const T b) {

    using type =
        std::conditional_t<(std::is_same_v<T, uint64_t>), uint32_t, std::conditional_t<(std::is_same_v<T, uint32_t>), uint16_t, uint8_t>>;

    constexpr uint32_t type_bits = sizeof(T) * 4;

    const T a_lo = (type)a;
    const T a_hi = a >> type_bits;
    const T b_lo = (type)b;
    const T b_hi = b >> type_bits;

    const T a_x_b_hi  = a_hi * b_hi;
    const T a_x_b_mid = a_hi * b_lo;
    const T b_x_a_mid = b_hi * a_lo;
    const T a_x_b_lo  = a_lo * b_lo;

    const T carry_bit = ((T)(type)a_x_b_mid + (T)(type)b_x_a_mid + (a_x_b_lo >> type_bits)) >> type_bits;

    const T res = a_x_b_hi + (a_x_b_mid >> type_bits) + (b_x_a_mid >> type_bits) + carry_bit;

    return res;
}

} // namespace rv64_emulator::libs

#endif
