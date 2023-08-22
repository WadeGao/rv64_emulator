#pragma once

#include <forward_list>
#include <memory>

#include "device/mmio.hpp"

namespace rv64_emulator::device::bus {

class Bus : public MmioDevice {
 public:
  Bus() = default;
  void MountDevice(MmioDeviceNode node);

  bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) override;
  bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) override;
  void Reset() override;

 private:
  std::forward_list<MmioDeviceNode> device_;

  std::forward_list<MmioDeviceNode>::const_iterator GetDeviceByRangeImpl(
      uint64_t addr) const;
  std::forward_list<MmioDeviceNode>::iterator GetDeviceByRange(uint64_t addr);
  std::forward_list<MmioDeviceNode>::const_iterator GetDeviceByRange(
      uint64_t addr) const;
};

}  // namespace rv64_emulator::device::bus
