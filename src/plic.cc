#include "plic.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "error_code.h"
#include "fmt/core.h"

namespace rv64_emulator::plic {

template <typename T>
static std::enable_if_t<(std::is_unsigned_v<T>), std::tuple<T, T>> GetQuoRem(
    const T id, const T group_bits) {
  const T quotient = id / group_bits;
  const T remain = id % group_bits;
  return {quotient, remain};
}

static void AssertAddressAligned(const uint64_t addr,
                                 const uint64_t align_bytes) {
  if (addr % align_bytes != 0) {
#ifdef DEBUG
    fmt::print("addr misalignde, addr[{}], now abort\n", addr);
#endif
    exit(static_cast<int>(errorcode::PlicErrorCode::kAddrMisAlign));
  }
}

PLIC::PLIC(std::vector<CPU*> processors, bool is_s_mode, uint64_t device_num)
    : m_processors(processors),
      m_contexts(processors.size() * (is_s_mode ? 2 : 1)),
      m_device_num(device_num),
      m_device_num_word((device_num + kWordBits - 1) / kWordBits) {
  const uint8_t context_per_hart = is_s_mode ? 2 : 1;

  for (size_t i = 0; i < m_contexts.size(); i++) {
    m_contexts[i].context_id = i;
    m_contexts[i].cpu = processors[i / context_per_hart];
    m_contexts[i].m_is_machine_mode =
        is_s_mode ? (i % context_per_hart == 0) : true;
  }
}

bool PLIC::IsInterruptEnabled(const uint64_t context_id,
                              const uint32_t irq) const {
  const auto [index, offset] =
      GetQuoRem(irq, static_cast<decltype(irq)>(kWordBits));
  return ((m_contexts[context_id].m_enabled[index] >> offset) & 1) == 1;
}

uint32_t PLIC::PriorityRead(const uint64_t addr) const {
  const uint64_t index = (addr - kSourcePriorityBase) >> 2;
  if (index > 0 && index < m_device_num) {
    return m_priority[index];
  }

#ifdef DEBUG
  fmt::print(
      "read plic priority failed, addr[{}], index[{}], m_device_num[{}]. now "
      "abort\n",
      addr, index, m_device_num);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kPriorityReadFailure));
  return 0;
}

void PLIC::PriorityWrite(const uint64_t addr, const uint32_t data) {
  const uint64_t index = (addr - kSourcePriorityBase) >> 2;

  if (index > 0 && index < m_device_num) {
    m_priority[index] = data;
  }

#ifdef DEBUG
  fmt::print(
      "write plic priority failed, addr[{}], index[{}], m_device_num[{}], "
      "data[{}]. now abort\n",
      addr, index, m_device_num, data);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kPriorityWriteFailure));
}

uint32_t PLIC::ContextEnableRead(const uint64_t context_id,
                                 const uint64_t offset) const {
  const uint64_t index = offset >> 2;

  if (index < m_device_num_word) {
    return m_contexts[context_id].m_enabled[index];
  }

#ifdef DEBUG
  fmt::print(
      "read context enable field failed, context_id[{}], offset[{}], "
      "m_device_num[{}]. now abort\n",
      context_id, offset, m_device_num);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kContextEnableReadFailure));
  return 0;
}

void PLIC::ContextEnableWrite(const uint64_t context_id, const uint64_t offset,
                              const uint32_t val) {
  const uint64_t index = offset >> 2;

  if (index >= m_device_num_word) {
#ifdef DEBUG
    fmt::print(
        "context enable write failure,index greater than device num, "
        "context_id[{}], offset[{}]. now abort\n",
        context_id, offset);
#endif
    exit(
        static_cast<int>(errorcode::PlicErrorCode::kContextEnableWriteFailure));
  }

  PlicContext& c = m_contexts[context_id];

  const uint32_t old_val = c.m_enabled[index];
  const uint32_t new_val = index == 0 ? (val & ~(uint32_t)1) : val;
  const uint32_t xor_val = new_val & old_val;

  c.m_enabled[index] = new_val;

  for (uint64_t i = 0; i < kWordBits; i++) {
    const uint64_t id = index * kWordBits + i;
    const uint32_t mask = 1 << i;
    uint32_t id_priority = m_priority[id];

    // 当前中断没有改变 enable 位
    if (!(xor_val & mask)) {
      continue;
    }

    // 此中断没有被 enable
    if (!(new_val & mask)) {
      c.m_pending[index] &= ~mask;
      c.m_claim[index] &= ~mask;
      c.m_priority[id] = 0;
    } else {
      // TODO: 需要设置 level
    }
  }
}

uint64_t PLIC::ContextRead(const uint64_t context_id, const uint64_t offset) {
  switch (offset) {
    case 0:
      return m_contexts[context_id].m_threshold;
    case 4:
      return ContextClaim(context_id);
    default:
      break;
  }

#ifdef DEBUG
  fmt::print("read context failed, context_id[{}], offset[{}]. now abort\n",
             context_id, offset);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kContextReadFailure));

  return 0;
}

