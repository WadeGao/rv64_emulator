#pragma once

#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "libs/LRU.hpp"
// #include "mmu.h"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace rv64_emulator {

namespace mmu {
class Mmu;
}
namespace cpu {

constexpr uint64_t kGeneralPurposeRegNum = 32;
constexpr uint64_t kFloatingPointRegNum  = 32;

enum class PrivilegeMode {
    kUser = 0,
    kSupervisor,
    kReserved,
    kMachine
};

class CPU {
private:
    uint64_t      m_clock;
    uint64_t      m_instruction_count;
    PrivilegeMode m_privilege_mode;
    uint64_t      m_pc;
    bool          m_wfi;

    uint64_t m_reg[kGeneralPurposeRegNum] = { 0 };

    // F 拓展说明：https://tclin914.github.io/3d45634e/
    float m_fp_reg[kFloatingPointRegNum] = { 0.0 };

    /*
    +───────────+───────────+────────────────────────────────────+────────+
    | Register  | ABI Name  | Description                        | Saver  |
    +───────────+───────────+────────────────────────────────────+────────+
    | x0        | zero      | Hard-wired zero                    |  ----  |
    | x1        | ra        | Return address                     | Caller |
    | x2        | sp        | Stack pointer                      | Callee |
    | x3        | gp        | Global pointer                     |  ----  |
    | x4        | tp        | Thread pointer                     |  ----  |
    | x5        | t0        | Temporary/alternate link register  | Caller |
    | x6 - 7    | t1 - 2    | Temporaries                        | Caller |
    | x8        | s0/fp     | Saved register/frame pointer       | Callee |
    | x9        | s1        | Saved registers                    | Callee |
    | x10 - 11  | a0 - 1    | Function args/return values        | Caller |
    | x12 - 17  | a2 - 7    | Function args                      | Caller |
    | x18 - 27  | s2 - 11   | Saved registers                    | Callee |
    | x28 - 31  | t3 - 6    | Temporaries                        | Caller |
    +───────────+───────────+────────────────────────────────────+────────+
    */

    std::unique_ptr<mmu::Mmu> m_mmu;

    libs::LRUCache<uint32_t, int64_t> m_decode_cache;

    trap::Trap TickOperate();

    void HandleTrap(const trap::Trap trap, const uint64_t inst_addr);
    void HandleInterrupt(const uint64_t inst_addr);

public:
    csr::State m_state;

    CPU(std::unique_ptr<mmu::Mmu> mmu);
    void Reset();

    trap::Trap Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const;
    trap::Trap Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer);

    trap::Trap Fetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer);
    trap::Trap Decode(const uint32_t word, int64_t& index);
    void       Tick();

    void     SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val);
    uint64_t GetGeneralPurposeRegVal(const uint64_t reg_num) const;

    inline void SetPC(const uint64_t new_pc) {
        m_pc = new_pc;
    }

    inline uint64_t GetPC() const {
        return m_pc;
    }

    inline PrivilegeMode GetPrivilegeMode() const {
        return m_privilege_mode;
    }

    inline void SetPrivilegeMode(const PrivilegeMode mode) {
        m_privilege_mode = mode;
    }

    inline bool GetWfi() const {
        return m_wfi;
    }

    inline void SetWfi(const bool wfi) {
        m_wfi = wfi;
    }

    void FlushTlb(const uint64_t vaddr, const uint64_t asid);

    void Disassemble(const uint64_t pc, const uint32_t word, const int64_t instruction_table_index) const;
    void DumpRegisters() const;

    ~CPU();
};

} // namespace cpu
} // namespace rv64_emulator
