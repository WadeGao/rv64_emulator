#pragma once

#include "conf.h"
#include <cstdint>

namespace rv64_emulator::cpu::plic {

enum class Irq {
    kVirtIO = 1,
    kUART   = 10,
};

constexpr uint64_t kSourcePriorityBase = kPlicBase;
constexpr uint64_t kSourcePriorityEnd  = kSourcePriorityBase + 0xfff;

constexpr uint64_t kPendingBase = kPlicBase + 0x1000;
constexpr uint64_t kPendingEnd  = kPlicBase + 0x107f;

// two context
constexpr uint64_t kEnableBase = kPlicBase + 0x2000;
constexpr uint64_t kEnableEnd  = kPlicBase + 0x20ff;

// two context
constexpr uint64_t kThresholdAndClaimBase = kPlicBase + 0x200000;
constexpr uint64_t kThresholdAndClaimEnd  = kPlicBase + 0x201007;

class PLIC {
private:
    uint64_t m_clock = 0;
    uint32_t m_irq;
    uint32_t m_threshold[2]   = { 0 };
    uint32_t m_claim[2]       = { 0 };
    uint32_t m_priority[1024] = { 0 };
    uint32_t m_pending[32]    = { 0 };
    uint32_t m_enabled[64]    = { 0 };
    bool     m_needs_update_irq;
    bool     m_virtio_ip_cache;

    bool IsInterruptEnabled(const uint8_t context, const uint32_t irq) const;
    void UpdateClaim(const uint32_t irq);
    void UpdateIrq(uint64_t& mip);
    void SetInterruptPending(const uint32_t irq);
    void ClearInterruptPending(const uint32_t irq);

public:
    PLIC();
    void     Tick(bool virtio_ip, bool uart_ip, uint64_t& mip);
    uint64_t Load(const uint64_t addr);
    void     Store(const uint64_t addr, const uint32_t data);
};

} // namespace rv64_emulator::cpu::plic
