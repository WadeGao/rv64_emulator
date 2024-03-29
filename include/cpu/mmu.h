#pragma once

#include <cstdint>
#include <memory>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "device/bus.h"

namespace rv64_emulator {

using cpu::CPU;
using cpu::csr::SatpDesc;
using cpu::trap::Trap;
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
  uint64_t ppn;
  uint64_t tag;
  uint64_t asid : 16;
  uint64_t R : 1;  // read
  uint64_t W : 1;  // write
  uint64_t X : 1;  // execute
  uint64_t U : 1;  // user
  uint64_t G : 1;  // global
  uint64_t A : 1;  // access
  uint64_t D : 1;  // dirty

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

constexpr uint64_t kTlbSize = 2;

class Sv39 {
 public:
  explicit Sv39(std::shared_ptr<Bus>& bus);
  Sv39TlbEntry* GetTlbEntry(SatpDesc satp, uint64_t vaddr);
  bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer);
  bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer);
  void FlushTlb(uint64_t vaddr, uint64_t asid);
  void Reset();
  void PrintStatistic();
  ~Sv39();

 private:
  uint64_t index_;
  uint64_t cache_hit_;
  uint64_t cache_miss_;

  Sv39TlbEntry tlb_[kTlbSize];
  std::shared_ptr<Bus> bus_;

  Sv39TlbEntry* LookUpTlb(SatpDesc satp, uint64_t vaddr);

  bool PageTableWalk(SatpDesc satp, uint64_t vaddr, Sv39PageTableEntry* pte,
                     uint64_t* page_size);
};

class Mmu {
 public:
  explicit Mmu(std::unique_ptr<Sv39> sv39);
  void SetProcessor(CPU* cpu);
  void FlushTlb(uint64_t vaddr, uint64_t asid);
  Trap Fetch(uint64_t addr, uint64_t bytes, uint8_t* buffer);
  Trap Load(uint64_t addr, uint64_t bytes, uint8_t* buffer);
  Trap Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer);
  void Reset();

 private:
  CPU* cpu_;
  std::unique_ptr<Sv39> sv39_;

  bool UsePhysAddr(SatpDesc satp, cpu::csr::MstatusDesc ms);
};

}  // namespace mmu
}  // namespace rv64_emulator
