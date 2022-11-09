#include "include/cpu.h"
#include "include/bus.h"
#include "include/conf.h"

#include <cstdlib>
#include <iostream>

namespace rv64_emulator {
namespace cpu {

CPU::CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_pc(DRAM_BASE)
    , m_bus(std::move(bus)) {
    memset(m_reg, 0, sizeof(uint64_t) * RV64_REGISTER_NUM);
    m_reg[0] = 0;
    m_reg[2] = DRAM_BASE + DRAM_SIZE;
#ifdef DEBUG
    printf("cpu init, bus addr is %p\n", m_bus.get());
#endif
}

uint32_t CPU::Fetch() const {
    uint32_t instruction = m_bus->Load(m_pc, RV64_INST_BIT_SIZE);
    return instruction;
}

uint64_t CPU::Load(const uint64_t addr, const uint64_t bit_size) const {
    return m_bus->Load(addr, bit_size);
}

void CPU::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    m_bus->Store(addr, bit_size, val);
}

CPU::~CPU() {
#ifdef DEBUG
    printf("destroy a cpu\n");
#endif
}

} // namespace cpu
} // namespace rv64_emulator