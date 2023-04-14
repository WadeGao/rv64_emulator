#include "mmu.h"

#include <cstdint>
#include <cstring>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "device/bus.h"

namespace rv64_emulator::mmu {

using cpu::PrivilegeMode;

#define CHECK_RANGE_PAGE_ALIGN(addr, bytes, raised_trap_type)                  \
  const bool kPageMisalign = (((addr) >> 12) != (((addr) + (bytes)-1) >> 12)); \
  if (kPageMisalign) {                                                         \
    return {                                                                   \
        .type = (raised_trap_type),                                            \
        .val = (addr),                                                         \
    };                                                                         \
  }

#define CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(cur_mode, mstatus_desc,   \
                                              tlb_entry, vaddr, step)   \
  PrivilegeMode real_mode = cur_mode;                                   \
  if ((mstatus_desc).mprv && (cur_mode) == PrivilegeMode::kMachine) {   \
    real_mode = static_cast<PrivilegeMode>((mstatus_desc).mpp);         \
  }                                                                     \
  if (real_mode == PrivilegeMode::kUser && !(tlb_entry)->U) {           \
    return {                                                            \
        .type = cpu::trap::TrapType::k##step##PageFault,                \
        .val = (vaddr),                                                 \
    };                                                                  \
  }                                                                     \
  if (!(mstatus_desc).sum && real_mode == PrivilegeMode::kSupervisor && \
      (tlb_entry)->U) {                                                 \
    return k##step##AccessTrap;                                         \
  }

static uint64_t MapVirtualAddress(const Sv39TlbEntry* entry,
                                  const uint64_t vaddr) {
  const uint64_t kOffsetBits = entry->page_size == 1   ? 12
                               : entry->page_size == 2 ? 21
                                                       : 30;
  const uint64_t kPhysicalAddr = entry->ppn + vaddr % (1 << kOffsetBits);
  return kPhysicalAddr;
}

static uint64_t GetPpnByPageTableEntry(const Sv39PageTableEntry* entry) {
  const Sv39VirtualAddress kPpnDesc = {
      .offset = 0,
      .vpn_0 = entry->ppn_0,
      .vpn_1 = entry->ppn_1,
      .vpn_2 = entry->ppn_2,
      .blank = 0,
  };

  return *reinterpret_cast<const uint64_t*>(&kPpnDesc);
}

static uint64_t GetTagVirtualAddress(const uint64_t vaddr,
                                     const uint64_t page_size) {
  return vaddr - vaddr % page_size;
}

static uint64_t GetTagMask(const uint64_t page_size) {
  // const uint64_t kTagMask =
  //     UINT64_MAX & (~((uint64_t(1) << (12 + (page_size - 1) * 9)) - 1));
  const uint64_t kTagMask = 0xfffffffffffffff8 << (page_size * 9);
  return kTagMask;
}

Sv39::Sv39(std::unique_ptr<Bus> bus) : index_(0), bus_(std::move(bus)) {
  memset(tlb_, 0, sizeof(tlb_));
}

Sv39TlbEntry* Sv39::LookUpTlb(const SatpDesc satp, const uint64_t vaddr) {
  Sv39TlbEntry* res = nullptr;

  for (uint64_t i = 0; i < kTlbSize; i++) {
    Sv39TlbEntry* entry = tlb_ + i;
    if (entry->asid == satp.asid || entry->G) {
      if (entry->page_size > 0) {
        // clear (huge) page offset field bits:
        const uint64_t kTagMask = GetTagMask(entry->page_size);
        const uint64_t kTag = vaddr & kTagMask;
        if (kTag == entry->tag) {
          res = entry;
          break;
        }
      }
    }
  }

  return res;
}

