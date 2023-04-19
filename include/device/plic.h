#pragma once

#include <_types/_uint64_t.h>

#include <cstdint>
#include <vector>

#include "conf.h"
#include "device/mmio.hpp"

namespace rv64_emulator {

namespace device::plic {

constexpr uint64_t kSourcePriorityBase = 0;

constexpr uint64_t kPendingBase = 0x1000;

constexpr uint64_t kEnableBase = 0x2000;

constexpr uint64_t kContextBase = 0x200000;
constexpr uint64_t kContextEnd = 0x3ffffff;

constexpr uint64_t kEnableBytesPerHart = 0x80;
constexpr uint64_t kContextBytesPerHart = 0x1000;

constexpr uint64_t kWordBits = sizeof(uint32_t) * 8;

constexpr uint64_t kLenGroupByWord =
    (kPlicMaxDevices + kWordBits - 1) / kWordBits;

using PlicContext = struct PlicContext {
  bool mmode = false;
  uint32_t id = 0;
  uint32_t claim = 0;
  uint32_t threshold = 0;
  uint32_t enable[kLenGroupByWord] = {0};
};

class Plic : public MmioDevice {
 private:
  std::vector<PlicContext> contexts_;
  uint64_t dev_num_;
  uint32_t priority_[kPlicMaxDevices] = {0};
  uint32_t pending_[kLenGroupByWord] = {0};
  uint32_t claimed_[kLenGroupByWord] = {0};

 public:
  Plic(const uint64_t cores, bool is_s_mode, uint64_t device_num);
  void UpdateExt(const uint32_t src_id, bool fired);
  bool GetInterrupt(const uint64_t ctx_id);
  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  void Reset() override;
};

}  // namespace device::plic
}  // namespace rv64_emulator
