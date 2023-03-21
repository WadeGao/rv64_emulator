#include "mmu.h"
#include "bus.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/instruction.h"
#include "cpu/trap.h"
#include "error_code.h"

#include "fmt/core.h"
#include "fmt/format.h"

#include <cstdint>
#include <cstring>

namespace rv64_emulator::mmu {

using cpu::PrivilegeMode;

#define CHECK_RANGE_PAGE_ALIGN(addr, bytes, raised_trap_type)                    \
    const bool kPageMisalign = (((addr) >> 12) != (((addr) + (bytes)-1) >> 12)); \
    if (kPageMisalign) {                                                         \
        return {                                                                 \
            .m_trap_type = (raised_trap_type),                                   \
            .m_val       = (addr),                                               \
        };                                                                       \
    }

#define CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(cur_mode, mstatus_desc, tlb_entry, vaddr, step) \
    PrivilegeMode real_mode = cur_mode;                                                       \
    if ((mstatus_desc).mprv && (cur_mode) == PrivilegeMode::kMachine) {                       \
        real_mode = static_cast<PrivilegeMode>((mstatus_desc).mpp);                           \
    }                                                                                         \
    if (real_mode == PrivilegeMode::kUser && !(tlb_entry)->U) {                               \
        return {                                                                              \
            .m_trap_type = cpu::trap::TrapType::k##step##PageFault,                           \
            .m_val       = (vaddr),                                                           \
        };                                                                                    \
    }                                                                                         \
    if (!(mstatus_desc).sum && real_mode == PrivilegeMode::kSupervisor && (tlb_entry)->U) {   \
        return k##step##AccessTrap;                                                           \
    }

static uint64_t MapVirtualAddress(const Sv39TlbEntry* entry, const uint64_t virtual_addr) {
    const uint64_t offset_bits   = entry->page_size == 1 ? 12 : entry->page_size == 2 ? 21 : 30;
    const uint64_t kPhysicalAddr = entry->ppn + virtual_addr % (1 << offset_bits);
    return kPhysicalAddr;
}

static uint64_t GetPpnByPageTableEntry(const Sv39PageTableEntry* entry) {
    const Sv39VirtualAddress kPpnDesc = {
        .page_offset = 0,
        .vpn_0       = entry->ppn_0,
        .vpn_1       = entry->ppn_1,
        .vpn_2       = entry->ppn_2,
        .blank       = 0,
    };

    return *reinterpret_cast<const uint64_t*>(&kPpnDesc);
}

static uint64_t GetTagVirtualAddress(const uint64_t virtual_address, const uint64_t page_size) {
    return virtual_address - virtual_address % page_size;
}

static uint64_t GetTagMask(const uint64_t page_size) {
    const uint64_t kTagMask = UINT64_MAX & (~((uint64_t(1) << (12 + (page_size - 1) * 9)) - 1));
    return kTagMask;
}

Sv39::Sv39(std::unique_ptr<Bus> bus)
    : m_full_associative_index(0)
    , m_bus(std::move(bus)) {
    memset(m_tlb, 0, sizeof(m_tlb));
}

Sv39TlbEntry* Sv39::LookUpTlb(const SatpDesc satp, const Sv39VirtualAddress virtual_address) {
    const uint64_t kSv39VirtualAddress = *reinterpret_cast<const uint64_t*>(&virtual_address);
    return LookUpTlb(satp, kSv39VirtualAddress);
}

Sv39TlbEntry* Sv39::LookUpTlb(const SatpDesc satp, const uint64_t virtual_address) {
    Sv39TlbEntry* res = nullptr;

    for (uint64_t i = 0; i < kTlbSize; i++) {
        Sv39TlbEntry* entry = m_tlb + i;
        if (entry->asid == satp.asid || entry->G) {
            if (entry->page_size > 0) {
                // clear (huge) page offset field bits:
                const uint64_t kTagMask = GetTagMask(entry->page_size);
                const uint64_t kTag     = virtual_address & kTagMask;
                if (kTag == entry->tag) {
                    res = entry;
                    break;
                }
            };
        }
    }

    return res;
}

bool Sv39::PageTableWalk(const SatpDesc satp, const uint64_t virtual_address, Sv39PageTableEntry& pte, uint64_t& page_size) {
    // filter out all mode not sv39
    if (satp.mode != static_cast<uint64_t>(AddressMode::kSv39)) {
        return false;
    }

    Sv39PageTableEntry       sv39_pte;
    uint64_t                 page_table_addr = satp.ppn << 12;
    const Sv39VirtualAddress sv39_va         = *reinterpret_cast<const Sv39VirtualAddress*>(&virtual_address);

    for (int i = 2; i >= 0; i--) {
        const uint64_t page_entry_index = (i == 2) ? sv39_va.vpn_2 : (i == 1) ? sv39_va.vpn_1 : sv39_va.vpn_0;
        const uint64_t physical_address = page_table_addr + page_entry_index * sizeof(Sv39PageTableEntry);

        const bool kSuccess = m_bus->Load(physical_address, sizeof(Sv39PageTableEntry), reinterpret_cast<uint8_t*>(&sv39_pte));

        if (!kSuccess || !sv39_pte.V || (!sv39_pte.R && sv39_pte.W) || sv39_pte.reserved || sv39_pte.pbmt) {
            return false;
        }

        if (!sv39_pte.R && !sv39_pte.W && !sv39_pte.X) {
            // non-leaf
            const Sv39PhysicalAddress sv39_pa = {
                .page_offset = 0,
                .ppn_0       = sv39_pte.ppn_0,
                .ppn_1       = sv39_pte.ppn_1,
                .ppn_2       = sv39_pte.ppn_2,
                .blank       = 0,
            };

            page_table_addr = *reinterpret_cast<const uint64_t*>(&sv39_pa);
        } else {
            // leaf
            const bool kPpnInvalid = (i == 2 && (sv39_pte.ppn_1 || sv39_pte.ppn_0)) || (i == 1 && sv39_pte.ppn_0);
            if (kPpnInvalid) {
                return false;
            }

            pte       = sv39_pte;
            page_size = (1 << 12) << (9 * i);
            return true;
        }
    }

    return false;
}

Sv39TlbEntry* Sv39::GetTlbEntry(const SatpDesc satp, const uint64_t virtual_address) {
    // sv39_va[39:63] = sv39_va[38]
    const bool kSv39VirtualAddressLegal = (0 <= virtual_address && virtual_address <= 0x0000003fffffffff) ||
                                          (0xffffffc000000000 <= virtual_address && virtual_address <= 0xffffffffffffffff);
    if (!kSv39VirtualAddressLegal) {
#ifdef DEBUG
        fmt::print("illegal sv39 virtual address[{}]\n", virtual_address);
#endif
        exit(static_cast<int>(rv64_emulator::errorcode::MmuErrorCode::kIllegalSv39Address));
    }

    Sv39TlbEntry* tlb_entry = LookUpTlb(satp, virtual_address);

    if (tlb_entry) {
        return tlb_entry;
    }

    // cache miss, now walk the page table
    Sv39PageTableEntry pte;
    uint64_t           out_size;

    bool success = PageTableWalk(satp, virtual_address, pte, out_size);

    if (!success) {
        return nullptr;
    }

    const uint64_t kTlbPageSizeField = (out_size == (1 << 12)) ? 1 : (out_size == (1 << 21)) ? 2 : 3;

    m_tlb[m_full_associative_index] = {
        .ppn       = GetPpnByPageTableEntry(&pte),
        .tag       = GetTagVirtualAddress(virtual_address, out_size),
        .R         = pte.R,
        .W         = pte.W,
        .X         = pte.X,
        .U         = pte.U,
        .G         = pte.G,
        .A         = pte.A,
        .D         = pte.D,
        .asid      = satp.asid,
        .page_size = kTlbPageSizeField,
    };

    Sv39TlbEntry* res        = m_tlb + m_full_associative_index;
    m_full_associative_index = (m_full_associative_index + 1) % kTlbSize;

#ifdef DEBUG
    Sv39TlbEntry* double_check = LookUpTlb(satp, virtual_address);
    if (double_check != res) {
        fmt::print(
            "result mismatch after walk the page table and update the tlb, expect[{}], real[{}]\n", fmt::ptr(res), fmt::ptr(double_check));
        exit(static_cast<int>(rv64_emulator::errorcode::MmuErrorCode::kTlbMissMatchAfterPageTableWalk));
    }
#endif

    return res;
}

Sv39TlbEntry* Sv39::GetTlbEntry(const SatpDesc satp, const Sv39VirtualAddress virtual_address) {
    const uint64_t kSv39VirtualAddress = *reinterpret_cast<const uint64_t*>(&virtual_address);
    return GetTlbEntry(satp, kSv39VirtualAddress);
}

bool Sv39::PhysicalAddressLoad(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const {
    return m_bus->Load(addr, bytes, buffer);
}

bool Sv39::PhysicalAddressStore(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) {
    return m_bus->Store(addr, bytes, buffer);
}

void Sv39::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
    for (uint64_t i = 0; i < kTlbSize; i++) {
        Sv39TlbEntry* entry = m_tlb + i;
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

Mmu::Mmu(std::unique_ptr<Sv39> sv39)
    : m_cpu(nullptr)
    , m_sv39(std::move(sv39)) {
#ifdef DEBUG
    fmt::print("mmu init, sv39 unit addr is {}\n", fmt::ptr(m_sv39.get()));
#endif
}

void Mmu::SetProcessor(CPU* cpu) {
    m_cpu = cpu;
}

void Mmu::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
    m_sv39->FlushTlb(vaddr, asid);
}

Trap Mmu::VirtualFetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
    if (bytes == 4 && addr % 4 == 2) {
        const Trap kTrap1 = VirtualFetch(addr, 2, buffer);
        if (kTrap1.m_trap_type != cpu::trap::TrapType::kNone) {
            return kTrap1;
        }

        const Trap kTrap2 = VirtualFetch(addr + 2, 2, buffer + 2);
        if (kTrap2.m_trap_type != cpu::trap::TrapType::kNone) {
            return {
                .m_trap_type = kTrap2.m_trap_type,
                .m_val       = addr,
            };
        }

        return cpu::trap::kNoneTrap;
    }

    const uint64_t kSatpVal  = m_cpu->m_state.Read(cpu::csr::kCsrSatp);
    const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

    const Trap kInstructionAccessTrap = {
        .m_trap_type = cpu::trap::TrapType::kInstructionAccessFault,
        .m_val       = addr,
    };

    const PrivilegeMode kCurMode = m_cpu->GetPrivilegeMode();
    // physical addr load
    if (kCurMode == PrivilegeMode::kMachine || kSatpDesc.mode == 0) {
        const bool kSucc = m_sv39->PhysicalAddressLoad(addr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kInstructionAccessTrap;
    } else {
        CHECK_RANGE_PAGE_ALIGN(addr, bytes, cpu::trap::TrapType::kInstructionAddressMisaligned);

        const Sv39TlbEntry* kTlbEntry = m_sv39->GetTlbEntry(kSatpDesc, addr);
        if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->X) {
            return {
                .m_trap_type = cpu::trap::TrapType::kInstructionPageFault,
                .m_val       = addr,
            };
        }

        if ((kCurMode == PrivilegeMode::kUser && !kTlbEntry->U) || (kCurMode == PrivilegeMode::kSupervisor && kTlbEntry->U)) {
            return {
                .m_trap_type = cpu::trap::TrapType::kInstructionPageFault,
                .m_val       = addr,
            };
        }

        const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

        const bool kSucc = m_sv39->PhysicalAddressLoad(kPhysicalAddr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kInstructionAccessTrap;
    }

    return kInstructionAccessTrap;
}

Trap Mmu::VirtualAddressLoad(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
    const uint64_t kSatpVal  = m_cpu->m_state.Read(cpu::csr::kCsrSatp);
    const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

    const uint64_t              kMstatus     = m_cpu->m_state.Read(cpu::csr::kCsrMstatus);
    const cpu::csr::MstatusDesc kMstatusDesc = *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

    const Trap kLoadAccessTrap = {
        .m_trap_type = cpu::trap::TrapType::kLoadAccessFault,
        .m_val       = addr,
    };

    const PrivilegeMode kCurMode = m_cpu->GetPrivilegeMode();
    // physical addr load
    if (kSatpDesc.mode == 0 || ((kCurMode == PrivilegeMode::kMachine) &&
                                (!kMstatusDesc.mprv || kMstatusDesc.mpp == static_cast<uint64_t>(PrivilegeMode::kMachine)))) {
        const bool kSucc = m_sv39->PhysicalAddressLoad(addr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kLoadAccessTrap;
    } else {
        CHECK_RANGE_PAGE_ALIGN(addr, bytes, cpu::trap::TrapType::kLoadAddressMisaligned);

        const Sv39TlbEntry* kTlbEntry = m_sv39->GetTlbEntry(kSatpDesc, addr);
        if (!kTlbEntry || !kTlbEntry->A || (!kTlbEntry->R && !(kMstatusDesc.mxr && kTlbEntry->X))) {
            return {
                .m_trap_type = cpu::trap::TrapType::kLoadPageFault,
                .m_val       = addr,
            };
        }

        CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(kCurMode, kMstatusDesc, kTlbEntry, addr, Load);

        const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

        const bool kSucc = m_sv39->PhysicalAddressLoad(kPhysicalAddr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kLoadAccessTrap;
    }

    return kLoadAccessTrap;
}

Trap Mmu::VirtualAddressStore(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) {
    const uint64_t kSatpVal  = m_cpu->m_state.Read(cpu::csr::kCsrSatp);
    const SatpDesc kSatpDesc = *reinterpret_cast<const SatpDesc*>(&kSatpVal);

    const uint64_t              kMstatus     = m_cpu->m_state.Read(cpu::csr::kCsrMstatus);
    const cpu::csr::MstatusDesc kMstatusDesc = *reinterpret_cast<const cpu::csr::MstatusDesc*>(&kMstatus);

    const Trap kStoreAccessTrap = {
        .m_trap_type = cpu::trap::TrapType::kStoreAccessFault,
        .m_val       = addr,
    };

    const PrivilegeMode kCurMode = m_cpu->GetPrivilegeMode();
    // physical addr store
    if (kSatpDesc.mode == 0 || (kCurMode == PrivilegeMode::kMachine &&
                                (!kMstatusDesc.mprv || kMstatusDesc.mpp == static_cast<uint64_t>(PrivilegeMode::kMachine)))) {
        const bool kSucc = m_sv39->PhysicalAddressStore(addr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kStoreAccessTrap;
    } else {
        CHECK_RANGE_PAGE_ALIGN(addr, bytes, cpu::trap::TrapType::kStoreAddressMisaligned);

        const Sv39TlbEntry* kTlbEntry = m_sv39->GetTlbEntry(kSatpDesc, addr);
        // D 位在 C906 的硬件实现与 W 属性类似。当 D 位为 0 时，store 会触发 Page Fault
        if (!kTlbEntry || !kTlbEntry->A || !kTlbEntry->W || !kTlbEntry->D) {
            return {
                .m_trap_type = cpu::trap::TrapType::kStorePageFault,
                .m_val       = addr,
            };
        }

        CHECK_VIRTUAL_MEMORY_ACCESS_PRIVILEGE(kCurMode, kMstatusDesc, kTlbEntry, addr, Store);

        const uint64_t kPhysicalAddr = MapVirtualAddress(kTlbEntry, addr);

        const bool kSucc = m_sv39->PhysicalAddressStore(kPhysicalAddr, bytes, buffer);
        return kSucc ? cpu::trap::kNoneTrap : kStoreAccessTrap;
    }

    return kStoreAccessTrap;
}

Mmu::~Mmu() {
    m_cpu = nullptr;
#ifdef DEBUG
    fmt::print("destroy a mmu\n");
#endif
}

} // namespace rv64_emulator::mmu