bool Sv39::PageTableWalk(const SatpDesc satp, const uint64_t vaddr,
                         Sv39PageTableEntry* pte, uint64_t* page_size) {
  // filter out all mode not sv39
  if (satp.mode != static_cast<uint64_t>(AddressMode::kSv39)) {
    return false;
  }

  Sv39PageTableEntry sv39_pte;
  uint64_t page_table_addr = satp.ppn << 12;
  const Sv39VirtualAddress kSv39Va =
      *reinterpret_cast<const Sv39VirtualAddress*>(&vaddr);

  for (int i = 2; i >= 0; i--) {
    const uint64_t kPageEntryIndex = (i == 2)   ? kSv39Va.vpn_2
                                     : (i == 1) ? kSv39Va.vpn_1
                                                : kSv39Va.vpn_0;
    const uint64_t kPhysAddr =
        page_table_addr + kPageEntryIndex * sizeof(Sv39PageTableEntry);

    const bool kSuccess = bus_->Load(kPhysAddr, sizeof(Sv39PageTableEntry),
                                     reinterpret_cast<uint8_t*>(&sv39_pte));

    if (!kSuccess || !sv39_pte.V || (!sv39_pte.R && sv39_pte.W) ||
        sv39_pte.reserved || sv39_pte.pbmt) {
      return false;
    }

    if (!sv39_pte.R && !sv39_pte.W && !sv39_pte.X) {
      // non-leaf
      const Sv39PhysicalAddress kSv39Pa = {
          .offset = 0,
          .ppn_0 = sv39_pte.ppn_0,
          .ppn_1 = sv39_pte.ppn_1,
          .ppn_2 = sv39_pte.ppn_2,
          .blank = 0,
      };

      page_table_addr = *reinterpret_cast<const uint64_t*>(&kSv39Pa);
    } else {
      // leaf
      const bool kPpnInvalid = (i == 2 && (sv39_pte.ppn_1 || sv39_pte.ppn_0)) ||
                               (i == 1 && sv39_pte.ppn_0);
      if (kPpnInvalid) {
        return false;
      }

      *pte = sv39_pte;
      *page_size = (1 << 12) << (9 * i);
      return true;
    }
  }

  return false;
}

Sv39TlbEntry* Sv39::GetTlbEntry(const SatpDesc satp, const uint64_t vaddr) {
  // sv39_va[39:63] = sv39_va[38]
  const bool kSv39VirtualAddressLegal =
      (0 <= vaddr && vaddr <= 0x0000003fffffffff) ||
      (0xffffffc000000000 <= vaddr && vaddr <= 0xffffffffffffffff);
  if (!kSv39VirtualAddressLegal) {
    return nullptr;
  }

  Sv39TlbEntry* tlb_entry = LookUpTlb(satp, vaddr);

  if (tlb_entry) {
    return tlb_entry;
  }

  // cache miss, now walk the page table
  Sv39PageTableEntry pte;
  uint64_t out_size;

  bool success = PageTableWalk(satp, vaddr, &pte, &out_size);

  if (!success) {
    return nullptr;
  }

  const uint64_t kTlbPageSizeField = (out_size == (1 << 12))   ? 1
                                     : (out_size == (1 << 21)) ? 2
                                                               : 3;

  tlb_[index_] = {
      .ppn = GetPpnByPageTableEntry(&pte),
      .tag = GetTagVirtualAddress(vaddr, out_size),
      .R = pte.R,
      .W = pte.W,
      .X = pte.X,
      .U = pte.U,
      .G = pte.G,
      .A = pte.A,
      .D = pte.D,
      .asid = satp.asid,
      .page_size = kTlbPageSizeField,
  };

  Sv39TlbEntry* res = tlb_ + index_;
  index_ = (index_ + 1) % kTlbSize;

  return res;
}

bool Sv39::Load(const uint64_t addr, const uint64_t bytes,
                uint8_t* buffer) const {
  return bus_->Load(addr, bytes, buffer);
}

bool Sv39::Store(const uint64_t addr, const uint64_t bytes,
                 const uint8_t* buffer) {
  return bus_->Store(addr, bytes, buffer);
}

void Sv39::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
  for (uint64_t i = 0; i < kTlbSize; i++) {
    Sv39TlbEntry* entry = tlb_ + i;
    if (asid == 0 || entry->asid == asid) {
      if (vaddr == 0) {
        entry->page_size = 0;
      } else {
        if (entry->page_size > 0) {
          const uint64_t kTagMask = GetTagMask(entry->page_size);
          if ((entry->tag & kTagMask) == (vaddr & kTagMask)) {
            entry->page_size = 0;
          }
        }
      }
    }
  }
}

void Sv39::Reset() {
  index_ = 0;
  memset(tlb_, 0, sizeof(tlb_));
  bus_->Reset();
}

Mmu::Mmu(std::unique_ptr<Sv39> sv39) : cpu_(nullptr), sv39_(std::move(sv39)) {}

void Mmu::SetProcessor(CPU* cpu) { cpu_ = cpu; }

void Mmu::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
  sv39_->FlushTlb(vaddr, asid);
}

