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

    void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        m_dram    = std::move(dram);
    }

    void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::dram::DRAM> m_dram;
};

constexpr uint64_t kArbitraryAddr = 0;
constexpr uint64_t kArbitraryVal  = 0x1122334455667788;

TEST_F(DRAMTest, GetSize) {
    ASSERT_EQ(kDramSize, m_dram->GetSize());
}

TEST_F(DRAMTest, Store) {
    ASSERT_TRUE(m_dram->Store(kArbitraryAddr, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&kArbitraryVal)));

    const uint8_t* kRawBytes = m_dram->m_memory.data();

    const uint64_t* kDwordRawData = reinterpret_cast<const uint64_t*>(kRawBytes);
    ASSERT_EQ(*kDwordRawData, kArbitraryVal);

    ASSERT_FALSE(m_dram->Store(m_dram->GetSize() - 7, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&kArbitraryVal)));
}

TEST_F(DRAMTest, Load) {
    m_dram->Store(kArbitraryAddr, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&kArbitraryVal));

    uint64_t* kDwordRawData = reinterpret_cast<uint64_t*>(m_dram->m_memory.data());
    *kDwordRawData          = kArbitraryVal;

    uint64_t res;
    ASSERT_TRUE(m_dram->Load(kArbitraryAddr, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
    ASSERT_EQ(res, kArbitraryVal);
}

TEST_F(DRAMTest, InvalidLoadAddressRange) {
    uint64_t res;
    ASSERT_FALSE(m_dram->Load(m_dram->GetSize() - 7, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
}

TEST_F(DRAMTest, InvalidStoreAddressRange) {
    uint64_t res;
    ASSERT_FALSE(m_dram->Store(m_dram->GetSize() - 7, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&kArbitraryVal)));
}
