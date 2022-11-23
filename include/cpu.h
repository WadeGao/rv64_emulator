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
    uint64_t m_reg[RV64_GENERAL_PURPOSE_REG_NUM] = { 0 };

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

    // RV32I Base Instruction Set
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
    void Exec_LBU(const uint32_t instruction);
    void Exec_LHU(const uint32_t instruction);

    void Exec_SB(const uint32_t instruction);
    void Exec_SH(const uint32_t instruction);
    void Exec_SW(const uint32_t instruction);

    void Exec_ADD(const uint32_t instruction);
    void Exec_SUB(const uint32_t instruction);
    void Exec_SLL(const uint32_t instruction);
    void Exec_SLT(const uint32_t instruction);
    void Exec_SLTU(const uint32_t instruction);
    void Exec_XOR(const uint32_t instruction);
    void Exec_SRL(const uint32_t instruction);
    void Exec_SRA(const uint32_t instruction);
    void Exec_OR(const uint32_t instruction);
    void Exec_AND(const uint32_t instruction);

    void Exec_FENCE(const uint32_t instruction);
    void Exec_FENCE_I(const uint32_t instruction);
    void Exec_ECALL(const uint32_t instruction);
    void Exec_EBREAK(const uint32_t instruction);
    void Exec_CSRRW(const uint32_t instruction);
    void Exec_CSRRS(const uint32_t instruction);
    void Exec_CSRRC(const uint32_t instruction);
    void Exec_CSRRWI(const uint32_t instruction);
    void Exec_CSRRSI(const uint32_t instruction);
    void Exec_CSRRCI(const uint32_t instruction);

    // RV64I Base Instruction Set (in addition to RV32I)
    void Exec_LWU(const uint32_t instruction);
    void Exec_LD(const uint32_t instruction);
    void Exec_SD(const uint32_t instruction);

    void Exec_ADDIW(const uint32_t instruction);
    void Exec_SLLIW(const uint32_t instruction);
    void Exec_SRLIW(const uint32_t instruction);
    void Exec_SRAIW(const uint32_t instruction);
    void Exec_ADDW(const uint32_t instruction);
    void Exec_SUBW(const uint32_t instruction);
    void Exec_SLLW(const uint32_t instruction);
    void Exec_SRLW(const uint32_t instruction);
    void Exec_SRAW(const uint32_t instruction);

    /*********************** TODO ***************************/

    // RV32M Standard Extension
    void Exec_MUL(const uint32_t instruction);
    void Exec_MULH(const uint32_t instruction);
    void Exec_MULHSU(const uint32_t instruction);
    void Exec_MULHU(const uint32_t instruction);
    void Exec_DIV(const uint32_t instruction);
    void Exec_DIVU(const uint32_t instruction);
    void Exec_REM(const uint32_t instruction);
    void Exec_REMU(const uint32_t instruction);

    //  RV64M Standard Extension (in addition to RV32M)
    void Exec_MULW(const uint32_t instruction);
    void Exec_DIVW(const uint32_t instruction);
    void Exec_DIVUW(const uint32_t instruction);
    void Exec_REMW(const uint32_t instruction);
    void Exec_REMUW(const uint32_t instruction);

    /*********************** TODO ***************************/
public:
    CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus);

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

    uint32_t Fetch();
    uint64_t Execute(const uint32_t instruction);

    void     SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val);
    uint64_t GetGeneralPurposeRegVal(const uint64_t reg_num) const;

    void     SetPC(const uint64_t new_pc);
    uint64_t GetPC() const;

#ifdef DEBUG
    void Dump() const;
#endif

    inline void IllegalInstructionHandler(const char* info, const uint32_t instruction) const;
    ~CPU();
};

typedef struct Instruction {
    uint32_t    m_mask;
    uint32_t    m_data;
    const char* m_name;

    void (*Exec)(CPU* cpu, const uint32_t instruction);
    // std::string Disassemble() const;
} Instruction;

} // namespace cpu
} // namespace rv64_emulator
#endif