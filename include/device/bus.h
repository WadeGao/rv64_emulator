#pragma once

#include <memory>
#include <vector>

#include "device/mmio.hpp"

namespace rv64_emulator::device {

namespace bus {

class Bus : public MmioDevice {
 private:
  std::vector<MmioDeviceNode> device_;

  std::vector<MmioDeviceNode>::const_iterator GetDeviceByRangeImpl(
      const uint64_t addr) const;
  std::vector<MmioDeviceNode>::iterator GetDeviceByRange(const uint64_t addr);
  std::vector<MmioDeviceNode>::const_iterator GetDeviceByRange(
      const uint64_t addr) const;

 public:
  Bus() = default;
  void MountDevice(MmioDeviceNode node);

  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) const override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  void Reset() override;
};

}  // namespace bus
}  // namespace rv64_emulator::device
