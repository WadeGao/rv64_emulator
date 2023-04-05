#include "bus.h"

#include "dram.h"
#include "fmt/core.h"
#include "fmt/format.h"

namespace rv64_emulator::bus {

Bus::Bus(std::unique_ptr<DRAM> dram) : dram_(std::move(dram)) {
#ifdef DEBUG
  fmt::print("bus init, dram addr is {}\n", fmt::ptr(dram_.get()));
#endif
}

bool Bus::Load(const uint64_t addr, const uint64_t bytes,
               uint8_t* buffer) const {
  return dram_->Load(addr, bytes, buffer);
}

bool Bus::Store(const uint64_t addr, const uint64_t bytes,
                const uint8_t* buffer) {
  return dram_->Store(addr, bytes, buffer);
}

void Bus::Reset() { dram_->Reset(); }

Bus::~Bus() {
#ifdef DEBUG
  fmt::print("destroy a bus\n");
#endif
}

}  // namespace rv64_emulator::bus
