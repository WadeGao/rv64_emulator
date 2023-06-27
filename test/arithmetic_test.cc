#include "libs/arithmetic.h"

#include <cstdint>

#include "gtest/gtest.h"

class MulUnsignedHiTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

using rv64_emulator::libs::arithmetic::MulUint64Hi;
using rv64_emulator::libs::arithmetic::PortableMulUnsignedHi;

TEST_F(MulUnsignedHiTest, MulUnsignedHiUint16) {
  ASSERT_EQ(PortableMulUnsignedHi<uint16_t>(0x1234, 0x5678), 0x0626);
}

TEST_F(MulUnsignedHiTest, MulUnsignedHiUint32) {
  ASSERT_EQ(PortableMulUnsignedHi<uint32_t>(0x12345678, 0x9ABCDEF0),
            0x0b00ea4e);
}

TEST_F(MulUnsignedHiTest, MulUnsignedHiUint64) {
  uint64_t a = 0x123456789ABCDEF0;
  ASSERT_EQ(MulUint64Hi(a, a), 0x014B66DC33F6ACDC);

  uint64_t b = 0x1FC93A3B2D7E4586;
  ASSERT_EQ(MulUint64Hi(a, b), 0x0242A5B49051CC0B);

  a = 0x3227eb79ad9e08a3;
  b = 0xb0d0d92bca4332d6;
  ASSERT_EQ(MulUint64Hi(a, b), 0x22A45CDF65291228);
}
