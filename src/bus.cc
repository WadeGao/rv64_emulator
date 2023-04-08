#include "bus.h"

#include "dram.h"

namespace rv64_emulator::bus {

Bus::Bus(std::unique_ptr<DRAM> dram) : dram_(std::move(dram)) {}

bool Bus::Load(const uint64_t addr, const uint64_t bytes,
               uint8_t* buffer) const {
  return dram_->Load(addr, bytes, buffer);
}

bool Bus::Store(const uint64_t addr, const uint64_t bytes,
                const uint8_t* buffer) {
  return dram_->Store(addr, bytes, buffer);
}

void Bus::Reset() { dram_->Reset(); }

}  // namespace rv64_emulator::bus
