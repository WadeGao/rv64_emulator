#include "bus.h"
#include "dram.h"
#include "fmt/core.h"

namespace rv64_emulator::bus {

Bus::Bus(std::unique_ptr<rv64_emulator::dram::DRAM> dram)
    : m_dram(std::move(dram)) {
#ifdef DEBUG
    printf("bus init, dram addr is %p\n", m_dram.get());
#endif
}

uint64_t Bus::Load(const uint64_t addr, const uint64_t bit_size) const {
    return m_dram->Load(addr, bit_size);
}

void Bus::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    m_dram->Store(addr, bit_size, val);
}

Bus::~Bus() {
#ifdef DEBUG
    fmt::print("destroy a bus\n");
#endif
}

} // namespace rv64_emulator::bus
