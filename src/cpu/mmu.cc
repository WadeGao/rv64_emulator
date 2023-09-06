#include "cpu/mmu.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "device/bus.h"

namespace rv64_emulator::mmu {

using cpu::PrivilegeMode;

#define MAKE_TRAP(etype, value) \
  { .type = (etype), .val = (value), }

#define CHECK_RANGE_PAGE_ALIGN(addr, bytes, etype)                  \
  bool misalign = (((addr) >> 12) != (((addr) + (bytes)-1) >> 12)); \
  if (misalign) {                                                   \
    return MAKE_TRAP(etype, addr);                                  \
  }

#define CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(cur_mode, mstatus_desc,   \
                                              tlb_entry, vaddr, step)   \
  PrivilegeMode real_mode = cur_mode;                                   \
  if ((mstatus_desc).mprv && (cur_mode) == PrivilegeMode::kMachine) {   \
    real_mode = static_cast<PrivilegeMode>((mstatus_desc).mpp);         \
  }                                                                     \
  if (real_mode == PrivilegeMode::kUser && !(tlb_entry)->U) {           \
    return MAKE_TRAP(cpu::trap::TrapType::k##step##PageFault, vaddr);   \
  }                                                                     \
  if (!(mstatus_desc).sum && real_mode == PrivilegeMode::kSupervisor && \
      (tlb_entry)->U) {                                                 \
    return k##step##AccessTrap;                                         \
  }

uint64_t __attribute__((always_inline))
MapVirtualAddress(const Sv39TlbEntry* entry, uint64_t vaddr) {
  uint64_t bits = entry->page_size;
  bits = (bits + 3) + (bits << 3);
  bits = (1ULL << bits);
  return entry->ppn + vaddr % bits;
}

uint64_t __attribute__((always_inline))
GetPpnByPageTableEntry(const Sv39PageTableEntry* entry) {
  const Sv39VirtualAddress kPpnDesc = {
      .offset = 0,
      .vpn_0 = entry->ppn_0,
      .vpn_1 = entry->ppn_1,
      .vpn_2 = entry->ppn_2,
      .blank = 0,
  };

  return *reinterpret_cast<const uint64_t*>(&kPpnDesc);
}

uint64_t __attribute__((always_inline))
GetTlbTag(uint64_t vaddr, uint64_t page_size) {
  uint64_t bits = page_size;
  bits = (bits + 3) + (bits << 3);
  bits = UINT64_MAX << bits;
  return vaddr & bits;
}

Sv39::Sv39(std::shared_ptr<Bus>& bus) : index_(0), bus_(bus) {
  memset(tlb_, 0, sizeof(tlb_));
}

Sv39TlbEntry* Sv39::LookUpTlb(SatpDesc satp, uint64_t vaddr) {
  Sv39TlbEntry* res = nullptr;

  for (uint64_t i = 0; i < kTlbSize; i++) {
    Sv39TlbEntry* entry = tlb_ + i;
    if (entry->asid == satp.asid || entry->G) {
      if (entry->page_size > 0) {
        if (GetTlbTag(vaddr, entry->page_size) == entry->tag) {
          res = entry;
          break;
        }
      }
    }
  }

  return res;
}

bool Sv39::PageTableWalk(SatpDesc satp, uint64_t vaddr, Sv39PageTableEntry* pte,
                         uint64_t* page_size) {
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
      *page_size = i + 1;
      return true;
    }
  }

  return false;
}

Sv39TlbEntry* Sv39::GetTlbEntry(SatpDesc satp, uint64_t vaddr) {
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
  if (!PageTableWalk(satp, vaddr, &pte, &out_size)) {
    return nullptr;
  }

  tlb_[index_] = {
      .ppn = GetPpnByPageTableEntry(&pte),
      .tag = GetTlbTag(vaddr, out_size),
      .asid = satp.asid,
      .R = pte.R,
      .W = pte.W,
      .X = pte.X,
      .U = pte.U,
      .G = pte.G,
      .A = pte.A,
      .D = pte.D,
      .page_size = out_size,
  };

  tlb_entry = tlb_ + index_;
  index_ = (index_ + 1) % kTlbSize;
  return tlb_entry;
}

bool Sv39::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  return bus_->Load(addr, bytes, buffer);
}

bool Sv39::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  return bus_->Store(addr, bytes, buffer);
}

