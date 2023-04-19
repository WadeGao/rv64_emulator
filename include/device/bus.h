#pragma once

#include <list>
#include <memory>

#include "device/mmio.hpp"

namespace rv64_emulator::device {

namespace bus {

class Bus : public MmioDevice {
 private:
  std::list<MmioDeviceNode> device_;

  std::list<MmioDeviceNode>::const_iterator GetDeviceByRangeImpl(
      const uint64_t addr) const;
  std::list<MmioDeviceNode>::iterator GetDeviceByRange(const uint64_t addr);
  std::list<MmioDeviceNode>::const_iterator GetDeviceByRange(
      const uint64_t addr) const;

 public:
  Bus() = default;
  void MountDevice(MmioDeviceNode node);

  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  void Reset() override;
};

}  // namespace bus
}  // namespace rv64_emulator::device
