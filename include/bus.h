#pragma once

#include "dram.h"
#include "mmio_device.hpp"

#include <memory>

namespace rv64_emulator {

namespace bus {

class Bus : public MmioDevice {
private:
    std::unique_ptr<rv64_emulator::dram::DRAM> m_dram;

public:
    Bus(std::unique_ptr<rv64_emulator::dram::DRAM> dram);
    bool Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const override;
    bool Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) override;
    void Reset() override;
    ~Bus() override;
};

} // namespace bus
} // namespace rv64_emulator
