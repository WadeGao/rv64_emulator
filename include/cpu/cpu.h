#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/executor.h"
#include "cpu/trap.h"

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
 public:
  using value_type = typename std::decay_t<T>;
  using reference = value_type&;
  using const_reference = const value_type&;
  static_assert(std::is_floating_point_v<value_type> ||
                    std::is_integral_v<value_type>,
                "reg group should be number type");

  reference operator[](uint32_t index) { return reg_[index]; }
  const_reference operator[](uint32_t index) const { return reg_[index]; }
  void Reset() { memset(reg_, 0, sizeof(value_type) * N); }

 private:
  value_type reg_[N] = {0};
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
 public:
  uint64_t pc_;
  uint64_t instret_;
  uint32_t hart_id_;
  csr::State state_;
  RegFile reg_file_;
  PrivilegeMode priv_mode_;

  explicit CPU(std::unique_ptr<mmu::Mmu> mmu);
  void Reset();
  trap::Trap Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) const;
  trap::Trap Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer);
  trap::Trap Fetch(uint64_t addr, uint64_t bytes, uint8_t* buffer);
  void Tick(bool meip, bool seip, bool msip, bool mtip, bool update);
  void Tick();
  void FlushTlb(uint64_t vaddr, uint64_t asid);
  uint64_t GetInstret() const;

 private:
  std::unique_ptr<executor::Executor> executor_;
  std::unique_ptr<mmu::Mmu> mmu_;

  trap::Trap TickOperate();
  void HandleTrap(trap::Trap trap, uint64_t epc);
  void HandleInterrupt(uint64_t inst_addr);
};

}  // namespace cpu
}  // namespace rv64_emulator
