#ifndef RV64_EMULATOR_INCLUDE_CPU_H_
#define RV64_EMULATOR_INCLUDE_CPU_H_

#include "include/bus.h"
#include "include/conf.h"
#include "include/decoder.h"

#include <cstdint>
#include <memory>

namespace rv64_emulator {
namespace cpu {

class CPU {
private:
    uint64_t m_pc;
    uint64_t m_reg[RV64_REGISTER_NUM] = {0};

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

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

    void Exec_ADDI(const uint32_t instruction);
    void Exec_SLLI(const uint32_t instruction);
    void Exec_SLTI(const uint32_t instruction);
    void Exec_SLTIU(const uint32_t instruction);
    void Exec_XORI(const uint32_t instruction);
    void Exec_SRLI(const uint32_t instruction);
    void Exec_SRAI(const uint32_t instruction);
    void Exec_ORI(const uint32_t instruction);
    void Exec_ANDI(const uint32_t instruction);

    void Exec_AUIPC(const uint32_t instruction);
    void Exec_LUI(const uint32_t instruction);

    void Exec_JAL(const uint32_t instruction);
    void Exec_JALR(const uint32_t instruction);

    void Exec_BEQ(const uint32_t instruction);
    void Exec_BNE(const uint32_t instruction);
    void Exec_BLT(const uint32_t instruction);
    void Exec_BGE(const uint32_t instruction);
    void Exec_BLTU(const uint32_t instruction);
    void Exec_BGEU(const uint32_t instruction);

    void Exec_LB(const uint32_t instruction);
    void Exec_LH(const uint32_t instruction);
    void Exec_LW(const uint32_t instruction);
    void Exec_LD(const uint32_t instruction);
    void Exec_LBU(const uint32_t instruction);
    void Exec_LHU(const uint32_t instruction);
    void Exec_LWU(const uint32_t instruction);

    void Exec_SB(const uint32_t instruction);
    void Exec_SH(const uint32_t instruction);
    void Exec_SW(const uint32_t instruction);
    void Exec_SD(const uint32_t instruction);

public:
    CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus);
    uint32_t Fetch() const;
    uint64_t Execute(const uint32_t instruction);
    ~CPU();
};

} // namespace cpu
} // namespace rv64_emulator
#endif