void PLIC::ContextWrite(const uint64_t context_id, const uint64_t offset,
                        const uint32_t val) {
  PlicContext& c = m_contexts[context_id];

  bool write_succ = true;
  bool update = false;
  switch (offset) {
    case 0:
      if (val <= kMaxPriority) {
        c.m_threshold = val;
        update = true;
      } else {
        write_succ = false;
      }
      break;
    case 4: {
      // now val is device index
      const auto [word, offset] =
          GetQuoRem(val, static_cast<decltype(val)>(kWordBits));
      const uint32_t mask = (1 << offset);
      // 设备号有效，且本设备中断开启
      if ((val < m_device_num) && (c.m_enabled[word] & mask)) {
        c.m_claim[word] &= ~mask;
        update = true;
      }
    } break;
    default:
      write_succ = false;
      break;
  }

  if (!write_succ) {
#ifdef DEBUG
    fmt::print(
        "write context failed, context_id[{}], offset[{}] val[{}]. now abort\n",
        context_id, offset, val);
#endif
    exit(static_cast<int>(errorcode::PlicErrorCode::kContextWriteFailure));
  }

  if (update) {
    ContextUpdate(context_id);
  }
}

// 选出本 context 中没被 claim 的且被挂起的优先级最高的中断号
uint64_t PLIC::SelectBestPending(const uint64_t context_id) const {
  uint32_t best_pending_priority = 0;
  uint64_t best_pending_id = 0;

  const auto& c = m_contexts[context_id];

  for (uint64_t i = 0; i < m_device_num_word; i++) {
    // 本组 32 个中断号全部没有被 pending，跳过
    if (!c.m_pending[i]) {
      continue;
    }

    for (uint8_t j = 0; j < kWordBits; j++) {
      const uint64_t id = i * kWordBits + j;
      // 当前 id 超过了最大设备数 || 本 id 没有被 pending || 本 id 已经被 claim
      if ((id >= m_device_num) || !(c.m_pending[i] & (1 << j)) ||
          (c.m_claim[i] & (1 << j))) {
        continue;
      }

      // 当前 id 的优先级更大
      if (!best_pending_id || best_pending_priority < c.m_priority[id]) {
        best_pending_id = id;
        best_pending_priority = c.m_priority[id];
      }
    }
  }

  return best_pending_id;
}

uint64_t PLIC::ContextClaim(const uint64_t context_id) {
  const uint64_t best_id = SelectBestPending(context_id);
  const auto [best_id_word, offset] = GetQuoRem(best_id, kWordBits);
  const uint32_t mask = (1 << offset);

  if (best_id) {
    m_contexts[context_id].m_claim[best_id_word] |= mask;
    m_contexts[context_id].m_pending[best_id_word] &= ~mask;
  }

  ContextUpdate(context_id);
  return best_id;
}

void PLIC::ContextUpdate(const uint64_t context_id) {
  const uint64_t best_id = SelectBestPending(context_id);
  const uint64_t mask = m_contexts[context_id].m_is_machine_mode
                            ? cpu::csr::kCsrMeipMask
                            : cpu::csr::kCsrSeipMask;
  const uint64_t val =
      m_contexts[context_id].cpu->state_.Read(cpu::csr::kCsrMip);
  // 中断号为 0，始终清除 eip 位
  // 中断号非 0，设置 eip 位
  const uint64_t new_val = (val & ~mask) | (best_id ? mask : 0);
  m_contexts[context_id].cpu->state_.Write(cpu::csr::kCsrMip, new_val);
}

// https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc#memory-map
uint64_t PLIC::Load(const uint64_t addr) {
  AssertAddressAligned(addr, 4);

  if (kSourcePriorityBase <= addr && addr < kEnableBase) {
    return PriorityRead(addr);
  }

  if (kEnableBase <= addr && addr < kContextBase) {
    const auto [context_id, offset] =
        GetQuoRem((addr - kEnableBase), kEnableBytesPerHart);
    if (context_id < m_contexts.size()) {
      return ContextEnableRead(context_id, offset);
    }
  }

  if (kContextBase <= addr && addr <= kContextEnd) {
    const auto [context_id, offset] =
        GetQuoRem((addr - kContextBase), kContextBytesPerHart);
    if (context_id < m_contexts.size()) {
      return ContextRead(context_id, offset);
    }
  }

#ifdef DEBUG
  fmt::print("load plic failed, addr[{}]. now abort\n", addr);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kLoadFailure));

  return 0;
}

void PLIC::Store(const uint64_t addr, const uint32_t val) {
  AssertAddressAligned(addr, 4);

  if (kSourcePriorityBase <= addr && addr < kEnableBase) {
    PriorityWrite(addr, val);
    return;
  }

  if (kEnableBase <= addr && addr < kContextBase) {
    const auto [context_id, offset] =
        GetQuoRem(addr - kEnableBase, kEnableBytesPerHart);
    if (context_id < m_contexts.size()) {
      ContextEnableWrite(context_id, offset, val);
      return;
    }
  }

  if (kContextBase <= addr && addr <= kContextEnd) {
    const auto [context_id, offset] =
        GetQuoRem((addr - kContextBase), kContextBytesPerHart);
    if (context_id < m_contexts.size()) {
      ContextWrite(context_id, offset, val);
      return;
    }
  }

#ifdef DEBUG
  fmt::print("store plic failed, addr[{}] val[{}]. now abort\n", addr, val);
#endif

  exit(static_cast<int>(errorcode::PlicErrorCode::kStoreFailure));

  assert(false);
}

}  // namespace rv64_emulator::plic
