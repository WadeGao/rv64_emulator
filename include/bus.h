#pragma once

#include "bidirectional_interface.hpp"
#include "dram.h"

#include <memory>

namespace rv64_emulator {

namespace bus {

class Bus : public BidirectionalInterface {
private:
    std::unique_ptr<rv64_emulator::dram::DRAM> m_dram;

public:
    Bus(std::unique_ptr<rv64_emulator::dram::DRAM> dram);
    bool Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const override;
    bool Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) override;
    ~Bus() override;
};

} // namespace bus
} // namespace rv64_emulator
