#pragma once

#include <memory>

#include "dram.h"
#include "mmio_device.hpp"

namespace rv64_emulator {

using dram::DRAM;

namespace bus {

class Bus : public MmioDevice {
 private:
  std::unique_ptr<DRAM> dram_;

 public:
  explicit Bus(std::unique_ptr<DRAM> dram);
  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) const override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  void Reset() override;
};

}  // namespace bus
}  // namespace rv64_emulator
