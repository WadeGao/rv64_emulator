#include "cpu/csr.h"
#include "libs/software_arithmetic.hpp"

#include <cstdint>

namespace rv64_emulator::cpu::csr {

State::State()
    : m_csr(kCsrCapacity, 0) {
    // set up MISA description
    MisaDesc* misa_desc = reinterpret_cast<MisaDesc*>(&m_csr[kCsrMisa]);
    misa_desc->mxl      = static_cast<uint64_t>(RiscvMXL::kRv64);
    misa_desc->U        = 1; // User mode implemented
    misa_desc->S        = 1; // Supervisor mode implemented
    misa_desc->M        = 1; // Integer Multiply/Divide extension implemented
    misa_desc->I        = 1; // RV32I/64I/128I base ISA implemented
    misa_desc->F        = 0; // Single-precision floating-point extension implemented
    misa_desc->D        = 0; // Double-precision floating-point extension implemented
    misa_desc->C        = 0; // Compressed extension implemented
    misa_desc->A        = 0; // Atomic extension implemented

    // set up Mstatus val
    MstatusDesc* mstatus_desc = reinterpret_cast<MstatusDesc*>(&m_csr[kCsrMstatus]);
    mstatus_desc->sxl         = static_cast<uint64_t>(RiscvMXL::kRv64);
    mstatus_desc->uxl         = static_cast<uint64_t>(RiscvMXL::kRv64);
}

uint64_t State::Read(const uint64_t addr) const {
    uint64_t res = 0;

    switch (addr) {
        case kCsrSstatus:
            res = Read(kCsrMstatus) & kCsrMstatusMaskSStatus;
            break;
        case kCsrSie:
            res = m_csr[kCsrMie] & m_csr[kCsrMideleg];
            break;
        case kCsrSip:
            res = m_csr[kCsrMip] & m_csr[kCsrMideleg];
            break;
        case kCsrMstatus: {
            MstatusDesc desc = *reinterpret_cast<const MstatusDesc*>(&m_csr[kCsrMstatus]);
            desc.sxl         = static_cast<uint64_t>(RiscvMXL::kRv64);
            desc.uxl         = static_cast<uint64_t>(RiscvMXL::kRv64);
            res              = *reinterpret_cast<const uint64_t*>(&desc);
        } break;
        case kCsrTselect:
        case kCsrTdata1:
            res = 0;
            break;
        default:
            res = m_csr[addr];
            break;
    }

    return res;
}

void State::Write(const uint64_t addr, const uint64_t val) {
    switch (addr) {
        case kCsrSstatus: {
            const uint64_t kOriginMstatus = Read(kCsrMstatus);
            m_csr[kCsrMstatus]            = (kOriginMstatus & (~kCsrMstatusMaskSStatus)) | (val & kCsrMstatusMaskSStatus);
        } break;
        case kCsrSie:
            m_csr[kCsrMie] &= (~m_csr[kCsrMideleg]);
            m_csr[kCsrMie] |= (val & m_csr[kCsrMideleg]);
            break;
        case kCsrSip:
            m_csr[kCsrMip] &= (~m_csr[kCsrMideleg]);
            m_csr[kCsrMip] |= (val & m_csr[kCsrMideleg]);
            break;
        case kCsrMstatus: {
            MstatusDesc desc   = *reinterpret_cast<const MstatusDesc*>(&val);
            desc.sxl           = static_cast<uint64_t>(RiscvMXL::kRv64);
            desc.uxl           = static_cast<uint64_t>(RiscvMXL::kRv64);
            m_csr[kCsrMstatus] = *reinterpret_cast<const uint64_t*>(&desc);
        } break;
        case kCsrMisa:
        case kCsrTselect:
        case kCsrTdata1:
        case kCsrMVendorId:
        case kCsrMArchId:
        case kCsrMImpId:
            break;
        default:
            m_csr[addr] = val;
            break;
    }
}

void State::Reset() {
    const uint64_t kOriginMisa = m_csr[kCsrMisa];
    std::fill(m_csr.begin(), m_csr.end(), 0);

    m_csr[kCsrMisa] = kOriginMisa;

    // set up Mstatus val
    auto* mstatus_desc = reinterpret_cast<MstatusDesc*>(&m_csr[kCsrMstatus]);
    mstatus_desc->sxl  = static_cast<uint64_t>(RiscvMXL::kRv64);
    mstatus_desc->uxl  = static_cast<uint64_t>(RiscvMXL::kRv64);
}

} // namespace rv64_emulator::cpu::csr