Trap Mmu::VirtualFetch(const uint64_t addr, const uint64_t bytes,
                       uint8_t* buffer) {
  if (bytes == 4 && addr % 4 == 2) {
    const Trap kTrap1 = VirtualFetch(addr, 2, buffer);
    if (kTrap1.type != cpu::trap::TrapType::kNone) {
      return kTrap1;
    }

    const Trap kTrap2 = VirtualFetch(addr + 2, 2, buffer + 2);
    if (kTrap2.type != cpu::trap::TrapType::kNone) {
      return {
          .type = kTrap2.type,
          .val = addr,
      };
    }

    return cpu::trap::kNoneTrap;
  }

  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const Trap kInstructionAccessTrap = {
      .type = cpu::trap::TrapType::kInstructionAccessFault,
      .val = addr,
  };

  const PrivilegeMode kCurMode = cpu_->GetPrivilegeMode();
  // physical addr load
  if (kCurMode == PrivilegeMode::kMachine || kSatpDesc.mode == 0) {
    const bool kSucc = sv39_->Load(addr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kInstructionAccessTrap;
  } else {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kInstructionAddressMisaligned);

    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->X) {
      return {
          .type = cpu::trap::TrapType::kInstructionPageFault,
          .val = addr,
      };
    }

    if ((kCurMode == PrivilegeMode::kUser && !kTlbEntry->U) ||
        (kCurMode == PrivilegeMode::kSupervisor && kTlbEntry->U)) {
      return {
          .type = cpu::trap::TrapType::kInstructionPageFault,
          .val = addr,
      };
    }

    const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

    const bool kSucc = sv39_->Load(kPhysicalAddr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kInstructionAccessTrap;
  }

  return kInstructionAccessTrap;
}

Trap Mmu::VirtualAddressLoad(const uint64_t addr, const uint64_t bytes,
                             uint8_t* buffer) {
  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const uint64_t kMstatus = cpu_->state_.Read(cpu::csr::kCsrMstatus);
  const cpu::csr::MstatusDesc kMstatusDesc =
      *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

  const Trap kLoadAccessTrap = {
      .type = cpu::trap::TrapType::kLoadAccessFault,
      .val = addr,
  };

  const PrivilegeMode kCurMode = cpu_->GetPrivilegeMode();
  // physical addr load
  if (kSatpDesc.mode == 0 ||
      ((kCurMode == PrivilegeMode::kMachine) &&
       (!kMstatusDesc.mprv ||
        kMstatusDesc.mpp == static_cast<uint64_t>(PrivilegeMode::kMachine)))) {
    const bool kSucc = sv39_->Load(addr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kLoadAccessTrap;
  } else {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kLoadAddressMisaligned);

    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    if (!kTlbEntry || !kTlbEntry->A ||
        (!kTlbEntry->R && !(kMstatusDesc.mxr && kTlbEntry->X))) {
      return {
          .type = cpu::trap::TrapType::kLoadPageFault,
          .val = addr,
      };
    }

    CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(kCurMode, kMstatusDesc, kTlbEntry,
                                          addr, Load);

    const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

    const bool kSucc = sv39_->Load(kPhysicalAddr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kLoadAccessTrap;
  }

  return kLoadAccessTrap;
}

Trap Mmu::VirtualAddressStore(const uint64_t addr, const uint64_t bytes,
                              const uint8_t* buffer) {
  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const uint64_t kMstatus = cpu_->state_.Read(cpu::csr::kCsrMstatus);
  const cpu::csr::MstatusDesc kMstatusDesc =
      *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

  const Trap kStoreAccessTrap = {
      .type = cpu::trap::TrapType::kStoreAccessFault,
      .val = addr,
  };

  const PrivilegeMode kCurMode = cpu_->GetPrivilegeMode();
  // physical addr store
  if (kSatpDesc.mode == 0 ||
      (kCurMode == PrivilegeMode::kMachine &&
       (!kMstatusDesc.mprv ||
        kMstatusDesc.mpp == static_cast<uint64_t>(PrivilegeMode::kMachine)))) {
    const bool kSucc = sv39_->Store(addr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kStoreAccessTrap;
  } else {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kStoreAddressMisaligned);

    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    // D 位在 C906 的硬件实现与 W 属性类似。当 D 位为 0 时，store 会触发 Page
    // Fault
    if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->W || !kTlbEntry->D) {
      return {
          .type = cpu::trap::TrapType::kStorePageFault,
          .val = addr,
      };
    }

    CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(kCurMode, kMstatusDesc, kTlbEntry,
                                          addr, Store);

    const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

    const bool kSucc = sv39_->Store(kPhysicalAddr, bytes, buffer);
    return kSucc ? cpu::trap::kNoneTrap : kStoreAccessTrap;
  }

  return kStoreAccessTrap;
}

void Mmu::Reset() { sv39_->Reset(); }

}  // namespace rv64_emulator::mmu
