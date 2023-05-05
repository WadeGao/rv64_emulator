#pragma once
#include <cstdint>
#include <vector>

#include "device/mmio.hpp"

namespace rv64_emulator {

namespace device::clint {

constexpr uint64_t kMsipBase = 0;
constexpr uint64_t kMtimeCmpBase = 0x4000;
constexpr uint64_t kMtimeBase = 0xbff8;

class Clint : public MmioDevice {
 private:
  uint64_t harts_;
  uint64_t mtime_;
  std::vector<uint64_t> mtimecmp_;
  std::vector<uint32_t> msip_;

 public:
  explicit Clint(const uint64_t harts);

  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;

  void Reset() override;

  bool MachineTimerIrq(const uint64_t hart_id);
  bool MachineSoftwareIrq(const uint64_t hart_id);

  void Tick();
};

}  // namespace device::clint

}  // namespace rv64_emulator
