#pragma once

#include <cstdint>
#include <memory>

namespace rv64_emulator::device {

class MmioDevice {
 public:
  virtual bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) = 0;
  virtual bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) = 0;
  virtual void Reset() = 0;
  virtual ~MmioDevice() = default;
};

using MmioDeviceNode = struct MmioDeviceNode {
  uint64_t base;
  uint64_t size;
  std::unique_ptr<MmioDevice> dev;
};

}  // namespace rv64_emulator::device
