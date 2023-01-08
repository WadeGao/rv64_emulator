#include "fmt/core.h"
#include "include/bus.h"
#include "include/conf.h"
#include "include/dram.h"
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

    virtual void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));

        m_bus = std::move(bus);
    }

    virtual void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::bus::Bus> m_bus;
};

TEST_F(BusTest, Load) {
    for (uint64_t i = 0; i < kDramSize; i += 8) {
        uint64_t* raw_data_ptr = reinterpret_cast<uint64_t*>(&(m_bus->m_dram->m_memory[i]));
        *raw_data_ptr          = 0x1122334455667788;
    }

    for (uint64_t i = 0; i < kDramSize; i += 8) {
        const uint64_t bus_word_val = m_bus->Load(kDramBaseAddr + i, 64);
        ASSERT_EQ(0x1122334455667788, bus_word_val);
    }
}

TEST_F(BusTest, Store) {
    for (uint64_t i = 0; i < kDramSize; i += 8) {
        m_bus->Store(kDramBaseAddr + i, 64, 0x1122334455667788);
    }

    for (uint64_t i = 0; i < kDramSize; i += 8) {
        const uint64_t* raw_data_ptr = reinterpret_cast<uint64_t*>(&(m_bus->m_dram->m_memory[i]));
        const uint64_t  bus_word_val = m_bus->Load(kDramBaseAddr + i, 64);
        ASSERT_EQ(*raw_data_ptr, bus_word_val);
    }
}
