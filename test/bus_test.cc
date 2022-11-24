#include "include/bus.h"
#include "include/conf.h"
#include "include/dram.h"
#include "gtest/gtest.h"
#include <memory>

TEST(BusTest, Load) {
    auto                       dram     = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    rv64_emulator::dram::DRAM* raw_dram = dram.get();
    auto                       bus      = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    ASSERT_EQ(raw_dram->Load(kDramBaseAddr, 64), bus->Load(kDramBaseAddr, 64));
}

TEST(BusTest, Store) {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::make_unique<rv64_emulator::dram::DRAM>(kDramSize));

    for (uint64_t i = 0; i < kDramSize; i += 8) {
        dram->Store(kDramBaseAddr + i, 64, 0x1122334455667788);
        bus->Store(kDramBaseAddr + i, 64, 0x1122334455667788);
    }

    for (uint64_t i = 0; i < kDramSize; i += 8) {
        const uint64_t dram_word_val = dram->Load(kDramBaseAddr + i, 64);
        const uint64_t bus_word_val  = bus->Load(kDramBaseAddr + i, 64);
        ASSERT_EQ(dram_word_val, bus_word_val);
    }
}
