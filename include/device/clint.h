#pragma once
#include <cstdint>
#include <vector>

#include "device/mmio.hpp"

namespace rv64_emulator::device::clint {

constexpr uint64_t kMsipBase = 0;
constexpr uint64_t kMtimeCmpBase = 0x4000;
constexpr uint64_t kMtimeBase = 0xbff8;

class Clint : public MmioDevice {
 public:
  explicit Clint(uint64_t harts);

  bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) override;
  bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) override;

  void Reset() override;

  bool MachineTimerIrq(uint64_t hart_id);
  bool MachineSoftwareIrq(uint64_t hart_id);

  void Tick();
  void UpdateMtime();

 private:
  uint64_t harts_;
  uint64_t mtime_;
  std::vector<uint64_t> mtimecmp_;
  std::vector<uint32_t> msip_;
};

}  // namespace rv64_emulator::device::clint
