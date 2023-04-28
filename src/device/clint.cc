#include "device/clint.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rv64_emulator::device::clint {

Clint::Clint(const uint64_t harts)
    : harts_(harts), mtime_(0), mtimecmp_(harts, 0), msip_(harts, 0) {}

bool Clint::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
  if (kMsipBase <= addr && addr + bytes <= kMsipBase + harts_ * 4) {
    // msip
    const uint8_t* kStartAddr =
        reinterpret_cast<const uint8_t*>(msip_.data()) + addr - kMsipBase;
    memcpy(buffer, kStartAddr, bytes);
  } else if (kMtimeCmpBase <= addr &&
             addr + bytes <= kMtimeCmpBase + harts_ * 8) {
    // mtimecmp
    const uint8_t* kStartAddr =
        reinterpret_cast<const uint8_t*>(mtimecmp_.data()) + addr -
        kMtimeCmpBase;
    memcpy(buffer, kStartAddr, bytes);
  } else if (kMtimeBase <= addr && addr + bytes <= kMtimeBase + 8) {
    const uint8_t* kStartAddr =
        reinterpret_cast<const uint8_t*>(&mtime_) + addr - kMtimeBase;
    memcpy(buffer, kStartAddr, bytes);
  } else {
    return false;
  }

  return true;
}

bool Clint::Store(const uint64_t addr, const uint64_t bytes,
                  const uint8_t* buffer) {
  if (kMsipBase <= addr && addr + bytes <= kMsipBase + harts_ * 4) {
    // msip
    uint8_t* start_addr =
        reinterpret_cast<uint8_t*>(msip_.data()) + addr - kMsipBase;
    memcpy(start_addr, buffer, bytes);
    for (uint64_t i = 0; i < msip_.size(); i++) {
      msip_[i] &= 1;
    }
  } else if (kMtimeCmpBase <= addr &&
             addr + bytes <= kMtimeCmpBase + harts_ * 8) {
    // mtimecmp
    uint8_t* start_addr =
        reinterpret_cast<uint8_t*>(mtimecmp_.data()) + addr - kMtimeCmpBase;
    memcpy(start_addr, buffer, bytes);
  } else if (kMtimeBase <= addr && addr + bytes <= kMtimeBase + 8) {
    uint8_t* start_addr =
        reinterpret_cast<uint8_t*>(&mtime_) + addr - kMtimeBase;
    memcpy(start_addr, buffer, bytes);
  } else {
    return false;
  }

  return true;
}

void Clint::Reset() {
  mtime_ = 0;
  std::fill(mtimecmp_.begin(), mtimecmp_.end(), 0);
  std::fill(msip_.begin(), msip_.end(), 0);
}

bool Clint::MachineTimerIrq(const uint64_t hart_id) {
  return mtime_ > mtimecmp_.at(hart_id);
}

bool Clint::MachineSoftwareIrq(const uint64_t hart_id) {
  return msip_.at(hart_id) & 1;
}

void Clint::Tick() { mtime_++; }

}  // namespace rv64_emulator::device::clint
