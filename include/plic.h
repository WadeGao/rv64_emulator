#pragma once

#include "conf.h"
#include "cpu/cpu.h"

#include <cstdint>
#include <tuple>
#include <vector>

namespace rv64_emulator {

namespace cpu {
class CPU;
}
using cpu::CPU;

namespace plic {

constexpr uint64_t kSourcePriorityBase = kPlicBase;
constexpr uint64_t kSourcePriorityEnd  = kSourcePriorityBase + 0xfff;

constexpr uint64_t kPendingBase = kPlicBase + 0x1000;

constexpr uint64_t kEnableBase = kPlicBase + 0x2000;

constexpr uint64_t kContextBase = kPlicBase + 0x200000;
constexpr uint64_t kContextEnd  = kPlicBase + 0x3ffffff;

constexpr uint64_t kEnableBytesPerHart  = 0x80;
constexpr uint64_t kContextBytesPerHart = 0x1000;

constexpr uint32_t kMaxPriority = UINT32_MAX;
constexpr uint64_t kWordBits    = sizeof(uint32_t) * 8;

class PLIC {
private:
    typedef struct PlicContext {
        bool     m_is_machine_mode;
        uint64_t context_id;
        uint32_t m_threshold;
        CPU*     cpu;

        uint32_t m_enabled[kPlicMaxDevices / kWordBits] = { 0 };
        uint32_t m_pending[kPlicMaxDevices / kWordBits] = { 0 };
        uint32_t m_claim[kPlicMaxDevices / kWordBits]   = { 0 };

        uint32_t m_priority[kPlicMaxDevices] = { 0 };
    } PlicContext;

    std::vector<CPU*>        m_processors;
    std::vector<PlicContext> m_contexts;
    uint64_t                 m_device_num;
    uint64_t                 m_device_num_word;
    uint32_t                 m_priority[kPlicMaxDevices] = { 0 };

    bool IsInterruptEnabled(const uint64_t context, const uint32_t irq) const;
    void UpdateClaim(const uint32_t irq);

    uint32_t PriorityRead(const uint64_t addr) const;
    void     PriorityWrite(const uint64_t addr, const uint32_t data);

    uint64_t ContextRead(const uint64_t context_id, const uint64_t offset);
    void     ContextWrite(const uint64_t context_id, const uint64_t offset, const uint32_t val);

    uint32_t ContextEnableRead(const uint64_t context_id, const uint64_t offset) const;
    void     ContextEnableWrite(const uint64_t context_id, const uint64_t offset, const uint32_t val);

    uint64_t ContextClaim(const uint64_t context_id);
    void     ContextUpdate(const uint64_t context_id);

    uint64_t SelectBestPending(const uint64_t context_id) const;

public:
    PLIC(std::vector<CPU*> processors, bool is_s_mode, uint64_t device_num);
    uint64_t Load(const uint64_t addr);
    void     Store(const uint64_t addr, const uint32_t data);
};

} // namespace plic
} // namespace rv64_emulator
