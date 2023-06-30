#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>

#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/executor.h"
#include "cpu/trap.h"
#include "libs/lru.hpp"

namespace rv64_emulator {

namespace mmu {
class Mmu;
}

namespace cpu {

namespace executor {
class Executor;
};

enum class PrivilegeMode {
  kUser = 0,
  kSupervisor,
  kReserved,
  kMachine,
};

template <typename T, uint32_t N>
class RegGroup {
  static_assert(std::is_floating_point_v<T> || std::is_integral_v<T>,
                "reg group should be number type");

 private:
  T reg_[N] = {0};

 public:
  T& operator[](uint32_t index) { return reg_[index]; }
  const T& operator[](uint32_t index) const { return reg_[index]; }
  void Reset() { memset(reg_, 0, sizeof(T) * N); }
};

class RegFile {
 public:
  RegGroup<uint64_t, 32> xregs;
  // F 拓展说明：https://tclin914.github.io/3d45634e/
  RegGroup<float, 32> fregs;

  void Reset() {
    xregs.Reset();
    fregs.Reset();
  }
};

class CPU {
 private:
  uint64_t clock_;
  uint64_t instret_;
  PrivilegeMode priv_mode_;

  /*
  +───────────+───────────+────────────────────────────────────+────────+
  | Register  | ABI Name  | Description                        | Saver  |
  +───────────+───────────+────────────────────────────────────+────────+
  | x0        | zero      | Hard-wired zero                    |  ----  |
  | x1        | ra        | Return address                     | Caller |
  | x2        | sp        | Stack pointer                      | Callee |
  | x3        | gp        | Global pointer                     |  ----  |
  | x4        | tp        | Thread pointer                     |  ----  |
  | x5        | t0        | Temporary/alternate link register  | Caller |
  | x6 - 7    | t1 - 2    | Temporaries                        | Caller |
  | x8        | s0/fp     | Saved register/frame pointer       | Callee |
  | x9        | s1        | Saved registers                    | Callee |
  | x10 - 11  | a0 - 1    | Function args/return values        | Caller |
  | x12 - 17  | a2 - 7    | Function args                      | Caller |
  | x18 - 27  | s2 - 11   | Saved registers                    | Callee |
  | x28 - 31  | t3 - 6    | Temporaries                        | Caller |
  +───────────+───────────+────────────────────────────────────+────────+
  */

  std::unique_ptr<executor::Executor> executor_;
  std::unique_ptr<mmu::Mmu> mmu_;

  // decode lookaside buffer
  libs::LRUCache<uint32_t, int32_t> dlb_;

  trap::Trap TickOperate();

  void HandleTrap(const trap::Trap trap, const uint64_t inst_addr);
  void HandleInterrupt(const uint64_t inst_addr);

 public:
  uint64_t pc_;
  csr::State state_;
  RegFile reg_file_;

  explicit CPU(std::unique_ptr<mmu::Mmu> mmu);
  void Reset();

  trap::Trap Load(const uint64_t addr, const uint64_t bytes,
                  uint8_t* buffer) const;
  trap::Trap Store(const uint64_t addr, const uint64_t bytes,
                   const uint8_t* buffer);

  trap::Trap Fetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer);
  trap::Trap Decode(decode::DecodeResDesc* res);

  void Tick(bool meip, bool seip, bool msip, bool mtip, bool update);
  void Tick();

  PrivilegeMode GetPrivilegeMode() const { return priv_mode_; }
  void SetPrivilegeMode(const PrivilegeMode mode) { priv_mode_ = mode; }

  void FlushTlb(const uint64_t vaddr, const uint64_t asid);

  void Disassemble(const uint64_t pc, const uint32_t word,
                   const int64_t index) const;
  void DumpRegs() const;
};

}  // namespace cpu
}  // namespace rv64_emulator