void Sv39::FlushTlb(uint64_t vaddr, uint64_t asid) {
  for (uint64_t i = 0; i < kTlbSize; i++) {
    Sv39TlbEntry* entry = tlb_ + i;
    if (asid == 0 || entry->asid == asid) {
      if (vaddr == 0) {
        entry->page_size = 0;
      } else {
        if (entry->page_size > 0) {
          if (entry->tag == GetTlbTag(vaddr, entry->page_size)) {
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

bool Mmu::UsePhysAddr(SatpDesc satp, cpu::csr::MstatusDesc ms) {
  constexpr auto kMach = cpu::PrivilegeMode::kMachine;
  constexpr auto kMachVal = static_cast<uint64_t>(kMach);
  return (satp.mode == 0 ||
          (cpu_->priv_mode_ == kMach && (!ms.mprv || ms.mpp == kMachVal)));
}

void Mmu::SetProcessor(CPU* cpu) { cpu_ = cpu; }

void Mmu::FlushTlb(uint64_t vaddr, uint64_t asid) {
  sv39_->FlushTlb(vaddr, asid);
}

Trap Mmu::Fetch(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  if (bytes == 4 && addr % 4 == 2) {
    const Trap kTrap1 = Fetch(addr, 2, buffer);
    if (kTrap1.type != cpu::trap::TrapType::kNone) {
      return kTrap1;
    }

    const Trap kTrap2 = Fetch(addr + 2, 2, buffer + 2);
    if (kTrap2.type != cpu::trap::TrapType::kNone) {
      return MAKE_TRAP(kTrap2.type, addr);
    }

    return cpu::trap::kNoneTrap;
  }

  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const Trap kInstructionAccessTrap =
      MAKE_TRAP(cpu::trap::TrapType::kInstructionAccessFault, addr);

  const auto kCurMode = cpu_->priv_mode_;
  const bool kUsePhysAddr =
      (kCurMode == PrivilegeMode::kMachine || kSatpDesc.mode == 0);
  // virtual addr load
  if (!kUsePhysAddr) {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kInstructionAddressMisaligned);

    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->X) {
      return MAKE_TRAP(cpu::trap::TrapType::kInstructionPageFault, addr);
    }

    if ((kCurMode == PrivilegeMode::kUser && !kTlbEntry->U) ||
        (kCurMode == PrivilegeMode::kSupervisor && kTlbEntry->U)) {
      return MAKE_TRAP(cpu::trap::TrapType::kInstructionPageFault, addr);
    }

    addr = MapVirtualAddress(kTlbEntry, addr);
  }

  const bool kSucc = sv39_->Load(addr, bytes, buffer);
  return kSucc ? cpu::trap::kNoneTrap : kInstructionAccessTrap;
}

Trap Mmu::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const uint64_t kMstatus = cpu_->state_.Read(cpu::csr::kCsrMstatus);
  const cpu::csr::MstatusDesc kMstatusDesc =
      *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

  const Trap kLoadAccessTrap =
      MAKE_TRAP(cpu::trap::TrapType::kLoadAccessFault, addr);

  // virtual addr load
  if (!UsePhysAddr(kSatpDesc, kMstatusDesc)) {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kLoadAddressMisaligned);
    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    if (!kTlbEntry || !kTlbEntry->A ||
        (!kTlbEntry->R && !(kMstatusDesc.mxr && kTlbEntry->X))) {
      return MAKE_TRAP(cpu::trap::TrapType::kLoadPageFault, addr);
    }
    CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(cpu_->priv_mode_, kMstatusDesc,
                                          kTlbEntry, addr, Load);
    addr = MapVirtualAddress(kTlbEntry, addr);
  }

  const bool kSucc = sv39_->Load(addr, bytes, buffer);
  return kSucc ? cpu::trap::kNoneTrap : kLoadAccessTrap;
}

Trap Mmu::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  const uint64_t kSatpVal = cpu_->state_.Read(cpu::csr::kCsrSatp);
  const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

  const uint64_t kMstatus = cpu_->state_.Read(cpu::csr::kCsrMstatus);
  const cpu::csr::MstatusDesc kMstatusDesc =
      *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

  const Trap kStoreAccessTrap =
      MAKE_TRAP(cpu::trap::TrapType::kStoreAccessFault, addr);

  // virtual addr store
  if (!UsePhysAddr(kSatpDesc, kMstatusDesc)) {
    CHECK_RANGE_PAGE_ALIGN(addr, bytes,
                           cpu::trap::TrapType::kStoreAddressMisaligned);
    const Sv39TlbEntry* kTlbEntry = sv39_->GetTlbEntry(kSatpDesc, addr);
    // D 位在 C906 的硬件实现与 W 属性类似。
    // 当 D 位为 0 时，store 会触发 Page Fault
    if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->W || !kTlbEntry->D) {
      return MAKE_TRAP(cpu::trap::TrapType::kStorePageFault, addr);
    }
    CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(cpu_->priv_mode_, kMstatusDesc,
                                          kTlbEntry, addr, Store);
    addr = MapVirtualAddress(kTlbEntry, addr);
  }

  const bool kSucc = sv39_->Store(addr, bytes, buffer);
  return kSucc ? cpu::trap::kNoneTrap : kStoreAccessTrap;
}

void Mmu::Reset() { sv39_->Reset(); }

}  // namespace rv64_emulator::mmu
