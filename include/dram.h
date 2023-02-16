#pragma once

#include "bidirectional_interface.hpp"

#include <cstdint>
#include <vector>

namespace rv64_emulator::dram {

class DRAM : public BidirectionalInterface {
private:
    const uint64_t       m_size;
    std::vector<uint8_t> m_memory;

public:
    DRAM(const uint64_t mem_size);
    bool     Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const override;
    bool     Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) override;
    uint64_t GetSize() const;
    ~DRAM() override;
};

} // namespace rv64_emulator::dram
