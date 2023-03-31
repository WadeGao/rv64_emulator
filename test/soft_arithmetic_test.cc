
#include "libs/arithmetic.hpp"

#include "gtest/gtest.h"

class MulUnsignedHiTest : public testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(MulUnsignedHiTest, MulUnsignedHi_uint16_t) {
    using T   = uint16_t;
    const T a = 0x1234;
    const T b = 0x5678;
    ASSERT_EQ(rv64_emulator::libs::arithmetic::MulUnsignedHi(a, b), 0x626);
}

TEST_F(MulUnsignedHiTest, MulUnsignedHi_uint32_t) {
    using T   = uint32_t;
    const T a = 0x12345678;
    const T b = 0x9ABCDEF0;
    ASSERT_EQ(rv64_emulator::libs::arithmetic::MulUnsignedHi(a, b), 0xb00ea4e);
}

TEST_F(MulUnsignedHiTest, MulUnsignedHi_uint64_t) {
    using T   = uint64_t;
    const T a = 0x123456789ABCDEF0;
    const T b = 0x123456789ABCDEF0;
    ASSERT_EQ(rv64_emulator::libs::arithmetic::MulUnsignedHi(a, b), 0x14b66dc33f6acdc);
}
