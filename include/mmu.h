#pragma once

#include "cpu/trap.h"

#include "cpu/cpu.h"
#include <cstdint>
#include <memory>

namespace rv64_emulator {

namespace bus {
class Bus;
}

namespace cpu {

namespace csr {
struct SatpDesc;
}

namespace trap {
struct Trap;
}

class CPU;
} // namespace cpu

namespace cpu::trap {
struct Trap;
}

using bus::Bus;
using cpu::CPU;
using cpu::csr::SatpDesc;
using cpu::trap::Trap;

namespace mmu {

enum class AddressMode {
    kBare = 0,
    kSv39 = 8,
    kSv48 = 9,
    kSv57 = 10,
    kSv64 = 11,
};

using Sv39TlbEntry = struct Sv39TlbEntry {
    uint64_t ppn; // 56 bits
    uint64_t tag; // 52 bits

    uint64_t R         : 1; // read
    uint64_t W         : 1; // write
    uint64_t X         : 1; // execute
    uint64_t U         : 1; // user
    uint64_t G         : 1; // global
    uint64_t A         : 1; // access
    uint64_t D         : 1; // dirty

    uint64_t asid      : 16;

    // 0: invalid
    // 1: 4KB
    // 2: 2MB
    // 3: 1GB
    uint64_t page_size : 2;
};

using Sv39PageTableEntry = struct Sv39PageTableEntry {
    uint64_t V        : 1; // valid
    uint64_t R        : 1; // read
    uint64_t W        : 1; // write
    uint64_t X        : 1; // execute
    uint64_t U        : 1; // user
    uint64_t G        : 1; // global
    uint64_t A        : 1; // access
    uint64_t D        : 1; // dirty
    uint64_t rsw      : 2; // reserved for use by supervisor software
    uint64_t ppn_0    : 9;
    uint64_t ppn_1    : 9;
    uint64_t ppn_2    : 26;
    uint64_t reserved : 7;
    uint64_t pbmt     : 2; // pbmt is not implemented
    uint64_t N        : 1;
};

using Sv39VirtualAddress = struct Sv39VirtualAddress {
    uint64_t page_offset : 12;
    uint64_t vpn_0       : 9;
    uint64_t vpn_1       : 9;
    uint64_t vpn_2       : 9;
    uint64_t blank       : 25;
};

using Sv39PhysicalAddress = struct Sv39PhysicalAddress {
    uint64_t page_offset : 12;
    uint64_t ppn_0       : 9;
    uint64_t ppn_1       : 9;
    uint64_t ppn_2       : 26;
    uint64_t blank       : 8;
};

constexpr uint64_t kTlbSize = 32;

class Sv39 {
private:
    uint64_t     m_full_associative_index;
    Sv39TlbEntry m_tlb[kTlbSize];

    std::unique_ptr<Bus> m_bus;

    Sv39TlbEntry* LookUpTlb(const SatpDesc satp, const uint64_t virtual_address);
    Sv39TlbEntry* LookUpTlb(const SatpDesc satp, const Sv39VirtualAddress virtual_address);

    bool PageTableWalk(const SatpDesc satp, const uint64_t virtual_address, Sv39PageTableEntry& pte, uint64_t& page_size);

public:
    Sv39(std::unique_ptr<Bus> bus);

    Sv39TlbEntry* GetTlbEntry(const SatpDesc satp, const uint64_t virtual_address);
    Sv39TlbEntry* GetTlbEntry(const SatpDesc satp, const Sv39VirtualAddress virtual_address);

    bool PhysicalAddressLoad(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const;
    bool PhysicalAddressStore(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer);
};

class Mmu {
private:
    CPU*                  m_cpu;
    std::unique_ptr<Sv39> m_sv39;

public:
    Mmu(std::unique_ptr<Sv39> sv39);
    void SetProcessor(CPU* cpu);
    Trap VirtualFetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer);
    Trap VirtualAddressLoad(const uint64_t addr, const uint64_t bytes, uint8_t* buffer);
    Trap VirtualAddressStore(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer);
    ~Mmu();
};

} // namespace mmu
} // namespace rv64_emulator
