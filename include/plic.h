#ifndef RV64_EMULATOR_INCLUDE_PLIC_H_
#define RV64_EMULATOR_INCLUDE_PLIC_H_

#include "include/conf.h"
#include <cstdint>

namespace rv64_emulator {
namespace cpu {
namespace plic {

enum class Irq {
    kVirtIO = 1,
    kUART   = 10,
};

constexpr uint64_t kSourcePriorityBase = kPlicBase;
constexpr uint64_t kSourcePriorityEnd  = kSourcePriorityBase + 0xffc;

constexpr uint64_t kPendingBase = kPlicBase + 0x1000;
constexpr uint64_t kPendingEnd  = kPlicBase + 0x107f;

// two context
constexpr uint64_t kEnableBase = kPlicBase + 0x2000;
constexpr uint64_t kEnableEnd  = kPlicBase + 0x20fc;

class PLIC {
private:
    uint64_t m_clock = 0;
    uint32_t m_irq;
    uint32_t m_threshold      = 0;
    uint32_t m_priority[1024] = { 0 };
    uint8_t  m_pending[128]   = { 0 };
    uint8_t  m_enabled[128]   = { 0 };
    bool     m_needs_update_irq;
    bool     m_virtio_ip_cache;

    void    UpdateIrq(uint64_t& mip);
    void    SetInterruptPending(const uint32_t irq);
    void    ClearInterruptPending(const uint32_t irq);
    uint8_t Load(const uint64_t addr);

public:
    PLIC();
    void Tick(bool virtio_ip, bool uart_ip, uint64_t& mip);
};

} // namespace plic
} // namespace cpu
} // namespace rv64_emulator
#endif