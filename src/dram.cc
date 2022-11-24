#include "include/dram.h"
#include "include/conf.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace rv64_emulator {
namespace dram {

DRAM::DRAM(const uint64_t mem_size)
    : m_size(mem_size)
    , m_memory(std::move(std::make_unique<uint8_t[]>(mem_size))) {
    for (uint64_t i = 0; i < mem_size; i++) {
        m_memory[i] = 0;
    }

#ifdef DEBUG
    printf("dram init to all zeros, size %lu bytes\n", m_size);
#endif
}

uint64_t DRAM::GetSize() const {
    return m_size;
}

uint64_t DRAM::Load(const uint64_t addr, const uint64_t bit_size) const {
    uint64_t res = 0;
    switch (bit_size) {
        case 8:
        case 16:
        case 32:
        case 64: {
            const uint64_t byte_size = bit_size >> 3;
            uint8_t        mask      = 0;
            for (uint64_t i = 0; i < byte_size; i++) {
                res |= static_cast<uint64_t>(m_memory[addr - kDramBaseAddr + i]) << mask;
                mask += 8;
            }
            break;
        }
        default:
#ifdef DEBUG
            printf("dram invalid load from addr [%lu] bit_size [%lu], now abort\n", addr, bit_size);
#endif
            assert(false);
            break;
    }
    return res;
}

void DRAM::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    switch (bit_size) {
        case 8:
        case 16:
        case 32:
        case 64: {
            const uint64_t byte_size     = bit_size >> 3;
            uint8_t        mask          = 0;
            const uint64_t phy_addr_base = addr - kDramBaseAddr;
            for (uint64_t i = 0; i < byte_size; i++) {
                m_memory[phy_addr_base + i] = static_cast<uint8_t>((val >> (i << 3)) & 0xff);
            }
            break;
        }
        default:
#ifdef DEBUG
            printf("dram invalid store to addr [%lu] bit_size [%lu] val [%lu], now abort\n", addr, bit_size, val);
#endif
            assert(false);
            break;
    }
}

DRAM::~DRAM() {
#ifdef DEBUG
    printf("destroy a dram, size %lu bytes\n", m_size);
#endif
    m_size = 0;
}

} // namespace dram
} // namespace rv64_emulator
