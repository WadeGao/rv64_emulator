#include "cpu/csr.h"
#include "libs/software_arithmetic.hpp"

#include <cstdint>

namespace rv64_emulator::cpu::csr {

State::State()
    : m_csr(kCsrCapacity, 0) {
    // set up MISA description
    auto* misa_desc = reinterpret_cast<MisaDesc*>(&m_csr[kCsrMisa]);
    misa_desc->mxl  = static_cast<uint64_t>(RiscvMXL::kRv64);
    misa_desc->U    = 1; // User mode implemented
    misa_desc->S    = 1; // Supervisor mode implemented
    misa_desc->M    = 1; // Integer Multiply/Divide extension implemented
    misa_desc->I    = 1; // RV32I/64I/128I base ISA implemented
    misa_desc->F    = 0; // Single-precision floating-point extension implemented
    misa_desc->D    = 0; // Double-precision floating-point extension implemented
    misa_desc->C    = 0; // Compressed extension implemented
    misa_desc->A    = 0; // Atomic extension implemented

    // set up Mstatus val
    auto* mstatus_desc = reinterpret_cast<MstatusDesc*>(&m_csr[kCsrMstatus]);
    mstatus_desc->sxl  = static_cast<uint64_t>(RiscvMXL::kRv64);
    mstatus_desc->uxl  = static_cast<uint64_t>(RiscvMXL::kRv64);
}

uint64_t State::Read(const uint64_t addr) const {
    uint64_t res = 0;

    switch (addr) {
        case kCsrSstatus: {
            auto*       ss_desc = reinterpret_cast<SstatusDesc*>(&res);
            const auto* ms_desc = reinterpret_cast<const MstatusDesc*>(&m_csr[kCsrMstatus]);

            ss_desc->sie  = ms_desc->sie;
            ss_desc->spie = ms_desc->spie;
            ss_desc->ube  = ms_desc->ube;
            ss_desc->spp  = ms_desc->spp;
            ss_desc->vs   = ms_desc->vs;
            ss_desc->fs   = ms_desc->fs;
            ss_desc->xs   = ms_desc->xs;
            ss_desc->sum  = ms_desc->sum;
            ss_desc->mxr  = ms_desc->mxr;
            ss_desc->uxl  = ms_desc->uxl;
            ss_desc->sd   = ms_desc->sd;
        } break;
        case kCsrSie:
            res = m_csr[kCsrMie] & kCsrSInterruptMask;
            break;
        case kCsrSip:
            res = m_csr[kCsrMip] & kCsrSInterruptMask;
            break;
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
            const auto* val_desc = reinterpret_cast<const SstatusDesc*>(&val);
            auto*       ms_desc  = reinterpret_cast<SstatusDesc*>(&m_csr[kCsrMstatus]);

            ms_desc->sie  = val_desc->sie;
            ms_desc->spie = val_desc->spie;
            ms_desc->spp  = val_desc->spp;
            ms_desc->sum  = val_desc->sum;
            ms_desc->mxr  = val_desc->mxr;
        } break;
        case kCsrMie:
        case kCsrMip:
            m_csr[addr] = val & kCsrMInterruptMask;
            break;
        case kCsrSie:
            m_csr[kCsrMie] &= (~kCsrSInterruptMask);
            m_csr[kCsrMie] |= (val & kCsrSInterruptMask);
            break;
        case kCsrSip:
            m_csr[kCsrMip] &= (~kCsrSInterruptMask);
            m_csr[kCsrMip] |= (val & kCsrSInterruptMask);
            break;
        case kCsrMstatus: {
            const auto* val_desc = reinterpret_cast<const MstatusDesc*>(&val);
            auto*       ms_desc  = reinterpret_cast<MstatusDesc*>(&m_csr[kCsrMstatus]);

            ms_desc->sie  = val_desc->sie;
            ms_desc->mie  = val_desc->mie;
            ms_desc->spie = val_desc->spie;
            ms_desc->mpie = val_desc->mpie;
            ms_desc->spp  = val_desc->spp;
            ms_desc->mpp  = val_desc->mpp;
            ms_desc->mprv = val_desc->mprv;
            ms_desc->sum  = val_desc->sum; // always true
            ms_desc->mxr  = val_desc->mxr; // always true
            ms_desc->tvm  = val_desc->tvm;
            ms_desc->tw   = val_desc->tw;  // not supported but wfi impl as nop
            ms_desc->tsr  = val_desc->tsr;
        } break;
        case kCsrMideleg:
            m_csr[addr] = val & kCsrSInterruptMask;
            break;
        case kCsrMedeleg:
            m_csr[addr] = val & kCsrSExceptionMask;
            break;
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
