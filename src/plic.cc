#include "include/plic.h"
#include "include/csr.h"

#include <cassert>
#include <cstdint>

namespace rv64_emulator {
namespace cpu {
namespace plic {

void PLIC::Tick(bool virtio_ip, bool uart_ip, uint64_t& mip) {
    m_clock = (m_clock + 1) % UINT64_MAX;

    // edge-triggered interrupt
    if (m_virtio_ip_cache != virtio_ip) {
        if (virtio_ip) {
            SetInterruptPending((uint32_t)Irq::kVirtIO);
        }
        m_virtio_ip_cache = virtio_ip;
    }

    if (uart_ip) {
        SetInterruptPending((uint32_t)Irq::kUART);
    }
    if (m_needs_update_irq) {
        m_needs_update_irq = false;
        UpdateIrq(mip);
    }
}

bool PLIC::IsInterruptEnabled(const uint8_t context, const uint32_t irq) const {
    const uint32_t index  = irq / 32;
    const uint32_t offset = irq % 32;
    return ((m_enabled[context * 32 + index] >> offset) & 1) == 1;
}

void PLIC::UpdateClaim(const uint32_t irq) {
    if (IsInterruptEnabled(1, irq) || irq == 0) {
        m_claim[1] = irq;
    }
}
void PLIC::SetInterruptPending(const uint32_t irq) {
    const uint32_t index  = irq / 32;
    const uint32_t offset = irq % 32;
    m_pending[index] |= (1 << offset);

    // TODO: 使用哪个版本
    m_needs_update_irq = true;
    UpdateClaim(irq);
}

void PLIC::ClearInterruptPending(const uint32_t irq) {
    const uint32_t index  = irq / 32;
    const uint32_t offset = irq % 32;
    m_pending[index] &= ~(1 << offset);

    // TODO: 使用哪个版本
    m_needs_update_irq = true;
    UpdateClaim(0);
}

void PLIC::UpdateIrq(uint64_t& mip) {
    bool virtio_ip = (m_pending[(uint32_t)Irq::kVirtIO / 8] >> ((uint32_t)Irq::kVirtIO % 8)) & 1;
    bool uart_ip   = (m_pending[(uint32_t)Irq::kUART / 8] >> ((uint32_t)Irq::kUART % 8)) & 1;

    bool virtio_enabled = (m_enabled[(uint32_t)Irq::kVirtIO / 8] >> ((uint32_t)Irq::kVirtIO % 8)) & 1;
    bool uart_enabled   = (m_enabled[(uint32_t)Irq::kUART / 8] >> ((uint32_t)Irq::kUART % 8)) & 1;

    const uint32_t virtio_priority = m_priority[(uint32_t)Irq::kVirtIO];
    const uint32_t uart_priority   = m_priority[(uint32_t)Irq::kUART];

    uint32_t irq      = 0;
    uint32_t priority = 0;

    // TODO: 这里有针对 virtio 和 uart 中断设置优先级吗
    if (virtio_ip && virtio_enabled && virtio_priority > priority && virtio_priority > m_threshold[1]) {
        irq      = (uint32_t)Irq::kVirtIO;
        priority = virtio_priority;
    }

    if (uart_ip && uart_enabled && uart_priority > priority && uart_priority > m_threshold[1]) {
        irq      = (uint32_t)Irq::kVirtIO;
        priority = virtio_priority;
    }

    m_irq = irq;
    if (m_irq != 0) {
        mip |= csr::kCsrSeipMask;
    }
}

// https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc#memory-map
uint64_t PLIC::Load(const uint64_t addr) {
    if (addr >= kSourcePriorityBase && addr <= kSourcePriorityEnd) {
        assert((addr - kSourcePriorityBase) % 4 == 0);
        const uint32_t index = (addr - kSourcePriorityBase) / 4;
        return m_priority[index];
    }

    if (addr >= kPendingBase && addr <= kPendingEnd) {
        assert((addr - kPendingBase) % 4 == 0);
        const uint32_t index = (addr - kPendingBase) / 4;
        return m_pending[index];
    }

    if (addr >= kEnableBase && addr <= kEnableEnd) {
        assert((addr - kEnableBase) % 4 == 0);
        const uint32_t index = (addr - kEnableBase) / 4;
        return m_enabled[index];
    }

    if (addr >= kThresholdAndClaimBase && addr <= kThresholdAndClaimEnd) {
        const uint8_t  context = (addr - kThresholdAndClaimBase) / 0x1000;
        const uint64_t offset  = (addr - kThresholdAndClaimBase) % 0x1000;
        switch (offset) {
            case 0:
                return m_threshold[context];
            case 4:
                return m_claim[context];
            default:
                break;
        }
    }

    assert(false);
    return 0;
}

void PLIC::Store(const uint64_t addr, const uint32_t data) {
    if (addr >= kSourcePriorityBase && addr <= kSourcePriorityEnd) {
        assert((addr - kSourcePriorityBase) % 4 == 0);
        const uint32_t index = (addr - kSourcePriorityBase) / 4;
        m_priority[index]    = data;
        return;
    }

    if (addr >= kPendingBase && addr <= kPendingEnd) {
        assert((addr - kPendingBase) % 4 == 0);
        const uint32_t index = (addr - kPendingBase) / 4;
        m_pending[index]     = data;
    }

    if (addr >= kEnableBase && addr <= kEnableEnd) {
        assert((addr - kEnableBase) % 4 == 0);
        const uint32_t index = (addr - kEnableBase) / 4;
        m_enabled[index]     = data;
        return;
    }

    if (addr >= kThresholdAndClaimBase && addr <= kThresholdAndClaimEnd) {
        const uint8_t  context = (addr - kThresholdAndClaimBase) / 0x1000;
        const uint64_t offset  = (addr - kThresholdAndClaimBase) % 0x1000;
        switch (offset) {
            case 0:
                m_threshold[context] = data;
                break;
            case 4:
                ClearInterruptPending(data);
                break;
            default:
                break;
        }
    }

    assert(false);
}

} // namespace plic
} // namespace cpu
} // namespace rv64_emulator
