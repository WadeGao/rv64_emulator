#include "device/bus.h"

#include <algorithm>

#include "device/dram.h"
#include "device/mmio.hpp"

namespace rv64_emulator::device::bus {

Bus::Bus() : dev_cnt_(0) {}

void Bus::MountDevice(MmioDeviceNode node) {
  device_[dev_cnt_++] = std::move(node);
}

int64_t Bus::GetDeviceByRange(uint64_t addr) {
  for (uint64_t i = 0; i < dev_cnt_; i++) {
    auto* iter = device_ + i;
    if (iter->base <= addr && addr < iter->base + iter->size) {
      return i;
    }
  }
  return -1;
}

bool Bus::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  const auto kDevIndex = GetDeviceByRange(addr);
  if (kDevIndex == -1) {
    return false;
  }
  const auto kDevNode = device_ + kDevIndex;
  return kDevNode->dev->Load(addr - kDevNode->base, bytes, buffer);
}

bool Bus::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  const auto kDevIndex = GetDeviceByRange(addr);
  if (kDevIndex == -1) {
    return false;
  }
  auto dev_node = device_ + kDevIndex;
  return dev_node->dev->Store(addr - dev_node->base, bytes, buffer);
}

void Bus::Reset() {
  for (uint64_t i = 0; i < dev_cnt_; i++) {
    device_[i].dev->Reset();
  }
}

}  // namespace rv64_emulator::device::bus
