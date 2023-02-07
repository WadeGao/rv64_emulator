#ifndef RV64_EMULATOR_INCLUDE_CPU_CPU_H_
#define RV64_EMULATOR_INCLUDE_CPU_CPU_H_

#include "bus.h"
#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "libs/LRU.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace rv64_emulator::cpu {

constexpr uint64_t kGeneralPurposeRegNum = 32;
constexpr uint64_t kFloatingPointRegNum  = 32;
constexpr uint64_t kInstructionBits      = 32;

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
    uint64_t      m_last_executed_pc;

    bool m_wfi = false;

    // TODO: 把 gpr 的类型迁移到 int64_t
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

    std::unique_ptr<rv64_emulator::bus::Bus> m_bus;

    // decode cache, key: inst_word, val: inst_table_index
    rv64_emulator::libs::LRUCache<uint32_t, int64_t> m_decode_cache;

    bool CheckInterruptBitsValid(const PrivilegeMode cur_pm, const PrivilegeMode new_pm, const trap::TrapType trap_type) const;

    void     ModifyCsrStatusReg(const PrivilegeMode cur_pm, const PrivilegeMode new_pm);
    uint64_t GetTrapVectorNewPC(const uint64_t csr_tvec_addr, const uint64_t exception_code) const;

    trap::Trap TickOperate();

public:
    csr::State m_state;

    CPU(PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus);
    void Reset();

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

    uint32_t Fetch();
    int64_t  Decode(const uint32_t inst_word);
    void     Tick();

    void     SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val);
    uint64_t GetGeneralPurposeRegVal(const uint64_t reg_num) const;

    inline void SetPC(const uint64_t new_pc) {
        m_pc = new_pc;
    }

    inline uint64_t GetPC() const {
        return m_pc;
    }

    inline uint64_t GetMaxXLen() const {
        return 64;
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

    bool HandleTrap(const trap::Trap trap, const uint64_t inst_addr);
    void HandleInterrupt(const uint64_t inst_addr);

    void Dump() const;

    ~CPU();
};

typedef struct Instruction {
    uint32_t    m_mask;
    uint32_t    m_data;
    const char* m_name;

    trap::Trap (*Exec)(CPU* cpu, const uint32_t inst_word);
    // std::string Disassemble() const;
} Instruction;

} // namespace rv64_emulator::cpu

#endif