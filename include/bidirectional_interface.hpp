#pragma once

#include <cstdint>

namespace rv64_emulator {

class BidirectionalInterface {
public:
    virtual bool Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const  = 0;
    virtual bool Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) = 0;

    virtual void Reset() = 0;

    virtual ~BidirectionalInterface() = default;
};

} // namespace rv64_emulator
