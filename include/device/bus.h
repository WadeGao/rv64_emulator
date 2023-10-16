#pragma once

#include <memory>

#include "device/mmio.hpp"

namespace rv64_emulator::device::bus {

constexpr uint64_t kMaxMmioDevicesNum = 16;

class Bus : public MmioDevice {
 public:
  Bus();
  void MountDevice(MmioDeviceNode node);

  bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) override;
  bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) override;
  void Reset() override;

 private:
  uint64_t dev_cnt_;
  MmioDeviceNode device_[kMaxMmioDevicesNum];

  int64_t GetDeviceByRange(uint64_t addr);
};

}  // namespace rv64_emulator::device::bus
