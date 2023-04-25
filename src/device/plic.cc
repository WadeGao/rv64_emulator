#include "device/plic.h"

#include <_types/_uint32_t.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace rv64_emulator::device::plic {

Plic::Plic(const uint64_t cores, bool is_s_mode, uint64_t device_num)
    : contexts_(cores * (is_s_mode ? 2 : 1)), dev_num_(device_num) {
  const uint8_t context_per_hart = is_s_mode ? 2 : 1;
  for (size_t i = 0; i < contexts_.size(); i++) {
    contexts_[i].id = i;
    contexts_[i].mmode = is_s_mode ? (i % context_per_hart == 0) : true;
  }
}

void Plic::UpdateExt(const uint32_t src_id, bool fired) {
  const uint32_t kIndex = src_id / kWordBits;
  const uint32_t kOffset = src_id % kWordBits;
  const uint32_t kMask = (1u << kOffset);
  pending_[kIndex] &= ~kMask;
  if (fired) {
    pending_[kIndex] |= kMask;
  }
}

bool Plic::GetInterrupt(const uint64_t ctx_id) {
  uint64_t max_priority = 0;
  uint64_t best_interrupt = 0;
  for (uint64_t i = 0; i < dev_num_; i++) {
    const uint64_t kIndex = i / kWordBits;
    const uint64_t kOffset = i % kWordBits;
    const uint32_t kMask = 1u << kOffset;

    if ((priority_[i] >= contexts_.at(ctx_id).threshold) &&
        (pending_[kIndex] & kMask) &&
        (contexts_.at(ctx_id).enable[kIndex] & kMask) &&
        !(contexts_.at(ctx_id).claimed_[kIndex] & kMask)) {
      if (max_priority < priority_[i]) {
        max_priority = priority_[i];
        best_interrupt = i;
      }
    }
  }
  contexts_.at(ctx_id).claim = best_interrupt;
  return best_interrupt != 0;
}

// https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc#memory-map
bool Plic::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
  if (kSourcePriorityBase <= addr && addr < kPendingBase) {
    const uint64_t kSourceIndex = addr >> 2;
    if (kSourceIndex > dev_num_ - 1) {
      return false;
    }
    memcpy(buffer, &priority_[kSourceIndex], bytes);
    return true;
  } else if (kPendingBase <= addr && addr < 0x1080) {
    const uint64_t kPendingIndex = (addr - kPendingBase) >> 2;

    const uint64_t kMinSource = kPendingIndex * kWordBits;
    if (kMinSource > dev_num_ - 1) {
      return false;
    }

    memcpy(buffer, &pending_[kPendingIndex], bytes);
    return true;
  } else if (kEnableBase <= addr && addr < 0x1f2000) {
    const uint64_t kCtxId = (addr - kEnableBase) / kEnableBytesPerHart;
    if (kCtxId >= contexts_.size()) {
      return false;
    }

    const uint64_t kEnableIndex = (addr % kEnableBytesPerHart) >> 2;

    const uint64_t kMinSource = kEnableIndex * kWordBits;
    if (kMinSource > dev_num_ - 1) {
      return false;
    }

    memcpy(buffer, &contexts_[kCtxId].enable[kEnableIndex], bytes);
    return true;
  } else if (kContextBase <= addr && addr <= kContextEnd) {
    const uint64_t kCtxId = (addr - kContextBase) / kContextBytesPerHart;
    if (kCtxId >= contexts_.size()) {
      return false;
    }

    const uint64_t kOffset = addr % kContextBytesPerHart;
    if (kOffset == 0) {
      const auto kThreshold = contexts_[kCtxId].threshold;
      memcpy(buffer, &kThreshold, bytes);
      return true;
    } else if (kOffset == 4) {
      const auto kClaim = contexts_[kCtxId].claim;
      const uint32_t kMask = (1u << (kClaim % kWordBits));
      memcpy(buffer, &kClaim, bytes);
      contexts_[kCtxId].claimed_[kClaim / kWordBits] |= kMask;
      return true;
    }
  }

  return false;
}

bool Plic::Store(const uint64_t addr, const uint64_t bytes,
                 const uint8_t* buffer) {
  if (kSourcePriorityBase <= addr && addr < kPendingBase) {
    const uint64_t kSourceIndex = addr >> 2;
    if (kSourceIndex > dev_num_ - 1) {
      return false;
    }
    memcpy(&priority_[kSourceIndex], buffer, bytes);
    return true;
  } else if (kPendingBase <= addr && addr < 0x1080) {
    const uint64_t kPendingIndex = (addr - kPendingBase) >> 2;

    const uint64_t kMinSource = kPendingIndex * kWordBits;
    if (kMinSource > dev_num_ - 1) {
      return false;
    }

    memcpy(&pending_[kPendingIndex], buffer, bytes);
    return true;
  } else if (kEnableBase <= addr && addr < 0x1f2000) {
    const uint64_t kCtxId = (addr - kEnableBase) / kEnableBytesPerHart;
    if (kCtxId >= contexts_.size()) {
      return false;
    }

    const uint64_t kEnableIndex = (addr % kEnableBytesPerHart) >> 2;

    const uint64_t kMinSource = kEnableIndex * kWordBits;
    if (kMinSource > dev_num_ - 1) {
      return false;
    }

    memcpy(&(contexts_[kCtxId].enable[kEnableIndex]), buffer, bytes);
    return true;
  } else if (kContextBase <= addr && addr <= kContextEnd) {
    const uint64_t kCtxId = (addr - kContextBase) / kContextBytesPerHart;
    if (kCtxId >= contexts_.size()) {
      return false;
    }

    const uint64_t kOffset = addr % kContextBytesPerHart;
    if (kOffset == 0) {
      memcpy(&(contexts_[kCtxId].threshold), buffer, bytes);
      return true;
    } else if (kOffset == 4) {
      const uint32_t kVal = *reinterpret_cast<const uint32_t*>(buffer);

      if (kVal > dev_num_ - 1) {
        return false;
      }

      const uint32_t kIndex = kVal / kWordBits;
      const uint32_t kOffset = kVal % kWordBits;
      const uint32_t kMask = ~(1u << kOffset);
      contexts_[kCtxId].claimed_[kIndex] &= kMask;
      return true;
    }
  }

  return false;
}

void Plic::Reset() {
  memset(priority_, 0, sizeof(priority_));
  memset(pending_, 0, sizeof(pending_));

  std::fill(contexts_.begin(), contexts_.end(), PlicContext{});
}

}  // namespace rv64_emulator::device::plic
