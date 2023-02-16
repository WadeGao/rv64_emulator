#include "conf.h"
#include "dram.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

class DRAMTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        fmt::print("start running DRAM test case...\n");
    }

    static void TearDownTestSuite() {
        fmt::print("finish running DRAM test case...\n");
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

TEST_F(DRAMTest, Store) {

    m_dram->Store(kDramBaseAddr, 64, 0x1122334455667788);

    const uint64_t word_val = m_dram->Load(kDramBaseAddr, 64);
    ASSERT_EQ(word_val, 0x1122334455667788);

    const uint32_t half_word_val = static_cast<uint32_t>(m_dram->Load(kDramBaseAddr, 32));
    ASSERT_EQ(half_word_val, 0x55667788);

    const uint16_t double_byte_val = static_cast<uint16_t>(m_dram->Load(kDramBaseAddr, 16));
    ASSERT_EQ(double_byte_val, 0x7788);

    const uint8_t byte_val = static_cast<uint32_t>(m_dram->Load(kDramBaseAddr, 8));
    ASSERT_EQ(byte_val, 0x88);

    constexpr uint8_t byte_val_order[8] = { 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };
    for (uint64_t i = 0; i < sizeof(byte_val_order); i++) {
        const uint8_t byte_val = static_cast<uint8_t>(m_dram->Load(kDramBaseAddr + i, 8));
        ASSERT_EQ(byte_val, byte_val_order[i]) << "now i is: " << i;
    }
}

TEST_F(DRAMTest, Load) {
    constexpr uint64_t kDramAddr = 0x80000000;
    m_dram->Store(kDramAddr, 64, 0x1234567890abcdef);

    EXPECT_EQ(m_dram->Load(kDramAddr, 8), 0xef);
    EXPECT_EQ(m_dram->Load(kDramAddr, 16), 0xcdef);
    EXPECT_EQ(m_dram->Load(kDramAddr, 32), 0x90abcdef);
    EXPECT_EQ(m_dram->Load(kDramAddr, 64), 0x1234567890abcdef);
}

// #ifdef DEBUG
TEST_F(DRAMTest, InvalidLoadBitWidth) {
    constexpr uint64_t kDramAddr = 0x80000000;
    ASSERT_EXIT(m_dram->Load(kDramAddr, 31), !testing::ExitedWithCode(0), "");
}

TEST_F(DRAMTest, InvalidStoreBitWidth) {
    constexpr uint64_t kDramAddr = 0x80000000;
    ASSERT_EXIT(m_dram->Store(kDramAddr, 63, 0x1234567890abcdef), !testing::ExitedWithCode(0), "");
}

TEST_F(DRAMTest, InvalidLoadAddressRange) {
    ASSERT_EXIT(m_dram->Load(kDramBaseAddr - 1, 32), !testing::ExitedWithCode(0), "");
}

TEST_F(DRAMTest, InvalidStoreAddressRange) {
    ASSERT_EXIT(m_dram->Store(kDramBaseAddr - 1, 64, 0x1234567890abcdef), !testing::ExitedWithCode(0), "");
}
// #endif
