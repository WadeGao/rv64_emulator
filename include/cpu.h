#ifndef RV64_EMULATOR_INCLUDE_CPU_H_
#define RV64_EMULATOR_INCLUDE_CPU_H_

#include "include/bus.h"
#include "include/conf.h"
#include "include/decode.h"
#include "libs/LRU.hpp"

#include <cstdint>
#include <memory>

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

class CPU {
private:
    ArchMode      m_arch_mode;
    PrivilegeMode m_privilege_mode;
    uint64_t      m_pc;
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

public:
    CPU(ArchMode arch_mode, PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus);

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

    uint32_t Fetch();
    int64_t  Decode(const uint32_t inst_word);
    uint64_t Execute(const uint32_t inst_word);

    void     SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val);
    uint64_t GetGeneralPurposeRegVal(const uint64_t reg_num) const;

    void     SetPC(const uint64_t new_pc);
    uint64_t GetPC() const;

    ArchMode      GetArchMode() const;
    PrivilegeMode GetPrivilegeMode() const;

    bool HasCsrAccessPrivilege(const uint16_t csr_num) const;

#ifdef DEBUG
    void Dump() const;
#endif

    ~CPU();
};

typedef struct Instruction {
    uint32_t    m_mask;
    uint32_t    m_data;
    const char* m_name;

    void (*Exec)(CPU* cpu, const uint32_t inst_word);
    // std::string Disassemble() const;
} Instruction;

} // namespace cpu
} // namespace rv64_emulator

#endif