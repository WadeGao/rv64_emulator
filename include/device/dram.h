#pragma once

#include <cstdint>
#include <vector>

#include "device/mmio.hpp"

namespace rv64_emulator::device {

namespace dram {

class DRAM : public MmioDevice {
 private:
  const uint64_t size_;
  std::vector<uint8_t> memory_;

 public:
  explicit DRAM(const uint64_t mem_size);
  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) const override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  uint64_t GetSize() const;
  void Reset() override;
};

}  // namespace dram
}  // namespace rv64_emulator::device
