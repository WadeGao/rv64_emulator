#include "device/bus.h"

#include <algorithm>

#include "device/dram.h"
#include "device/mmio.hpp"

namespace rv64_emulator::device::bus {

void Bus::MountDevice(MmioDeviceNode node) {
  device_.emplace_back(std::move(node));
}

std::list<MmioDeviceNode>::const_iterator Bus::GetDeviceByRangeImpl(
    uint64_t addr) const {
  for (auto iter = device_.cbegin(); iter != device_.cend(); iter++) {
    if (iter->base <= addr && addr < iter->base + iter->size) {
      return iter;
    }
  }
  return device_.cend();
}

std::list<MmioDeviceNode>::iterator Bus::GetDeviceByRange(uint64_t addr) {
  auto citer = GetDeviceByRangeImpl(addr);
  auto res = device_.begin();
  std::advance(res, std::distance<decltype(citer)>(res, citer));
  return res;
}

std::list<MmioDeviceNode>::const_iterator Bus::GetDeviceByRange(
    const uint64_t addr) const {
  return GetDeviceByRangeImpl(addr);
}

bool Bus::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  const auto kDevNode = GetDeviceByRange(addr);
  if (kDevNode == device_.end()) {
    return false;
  }
  return kDevNode->dev->Load(addr - kDevNode->base, bytes, buffer);
}

bool Bus::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  auto dev_node = GetDeviceByRange(addr);
  if (dev_node == device_.end()) {
    return false;
  }
  return dev_node->dev->Store(addr - dev_node->base, bytes, buffer);
}

void Bus::Reset() {
  for (auto& iter : device_) {
    iter.dev->Reset();
  }
}

}  // namespace rv64_emulator::device::bus
