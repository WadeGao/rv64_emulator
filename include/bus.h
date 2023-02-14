#pragma once

#include "dram.h"

#include <memory>

namespace rv64_emulator::bus {

class Bus {
private:
    std::unique_ptr<rv64_emulator::dram::DRAM> m_dram;

public:
    Bus(std::unique_ptr<rv64_emulator::dram::DRAM> dram);
    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);
    ~Bus();
};

} // namespace rv64_emulator::bus
