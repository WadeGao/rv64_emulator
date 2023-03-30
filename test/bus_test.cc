#include "bus.h"
#include "conf.h"
#include "dram.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

class BusTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        fmt::print("start running Bus test case...\n");
    }

    static void TearDownTestSuite() {
        fmt::print("finish running Bus test case...\n");
    }

    void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));

        m_bus = std::move(bus);
    }

    void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::bus::Bus> m_bus;
};

TEST_F(BusTest, Load) {

    uint64_t* raw_data_ptr = reinterpret_cast<uint64_t*>(m_bus->m_dram->m_memory.data());
    *raw_data_ptr          = 0x1122334455667788;

    uint64_t res = 0;
    ASSERT_TRUE(m_bus->Load(0, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
    ASSERT_EQ(0x1122334455667788, res);
}

TEST_F(BusTest, Store) {
    constexpr uint64_t kVal = 0x1122334455667788;
    m_bus->Store(0, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&kVal));

    const uint64_t* kRawData = reinterpret_cast<uint64_t*>(m_bus->m_dram->m_memory.data());
    uint64_t        res      = 0;
    ASSERT_TRUE(m_bus->Load(0, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
    ASSERT_EQ(*kRawData, res);
}
