#ifndef RV64_EMULATOR_INCLUDE_CPU_H_
#define RV64_EMULATOR_INCLUDE_CPU_H_

#include "include/bus.h"
#include "include/conf.h"
#include "include/csr.h"
#include "include/decode.h"
#include "libs/LRU.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>

namespace rv64_emulator {
namespace cpu {

constexpr uint64_t kGeneralPurposeRegNum = 32;
constexpr uint64_t kFloatingPointRegNum  = 32;
constexpr uint64_t kInstructionBits      = 32;
constexpr uint64_t kCsrCapacity          = 4096;

enum class ArchMode {
    kBit32 = 0,
    kBit64
};

enum class PrivilegeMode {
    kUser = 0,
    kSupervisor,
    kReserved,
    kMachine
};

enum class TrapType {
    kInstructionAddressMisaligned = 0,
    kInstructionAccessFault,
    kIllegalInstruction,
    kBreakpoint,
    kLoadAddressMisaligned,
    kLoadAccessFault,
    kStoreAddressMisaligned,
    kStoreAccessFault,
    kEnvironmentCallFromUMode,
    kEnvironmentCallFromSMode,
    kEnvironmentCallFromMMode,
    kInstructionPageFault,
    kLoadPageFault,
    kStorePageFault,
    // three standard interrupt source: software, timer, external
    kUserSoftwareInterrupt,
    kSupervisorSoftwareInterrupt,
    kMachineSoftwareInterrupt,
    kUserTimerInterrupt,
    kSupervisorTimerInterrupt,
    kMachineTimerInterrupt,
    kUserExternalInterrupt,
    kSupervisorExternalInterrupt,
    kMachineExternalInterrupt,
    kNone
};

const std::map<TrapType, uint64_t> TrapToCauseTable = {
    { TrapType::kInstructionAddressMisaligned, 0 },
    { TrapType::kInstructionAccessFault, 1 },
    { TrapType::kIllegalInstruction, 2 },
    { TrapType::kBreakpoint, 3 },
    { TrapType::kLoadAddressMisaligned, 4 },
    { TrapType::kLoadAccessFault, 5 },
    { TrapType::kStoreAddressMisaligned, 6 },
    { TrapType::kStoreAccessFault, 7 },
    { TrapType::kEnvironmentCallFromUMode, 8 },
    { TrapType::kEnvironmentCallFromSMode, 9 },
    { TrapType::kEnvironmentCallFromMMode, 11 },
    { TrapType::kInstructionPageFault, 12 },
    { TrapType::kLoadPageFault, 13 },
    { TrapType::kStorePageFault, 15 },
    /* ----------- belows are interrupts ----------- */
    { TrapType::kUserSoftwareInterrupt, 0 },
    { TrapType::kSupervisorSoftwareInterrupt, 1 },
    { TrapType::kMachineSoftwareInterrupt, 3 },
    { TrapType::kUserTimerInterrupt, 4 },
    { TrapType::kSupervisorTimerInterrupt, 5 },
    { TrapType::kMachineTimerInterrupt, 7 },
    { TrapType::kUserExternalInterrupt, 8 },
    { TrapType::kSupervisorExternalInterrupt, 9 },
    { TrapType::kMachineExternalInterrupt, 11 },
};

typedef struct Trap {
    TrapType m_trap_type;
    uint64_t m_val;
} Trap;

class CPU {
private:
    uint64_t      m_clock;
    ArchMode      m_arch_mode;
    PrivilegeMode m_privilege_mode;
    uint64_t      m_pc;
    uint64_t      m_mstatus;

    bool m_wfi = false;
    // TODO: 把 gpr 的类型迁移到 int64_t
    uint64_t m_reg[kGeneralPurposeRegNum]   = { 0 };
    double   m_fp_reg[kFloatingPointRegNum] = { 0.0 };
    static_assert(sizeof(double) == 8, "double is not 8 bytes, can't assure the bit width of floating point reg");

    // TODO: 是否会超过栈大小？是否用 std::array?
    uint64_t m_csr[kCsrCapacity] = { 0 };

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

    uint64_t ReadCsrDirectly(const uint16_t csr_addr) const;
    void     WriteCsrDirectly(const uint16_t csr_addr, const uint64_t val);

    void UpdateMstatus(const uint64_t mstatus);

    uint64_t                   GetTrapCause(const Trap trap) const;
    std::tuple<bool, uint64_t> RefactorGetTrapCause(const Trap trap) const;
    uint64_t                   GetCurrentStatus(const PrivilegeMode mode) const;
    uint64_t                   GetInterruptEnable(const PrivilegeMode mode) const;

public:
    CPU(ArchMode arch_mode, PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus);

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

    uint32_t Fetch();
    int64_t  Decode(const uint32_t inst_word);
    uint64_t Execute(const uint32_t inst_word);

    void     SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val);
    uint64_t GetGeneralPurposeRegVal(const uint64_t reg_num) const;

    inline void     SetPC(const uint64_t new_pc);
    inline uint64_t GetPC() const;

    inline ArchMode      GetArchMode() const;
    inline PrivilegeMode GetPrivilegeMode() const;
    inline void          SetPrivilegeMode(const PrivilegeMode mode);

    inline bool HasCsrAccessPrivilege(const uint16_t csr_num) const;

    std::tuple<uint64_t, Trap> ReadCsr(const uint16_t csr_addr) const;
    Trap                       WriteCsr(const uint16_t csr_addr, const uint64_t val);

    bool RefactorHandleTrap(const Trap trap, const uint64_t inst_addr);
    void RefactoHandleException(const Trap exception, const uint64_t inst_addr);
    void RefactoHandleInterrupt(const uint64_t inst_addr);

    bool HandleTrap(const Trap trap, const uint64_t inst_addr, bool is_interrupt);
    void HandleException(const Trap exception, const uint64_t inst_addr);
    void HandleInterrupt(const uint64_t inst_addr);

#ifdef DEBUG
    void Dump() const;
#endif

    ~CPU();
};

typedef struct Instruction {
    uint32_t    m_mask;
    uint32_t    m_data;
    const char* m_name;

    Trap (*Exec)(CPU* cpu, const uint32_t inst_word);
    // std::string Disassemble() const;
} Instruction;

} // namespace cpu
} // namespace rv64_emulator

#endif