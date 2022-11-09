#include "include/bus.h"
#include "include/conf.h"
#include "include/dram.h"
#include "gtest/gtest.h"
#include <memory>

TEST(BusTest, Load) {
    auto                       dram     = std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE);
    rv64_emulator::dram::DRAM* raw_dram = dram.get();
    auto                       bus      = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    ASSERT_EQ(raw_dram->Load(DRAM_BASE, 64), bus->Load(DRAM_BASE, 64));
}

TEST(BusTest, Store) {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE));

    for (uint64_t i = 0; i < DRAM_SIZE; i += 8) {
        dram->Store(DRAM_BASE + i, 64, 0x1122334455667788);
        bus->Store(DRAM_BASE + i, 64, 0x1122334455667788);
    }

    for (uint64_t i = 0; i < DRAM_SIZE; i += 8) {
        const uint64_t dram_word_val = dram->Load(DRAM_BASE + i, 64);
        const uint64_t bus_word_val  = bus->Load(DRAM_BASE + i, 64);
        ASSERT_EQ(dram_word_val, bus_word_val);
    }
}
