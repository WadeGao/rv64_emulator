#include "cpu/csr.h"

#include <cstdint>

#include "libs/arithmetic.h"

namespace rv64_emulator::cpu::csr {

State::State() : wfi_(false), csr_(kCsrCapacity, 0) {
  // set up MISA description
  auto* misa_desc = reinterpret_cast<MisaDesc*>(&csr_[kCsrMisa]);
  misa_desc->mxl = static_cast<uint64_t>(RiscvMXL::kRv64);
  misa_desc->I = 1;  // RV32I/64I/128I base ISA implemented
  misa_desc->M = 1;  // Integer Multiply/Divide extension implemented
  misa_desc->A = 1;  // Atomic extension implemented
  misa_desc->S = 1;  // Supervisor mode implemented
  misa_desc->U = 1;  // User mode implemented

  // set up Mstatus val
  auto* mstatus_desc = reinterpret_cast<MstatusDesc*>(&csr_[kCsrMstatus]);
  mstatus_desc->sxl = static_cast<uint64_t>(RiscvMXL::kRv64);
  mstatus_desc->uxl = static_cast<uint64_t>(RiscvMXL::kRv64);
}

uint64_t State::Read(const uint64_t addr) const {
  uint64_t res = 0;

  switch (addr) {
    case kCsrSstatus: {
      auto* ss_desc = reinterpret_cast<SstatusDesc*>(&res);
      const auto* ms_desc =
          reinterpret_cast<const MstatusDesc*>(&csr_[kCsrMstatus]);

      ss_desc->sie = ms_desc->sie;
      ss_desc->spie = ms_desc->spie;
      ss_desc->ube = ms_desc->ube;
      ss_desc->spp = ms_desc->spp;
      ss_desc->vs = ms_desc->vs;
      ss_desc->fs = ms_desc->fs;
      ss_desc->xs = ms_desc->xs;
      ss_desc->sum = ms_desc->sum;
      ss_desc->mxr = ms_desc->mxr;
      ss_desc->uxl = ms_desc->uxl;
      ss_desc->sd = ms_desc->sd;
    } break;
    case kCsrSie:
      res = csr_[kCsrMie] & kCsrSInterruptMask;
      break;
    case kCsrSip:
      res = csr_[kCsrMip] & kCsrSInterruptMask;
      break;
    case kCsrTselect:
    case kCsrTdata1:
      res = 0;
      break;
    default:
      res = csr_[addr];
      break;
  }

  return res;
}

void State::Write(const uint64_t addr, const uint64_t val) {
  switch (addr) {
    case kCsrSstatus: {
      const auto* val_desc = reinterpret_cast<const SstatusDesc*>(&val);
      auto* ss_desc = reinterpret_cast<SstatusDesc*>(&csr_[kCsrMstatus]);

      ss_desc->sie = val_desc->sie;
      ss_desc->spie = val_desc->spie;
      ss_desc->spp = val_desc->spp;
      ss_desc->sum = val_desc->sum;
      ss_desc->mxr = val_desc->mxr;
    } break;
    case kCsrMie:
    case kCsrMip:
      csr_[addr] = val & kCsrMInterruptMask;
      break;
    case kCsrSie:
      csr_[kCsrMie] &= (~kCsrSInterruptMask);
      csr_[kCsrMie] |= (val & kCsrSInterruptMask);
      break;
    case kCsrSip:
      csr_[kCsrMip] &= (~kCsrSInterruptMask);
      csr_[kCsrMip] |= (val & kCsrSInterruptMask);
      break;
    case kCsrMstatus: {
      const auto* val_desc = reinterpret_cast<const MstatusDesc*>(&val);
      auto* ms_desc = reinterpret_cast<MstatusDesc*>(&csr_[kCsrMstatus]);

      ms_desc->sie = val_desc->sie;
      ms_desc->mie = val_desc->mie;
      ms_desc->spie = val_desc->spie;
      ms_desc->mpie = val_desc->mpie;
      ms_desc->spp = val_desc->spp;
      ms_desc->mpp = val_desc->mpp;
      ms_desc->mprv = val_desc->mprv;
      ms_desc->sum = val_desc->sum;  // always true
      ms_desc->mxr = val_desc->mxr;  // always true
      ms_desc->tvm = val_desc->tvm;
      ms_desc->tw = val_desc->tw;  // not supported but wfi impl as nop
      ms_desc->tsr = val_desc->tsr;
    } break;
    case kCsrMideleg:
      csr_[addr] = val & kCsrSInterruptMask;
      break;
    case kCsrMedeleg:
      csr_[addr] = val & kCsrSExceptionMask;
      break;
    case kCsrMisa:
    case kCsrTselect:
    case kCsrTdata1:
    case kCsrMVendorId:
    case kCsrMArchId:
    case kCsrMImpId:
      break;
    case kCsrSatp: {
      auto new_satp = *reinterpret_cast<const SatpDesc*>(&val);
      const auto kOldSatp = *reinterpret_cast<const SatpDesc*>(&csr_[addr]);
      if (new_satp.mode != 0 && new_satp.mode != 8) {
        new_satp.mode = kOldSatp.mode;
      }
      csr_[addr] = *reinterpret_cast<uint64_t*>(&new_satp);
    } break;
    default:
      csr_[addr] = val;
      break;
  }
}

bool State::GetWfi() const { return wfi_; }

void State::SetWfi(const bool wfi) { wfi_ = wfi; }

void State::Reset() {
  wfi_ = false;

  const uint64_t kOriginMisa = csr_[kCsrMisa];
  std::fill(csr_.begin(), csr_.end(), 0);

  csr_[kCsrMisa] = kOriginMisa;

  // set up Mstatus val
  auto* mstatus_desc = reinterpret_cast<MstatusDesc*>(&csr_[kCsrMstatus]);
  mstatus_desc->sxl = static_cast<uint64_t>(RiscvMXL::kRv64);
  mstatus_desc->uxl = static_cast<uint64_t>(RiscvMXL::kRv64);
}

}  // namespace rv64_emulator::cpu::csr
