#pragma once

#include <cstdint>
#include <memory>

namespace rv64_emulator::device {

using MmioRange = struct MmioRange {
  uint64_t base;
  uint64_t size;
};

class MmioDevice {
 public:
  virtual bool Load(const uint64_t addr, const uint64_t bytes,
                    uint8_t* buffer) = 0;
  virtual bool Store(const uint64_t addr, const uint64_t bytes,
                     const uint8_t* buffer) = 0;

  virtual void Reset() = 0;

  virtual ~MmioDevice() = default;
};

struct MmioDeviceNode {
  uint64_t base;
  uint64_t size;
  std::unique_ptr<MmioDevice> dev;
};

}  // namespace rv64_emulator::device
