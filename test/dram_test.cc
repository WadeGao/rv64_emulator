#include "include/conf.h"
#include "include/dram.h"
#include "gtest/gtest.h"

#include <cstdio>

class DRAMTest : public testing::Test { // 继承了 testing::Test
protected:
    static void SetUpTestSuite() {
        printf("start running DRAM test case...\n");
    }

    static void TearDownTestSuite() {
        printf("finish running DRAM test case...\n");
    }

    virtual void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        m_dram    = std::move(dram);
    }

    virtual void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::dram::DRAM> m_dram;
};

TEST_F(DRAMTest, GetSize) {
    ASSERT_EQ(kDramSize, m_dram->GetSize());
}

TEST_F(DRAMTest, Load) {
    for (uint64_t i = 0; i < kDramSize; i++) {
        const uint8_t byte_val = static_cast<uint8_t>(m_dram->Load(kDramBaseAddr + i, 8));
        ASSERT_EQ(byte_val, 0x00);
    }

    // Abort if remove the following comment
    // EXPECT_TRUE(m_dram->Load(kDramSize, 37) >= 0);
}

TEST_F(DRAMTest, Store) {
    for (uint64_t i = 0; i < kDramSize; i += 8) {
        m_dram->Store(kDramBaseAddr + i, 64, 0x1122334455667788);
    }

    for (uint64_t i = 0; i < kDramSize; i += 8) {
        const uint64_t word_val = m_dram->Load(kDramBaseAddr + i, 64);
        ASSERT_EQ(word_val, 0x1122334455667788);

        const uint32_t half_word_val = static_cast<uint32_t>(m_dram->Load(kDramBaseAddr + i, 32));
        ASSERT_EQ(half_word_val, 0x55667788);

        const uint16_t double_byte_val = static_cast<uint16_t>(m_dram->Load(kDramBaseAddr + i, 16));
        ASSERT_EQ(double_byte_val, 0x7788);

        const uint8_t byte_val = static_cast<uint32_t>(m_dram->Load(kDramBaseAddr + i, 8));
        ASSERT_EQ(byte_val, 0x88);
    }

    constexpr uint8_t byte_val_order[8] = { 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };
    for (uint64_t i = 0; i < kDramSize; i++) {
        const uint8_t byte_val = static_cast<uint8_t>(m_dram->Load(kDramBaseAddr + i, 8));
        ASSERT_EQ(byte_val, byte_val_order[i % 8]) << "now i is: " << i;
    }
}