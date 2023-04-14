#pragma once

#include <cstdint>
#include <memory>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "device/bus.h"
#include "device/mmio.hpp"

namespace rv64_emulator {

using cpu::CPU;
using cpu::csr::SatpDesc;
using cpu::trap::Trap;
using device::MmioDevice;
using device::bus::Bus;

namespace mmu {
enum class AddressMode {
  kBare = 0,
  kSv39 = 8,
  kSv48 = 9,
  kSv57 = 10,
  kSv64 = 11,
};

using Sv39TlbEntry = struct Sv39TlbEntry {
  uint64_t ppn;    // 56 bits
  uint64_t tag;    // 52 bits
  uint64_t R : 1;  // read
  uint64_t W : 1;  // write
  uint64_t X : 1;  // execute
  uint64_t U : 1;  // user
  uint64_t G : 1;  // global
  uint64_t A : 1;  // access
  uint64_t D : 1;  // dirty
  uint64_t asid : 16;

  // 0: invalid
  // 1: 4KB
  // 2: 2MB
  // 3: 1GB
  uint64_t page_size : 2;
};

using Sv39PageTableEntry = struct Sv39PageTableEntry {
  uint64_t V : 1;    // valid
  uint64_t R : 1;    // read
  uint64_t W : 1;    // write
  uint64_t X : 1;    // execute
  uint64_t U : 1;    // user
  uint64_t G : 1;    // global
  uint64_t A : 1;    // access
  uint64_t D : 1;    // dirty
  uint64_t rsw : 2;  // reserved for use by supervisor software
  uint64_t ppn_0 : 9;
  uint64_t ppn_1 : 9;
  uint64_t ppn_2 : 26;
  uint64_t reserved : 7;
  uint64_t pbmt : 2;  // pbmt is not implemented
  uint64_t N : 1;
};

using Sv39VirtualAddress = struct Sv39VirtualAddress {
  uint64_t offset : 12;
  uint64_t vpn_0 : 9;
  uint64_t vpn_1 : 9;
  uint64_t vpn_2 : 9;
  uint64_t blank : 25;
};

using Sv39PhysicalAddress = struct Sv39PhysicalAddress {
  uint64_t offset : 12;
  uint64_t ppn_0 : 9;
  uint64_t ppn_1 : 9;
  uint64_t ppn_2 : 26;
  uint64_t blank : 8;
};

constexpr uint64_t kTlbSize = 32;

class Sv39 : public MmioDevice {
 private:
  uint64_t index_;
  Sv39TlbEntry tlb_[kTlbSize];

  std::unique_ptr<Bus> bus_;

  Sv39TlbEntry* LookUpTlb(const SatpDesc satp, const uint64_t vaddr);

  bool PageTableWalk(const SatpDesc satp, const uint64_t vaddr,
                     Sv39PageTableEntry* pte, uint64_t* page_size);

 public:
  explicit Sv39(std::unique_ptr<Bus> bus);

  Sv39TlbEntry* GetTlbEntry(const SatpDesc satp, const uint64_t vaddr);

  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) const override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;

  void FlushTlb(const uint64_t vaddr, const uint64_t asid);
  void Reset() override;
};

class Mmu {
 private:
  CPU* cpu_;
  std::unique_ptr<Sv39> sv39_;

 public:
  explicit Mmu(std::unique_ptr<Sv39> sv39);
  void SetProcessor(CPU* cpu);
  void FlushTlb(const uint64_t vaddr, const uint64_t asid);
  Trap VirtualFetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer);
  Trap VirtualAddressLoad(const uint64_t addr, const uint64_t bytes,
                          uint8_t* buffer);
  Trap VirtualAddressStore(const uint64_t addr, const uint64_t bytes,
                           const uint8_t* buffer);
  void Reset();
};

}  // namespace mmu
}  // namespace rv64_emulator
