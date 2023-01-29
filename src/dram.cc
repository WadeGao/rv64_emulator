#include "dram.h"
#include "conf.h"
#include "fmt/core.h"

#include <cassert>
#include <cstdint>

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

uint64_t DRAM::Load(const uint64_t addr, const uint64_t bit_size) const {
    uint64_t res = 0;
    switch (bit_size) {
        case 8:
        case 16:
        case 32:
        case 64: {
            const uint64_t kByteSize       = bit_size >> 3;
            const uint64_t kRealIndexStart = addr - kDramBaseAddr;
            const uint64_t kRealIndexEnd   = kRealIndexStart + (kByteSize - 1);

            // 提前确保整个访问范围处于有效内存范围内
            // addr大于基址 && 结束地址大于起始地址且没有整数溢出 && 结束地址仍位于有效地址内
            assert(addr >= kDramBaseAddr && kRealIndexEnd >= kRealIndexStart && kRealIndexEnd < m_size);

            uint8_t mask = 0;
            for (uint64_t i = 0; i < kByteSize; i++) {
                const uint64_t kIndex = kRealIndexStart + i;
                res |= static_cast<uint64_t>(m_memory.at(kIndex)) << mask;
                mask += 8;
            }
            break;
        }
        default:
#ifdef DEBUG
            fmt::print("dram invalid load from addr [{}] bit_size [{}], now abort\n", addr, bit_size);
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
            const uint64_t kByteSize       = bit_size >> 3;
            const uint64_t kRealIndexStart = addr - kDramBaseAddr;
            const uint64_t kRealIndexEnd   = kRealIndexStart + (kByteSize - 1);

            // 提前确保整个访问范围处于有效内存范围内
            // addr大于基址 && 结束地址大于起始地址且没有整数溢出 && 结束地址仍位于有效地址内
            assert(addr >= kDramBaseAddr && kRealIndexEnd >= kRealIndexStart && kRealIndexEnd < m_size);

            for (uint64_t i = 0; i < kByteSize; i++) {
                const uint64_t kIndex = kRealIndexStart + i;
                m_memory.at(kIndex)   = static_cast<uint8_t>((val >> (i << 3)) & 0xff);
            }
            break;
        }
        default:
#ifdef DEBUG
            fmt::print("dram invalid store to addr [{}] bit_size [{}] val [{}], now abort\n", addr, bit_size, val);
#endif
            assert(false);
            break;
    }
}

DRAM::~DRAM() {
#ifdef DEBUG
    fmt::print("destroy a dram, size {} bytes\n", m_size);
#endif
}

} // namespace rv64_emulator::dram
