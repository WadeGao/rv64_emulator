#include "cpu/csr.h"

#include <cstdint>

constexpr uint64_t kIsaDesc = (static_cast<uint64_t>(2) << 62) | // MXL[1:0]=2 (XLEN is 64)
                              (1 << 20) |                        // Extensions[20] (User mode implemented)
                              (1 << 18) |                        // Extensions[18] (Supervisor mode implemented)
                              (1 << 12) |                        // Extensions[12] (Integer Multiply/Divide extension)
                              (1 << 8) |                         // Extensions[8] (RV32I/64I/128I base ISA)
                              (1 << 5) |                         // Extensions[5] (Single-precision floating-point extension)
                              (1 << 3) |                         // Extensions[3] (Double-precision floating-point extension)
                              (1 << 2) |                         // Extensions[2] (Compressed extension)
                              1;

constexpr uint64_t kCsrSStatusMask = 0x80000003000de162;

namespace rv64_emulator::cpu::csr {

State::State()
    : m_csr(kCsrCapacity, 0) {
    m_csr[kCsrMisa] = kIsaDesc;
}

uint64_t State::Read(const uint64_t addr) const {
    uint64_t res = 0;

    switch (addr) {
        case kCsrSstatus:
            res = m_csr[kCsrMstatus] & kCsrSStatusMask;
            break;
        case kCsrSie:
            res = m_csr[kCsrMie] & m_csr[kCsrMideleg];
            break;
        case kCsrSip:
            res = m_csr[kCsrMip] & m_csr[kCsrMideleg];
            break;
        default:
            res = m_csr[addr];
            break;
    }

    return res;
}

void State::Write(const uint64_t addr, const uint64_t val) {
    switch (addr) {
        case kCsrSstatus:
            m_csr[csr::kCsrMstatus] &= (~kCsrSStatusMask);
            m_csr[csr::kCsrMstatus] |= (val & kCsrSStatusMask);
            break;
        case kCsrSie:
            m_csr[csr::kCsrMie] &= (~m_csr[kCsrMideleg]);
            m_csr[csr::kCsrMie] |= (val & m_csr[kCsrMideleg]);
            break;
        case kCsrSip:
            m_csr[csr::kCsrMip] &= (~m_csr[kCsrMideleg]);
            m_csr[csr::kCsrMip] |= (val & m_csr[kCsrMideleg]);
            break;
        default:
            m_csr[addr] = val;
            break;
    }
}

void State::Reset() {
    std::fill(m_csr.begin(), m_csr.end(), 0);
    m_csr[kCsrMisa] = kIsaDesc;
}

} // namespace rv64_emulator::cpu::csr