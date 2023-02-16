#include "bus.h"
#include "dram.h"

#include "fmt/core.h"
#include "fmt/format.h"

namespace rv64_emulator::bus {

Bus::Bus(std::unique_ptr<rv64_emulator::dram::DRAM> dram)
    : m_dram(std::move(dram)) {
#ifdef DEBUG
    fmt::print("bus init, dram addr is {}\n", fmt::ptr(m_dram.get()));
#endif
}

bool Bus::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const {
    return m_dram->Load(addr, bytes, buffer);
}

bool Bus::Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) {
    return m_dram->Store(addr, bytes, buffer);
}

Bus::~Bus() {
#ifdef DEBUG
    fmt::print("destroy a bus\n");
#endif
}

} // namespace rv64_emulator::bus
