#include "include/plic.h"
#include "include/csr.h"

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
    if (virtio_ip && virtio_enabled && virtio_priority > priority && virtio_priority > m_threshold) {
        irq      = (uint32_t)Irq::kVirtIO;
        priority = virtio_priority;
    }

    if (uart_ip && uart_enabled && uart_priority > priority && uart_priority > m_threshold) {
        irq      = (uint32_t)Irq::kVirtIO;
        priority = virtio_priority;
    }

    m_irq = irq;
    if (m_irq != 0) {
        mip |= csr::kCsrSeipMask;
    }
}

void PLIC::SetInterruptPending(const uint32_t irq) {
    const uint32_t index  = irq / 32;
    const uint32_t offset = irq % 32;
    m_pending[index] |= (1 << offset);
    m_needs_update_irq = true;
}

void PLIC::ClearInterruptPending(const uint32_t irq) {
    const uint32_t index  = irq / 8;
    const uint32_t offset = irq % 8;
    m_pending[index] &= ~(1 << offset);
    m_needs_update_irq = true;
}

// https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc#memory-map
uint8_t PLIC::Load(const uint64_t addr) {
}

} // namespace plic
} // namespace cpu
} // namespace rv64_emulator
