#include "dram.h"
#include "conf.h"

#include "fmt/core.h"

#include <cstdint>
#include <cstdlib>

namespace rv64_emulator::dram {

DRAM::DRAM(const uint64_t mem_size)
    : m_size(mem_size)
    , m_memory(mem_size, 0) {
#ifdef DEBUG
    fmt::print("dram init to all zeros, size {} bytes\n", mem_size);
#endif
}

uint64_t DRAM::GetSize() const {
    return m_size;
}

bool DRAM::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const {
    if (addr + bytes <= m_size) {
        memcpy(buffer, &m_memory[addr], bytes);
        return true;
    }

#ifdef DEBUG
    fmt::print("dram load error, addr[{:#018x}], bytes[{:#018x}], m_size[{:#018x}]\n", addr, bytes, m_size);
#endif

    return false;
}

bool DRAM::Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) {
    if (addr + bytes <= m_size) {
        memcpy(&m_memory[addr], buffer, bytes);
        return true;
    }

#ifdef DEBUG
    fmt::print("dram store error, addr[{:#018x}], bytes[{:#018x}], m_size[{:#018x}]\n", addr, bytes, m_size);
#endif

    return false;
}

DRAM::~DRAM() {
#ifdef DEBUG
    fmt::print("destroy a dram, size {} bytes\n", m_size);
#endif
}

} // namespace rv64_emulator::dram
