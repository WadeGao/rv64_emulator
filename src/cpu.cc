#include "include/cpu.h"
#include "include/bus.h"
#include "include/conf.h"
#include "include/decoder.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace rv64_emulator {
namespace cpu {

static inline void AssertPC_Aligned(const uint64_t pc) {
    if (pc & 0b0011) {
#ifdef DEBUG
        printf("PC address misalligned, now it is %lu\n", pc);
#endif
        assert(false);
    }
}

CPU::CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_pc(DRAM_BASE)
    , m_bus(std::move(bus)) {
    m_reg[0] = 0;
    m_reg[2] = DRAM_BASE + DRAM_SIZE;
#ifdef DEBUG
    printf("cpu init, bus addr is %p\n", m_bus.get());
#endif
}

uint32_t CPU::Fetch() const {
    uint32_t instruction = m_bus->Load(m_pc, RV64_INSTRUCTION_BIT_SIZE);
    return instruction;
}

uint64_t CPU::Load(const uint64_t addr, const uint64_t bit_size) const {
    return m_bus->Load(addr, bit_size);
}

void CPU::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    m_bus->Store(addr, bit_size, val);
}

uint64_t CPU::Execute(const uint32_t instruction) {
    const uint8_t opcode = rv64_emulator::decoder::GetOpCode(instruction);
    const uint8_t funct7 = rv64_emulator::decoder::GetFunct7(instruction);
    const uint8_t funct3 = rv64_emulator::decoder::GetFunct3(instruction);

    Exec_SLTIU(instruction);

    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;

    return 0;
}

void CPU::Exec_ADDI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = (int64_t)m_reg[rs1] + (int64_t)imm;

#ifdef DEBUG
    printf("addi x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SLLI(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction);

    m_reg[rd] = (int64_t)m_reg[rs1] << (int64_t)shamt;

#ifdef DEBUG
    printf("slli x%u, x%u, 0x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_SLTI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = ((int64_t)m_reg[rs1] < (int64_t)imm) ? 1 : 0;

#ifdef DEBUG
    printf("slti x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SLTIU(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = ((uint64_t)m_reg[rs1] < (uint64_t)imm) ? 1 : 0;

#ifdef DEBUG
    printf("sltiu x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_XORI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = m_reg[rs1] ^ imm;

#ifdef DEBUG
    printf("xori x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SRLI(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction);

    m_reg[rd] = (int64_t)m_reg[rs1] >> (int64_t)shamt;

#ifdef DEBUG
    printf("srli x%u, x%u, 0x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_SRAI(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction);

    m_reg[rd] = int64_t(m_reg[rs1]) >> shamt;

#ifdef DEBUG
    printf("srai x%u, x%u, 0x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_ORI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = m_reg[rs1] | imm;
#ifdef DEBUG
    printf("ori x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_ANDI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = m_reg[rs1] & imm;

#ifdef DEBUG
    printf("andi x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_AUIPC(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::U_Type);

    m_reg[rd] = (int64_t)m_pc + (int64_t)imm - 4;

#ifdef DEBUG
    printf("auipc x%u, 0x%x\n", rd, imm);
#endif
}

void CPU::Exec_LUI(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::U_Type);

    m_reg[rd] = (uint64_t)imm;

#ifdef DEBUG
    printf("lui x%u, 0x%x\n", rd, imm);
#endif
}

void CPU::Exec_JAL(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::J_Type);

    m_reg[rd] = m_pc;
    m_pc      = (int64_t)m_pc + (int64_t)imm - 4;

#ifdef DEBUG
    printf("jal x%u, 0x%x\n", rd, imm);
#endif

    AssertPC_Aligned(m_pc);
}

void CPU::Exec_JALR(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    uint64_t saved_pc = m_pc;

    m_pc      = ((int64_t)m_reg[rs1] + (int64_t)imm) & 0xfffffffe;
    m_reg[rd] = saved_pc;

#ifdef DEBUG
    printf("jalr x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif

    AssertPC_Aligned(m_pc);
}

void CPU::Exec_BEQ(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((int64_t)m_reg[rs1] == (int64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("beq x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_BNE(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((int64_t)m_reg[rs1] != (int64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("bne x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_BLT(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((int64_t)m_reg[rs1] < (int64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("blt x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_BGE(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((int64_t)m_reg[rs1] >= (int64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("bge x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_BLTU(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((uint64_t)m_reg[rs1] < (uint64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("bltu x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_BGEU(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::B_Type);

    if ((uint64_t)m_reg[rs1] >= (uint64_t)m_reg[rs2]) {
        m_pc += ((int64_t)imm - 4);
    }

#ifdef DEBUG
    printf("bgeu x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_LB(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    const int8_t   data = (int8_t)(Load(addr, 8));
    m_reg[rd]           = (int64_t)data;

#ifdef DEBUG
    printf("lb x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LH(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    const int16_t  data = (int16_t)(Load(addr, 16));
    m_reg[rd]           = (int64_t)data;

#ifdef DEBUG
    printf("lh x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    const int32_t  data = (int32_t)(Load(addr, 32));
    m_reg[rd]           = (int64_t)data;

#ifdef DEBUG
    printf("lw x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LD(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    const int64_t  data = (int64_t)(Load(addr, 64));
    m_reg[rd]           = (int64_t)data;

#ifdef DEBUG
    printf("ld x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LBU(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    m_reg[rd]           = Load(addr, 8);

#ifdef DEBUG
    printf("lbu x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LHU(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    m_reg[rd]           = Load(addr, 16);

#ifdef DEBUG
    printf("lhu x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_LWU(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    m_reg[rd]           = Load(addr, 32);

#ifdef DEBUG
    printf("lwu x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SB(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::S_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    Store(addr, 8, m_reg[rs2]);

#ifdef DEBUG
    printf("sb x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_SH(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::S_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    Store(addr, 16, m_reg[rs2]);

#ifdef DEBUG
    printf("sh x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_SW(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::S_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    Store(addr, 32, m_reg[rs2]);

#ifdef DEBUG
    printf("sw x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

void CPU::Exec_SD(const uint32_t instruction) {
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::S_Type);

    const uint64_t addr = (int64_t)m_reg[rs1] + (int64_t)imm;
    Store(addr, 64, m_reg[rs2]);

#ifdef DEBUG
    printf("sw x%u, x%u, 0x%x\n", rs1, rs2, imm);
#endif
}

CPU::~CPU() {
#ifdef DEBUG
    printf("destroy a cpu\n");
#endif
}

} // namespace cpu
} // namespace rv64_emulator
