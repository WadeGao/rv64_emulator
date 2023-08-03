#pragma once

#include <cstdint>
#include <vector>

#include "device/mmio.hpp"

namespace rv64_emulator::device::dram {

class DRAM : public MmioDevice {
 public:
  explicit DRAM(uint64_t mem_size);
  DRAM(uint64_t mem_size, const char* in_file);
  bool Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) override;
  bool Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) override;
  uint64_t GetSize() const;
  void Reset() override;

 private:
  uint64_t size_;
  std::vector<uint8_t> memory_;
};

}  // namespace rv64_emulator::device::dram
