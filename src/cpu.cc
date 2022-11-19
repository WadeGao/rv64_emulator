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

uint32_t CPU::Fetch() {
    uint32_t instruction = m_bus->Load(m_pc, RV64_INSTRUCTION_BIT_SIZE);
    m_pc += 4;
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

    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;

    using RV64OpCode = rv64_emulator::decoder::RV64OpCode;
    using RV64Funct7 = rv64_emulator::decoder::RV64Funct7;
    using RV64Funct3 = rv64_emulator::decoder::RV64Funct3;

    switch (opcode) {
        case uint8_t(RV64OpCode::LUI):
            Exec_LUI(instruction);
            break;
        case uint8_t(RV64OpCode::AUIPC):
            Exec_AUIPC(instruction);
            break;
        case uint8_t(RV64OpCode::JAL):
            Exec_JAL(instruction);
            break;
        case uint8_t(RV64OpCode::JALR):
            Exec_JALR(instruction);
            break;
        case uint8_t(RV64OpCode::Branch):
            switch (funct3) {
                case uint8_t(RV64Funct3::BEQ):
                    Exec_BEQ(instruction);
                    break;
                case uint8_t(RV64Funct3::BNE):
                    Exec_BNE(instruction);
                    break;
                case uint8_t(RV64Funct3::BLT):
                    Exec_BLT(instruction);
                    break;
                case uint8_t(RV64Funct3::BGE):
                    Exec_BGE(instruction);
                    break;
                case uint8_t(RV64Funct3::BLTU):
                    Exec_BLTU(instruction);
                    break;
                case uint8_t(RV64Funct3::BGEU):
                    Exec_BGEU(instruction);
                    break;
                default:
                    IllegalInstructionHandler("unknown branch instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Load):
            switch (funct3) {
                case uint8_t(RV64Funct3::LB):
                    Exec_LB(instruction);
                    break;
                case uint8_t(RV64Funct3::LH):
                    Exec_LH(instruction);
                    break;
                case uint8_t(RV64Funct3::LW):
                    Exec_LW(instruction);
                    break;
                case uint8_t(RV64Funct3::LBU):
                    Exec_LBU(instruction);
                    break;
                case uint8_t(RV64Funct3::LHU):
                    Exec_LHU(instruction);
                    break;
                case uint8_t(RV64Funct3::LWU):
                    Exec_LWU(instruction);
                    break;
                case uint8_t(RV64Funct3::LD):
                    Exec_LD(instruction);
                    break;
                default:
                    IllegalInstructionHandler("unknown load instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Store):
            switch (funct3) {
                case uint8_t(RV64Funct3::SB):
                    Exec_SB(instruction);
                    break;
                case uint8_t(RV64Funct3::SH):
                    Exec_SH(instruction);
                    break;
                case uint8_t(RV64Funct3::SW):
                    Exec_SW(instruction);
                    break;
                case uint8_t(RV64Funct3::SD):
                    Exec_SD(instruction);
                    break;
                default:
                    IllegalInstructionHandler("unknown store instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Imm):
            switch (funct3) {
                case uint8_t(RV64Funct3::ADDI):
                    Exec_ADDI(instruction);
                    break;
                case uint8_t(RV64Funct3::SLTI):
                    Exec_SLTI(instruction);
                    break;
                case uint8_t(RV64Funct3::SLTIU):
                    Exec_SLTIU(instruction);
                    break;
                case uint8_t(RV64Funct3::XORI):
                    Exec_XORI(instruction);
                    break;
                case uint8_t(RV64Funct3::ORI):
                    Exec_ORI(instruction);
                    break;
                case uint8_t(RV64Funct3::ANDI):
                    Exec_ANDI(instruction);
                    break;
                case uint8_t(RV64Funct3::SLLI):
                    Exec_SLLI(instruction);
                    break;
                case uint8_t(RV64Funct3::SRLI_SRAI):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::SRLI):
                            Exec_SRLI(instruction);
                            break;
                        case uint8_t(RV64Funct7::SRAI):
                            Exec_SRAI(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown SRLI or SRAI instruction", instruction);
                            break;
                    }
                    break;
                default:
                    IllegalInstructionHandler("unknown Imm instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Imm32):
            switch (funct3) {
                case uint8_t(RV64Funct3::ADDIW):
                    Exec_ADDIW(instruction);
                    break;
                case uint8_t(RV64Funct3::SLLIW):
                    Exec_SLLIW(instruction);
                    break;
                case uint8_t(RV64Funct3::SRLIW_SRAIW):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::SRLIW):
                            Exec_SRLIW(instruction);
                            break;
                        case uint8_t(RV64Funct7::SRAIW):
                            Exec_SRAIW(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown SRLIW or SRAIW instruction", instruction);
                            break;
                    }
                    break;
                default:
                    IllegalInstructionHandler("unknown Imm32 instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Reg):
            switch (funct3) {
                case uint8_t(RV64Funct3::SLL):
                    Exec_SLL(instruction);
                    break;
                case uint8_t(RV64Funct3::SLT):
                    Exec_SLT(instruction);
                    break;
                case uint8_t(RV64Funct3::SLTU):
                    Exec_SLTU(instruction);
                    break;
                case uint8_t(RV64Funct3::XOR):
                    Exec_XOR(instruction);
                    break;
                case uint8_t(RV64Funct3::OR):
                    Exec_OR(instruction);
                    break;
                case uint8_t(RV64Funct3::AND):
                    Exec_AND(instruction);
                    break;
                case uint8_t(RV64Funct3::ADD_SUB):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::ADD):
                            Exec_ADD(instruction);
                            break;
                        case uint8_t(RV64Funct7::SUB):
                            Exec_SUB(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown ADD or SUB instruction", instruction);
                            break;
                    }
                    break;
                case uint8_t(RV64Funct3::SRL_SRA):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::SRL):
                            Exec_SRL(instruction);
                            break;
                        case uint8_t(RV64Funct7::SRA):
                            Exec_SRA(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown SRL or SRA instruction", instruction);
                            break;
                    }
                    break;
                default:
                    IllegalInstructionHandler("unknown Reg instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::Reg32):
            switch (funct3) {
                case uint8_t(RV64Funct3::SLLW):
                    Exec_SLLW(instruction);
                    break;
                case uint8_t(RV64Funct3::DIVW):
                    Exec_DIVW(instruction);
                    break;
                case uint8_t(RV64Funct3::REMUW):
                    Exec_REMUW(instruction);
                    break;
                case uint8_t(RV64Funct3::REMW):
                    Exec_REMW(instruction);
                    break;
                case uint8_t(RV64Funct3::ADDW_MULW_SUBW):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::ADDW):
                            Exec_ADDW(instruction);
                            break;
                        case uint8_t(RV64Funct7::SUBW):
                            Exec_SUBW(instruction);
                            break;
                        case uint8_t(RV64Funct7::MULW):
                            Exec_MULW(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown ADDW or MULW or SUBW instruction", instruction);
                            break;
                    }
                    break;
                case uint8_t(RV64Funct3::SRLW_SRAW_DIVUW):
                    switch (funct7) {
                        case uint8_t(RV64Funct7::SRLW):
                            Exec_SRLW(instruction);
                            break;
                        case uint8_t(RV64Funct7::SRAW):
                            Exec_SRAW(instruction);
                            break;
                        case uint8_t(RV64Funct7::DIVUW):
                            Exec_DIVUW(instruction);
                            break;
                        default:
                            IllegalInstructionHandler("unknown SRLW or SRAW or DIVUW instruction", instruction);
                            break;
                    }
                    break;
                default:
                    IllegalInstructionHandler("unknown DIVW or REMW instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::FENCE):
            switch (funct3) {
                case uint8_t(RV64Funct3::FENCE):
                    Exec_FENCE(instruction);
                    break;
                case uint8_t(RV64Funct3::FENCE_I):
                    Exec_FENCE_I(instruction);
                    break;
                default:
                    IllegalInstructionHandler("unknown FENCE or FENCE_I instruction", instruction);
                    break;
            }
            break;
        case uint8_t(RV64OpCode::CSR_Type):
            switch (funct3) {
                case uint8_t(RV64Funct3::ECALL_EBREAK):
                    Exec_ECALL(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRW):
                    Exec_CSRRW(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRS):
                    Exec_CSRRS(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRC):
                    Exec_CSRRC(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRWI):
                    Exec_CSRRWI(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRSI):
                    Exec_CSRRSI(instruction);
                    break;
                case uint8_t(RV64Funct3::CSRRCI):
                    Exec_CSRRCI(instruction);
                    break;
                default:
                    IllegalInstructionHandler("unknown ECALL or EBREAK instruction", instruction);
                    break;
            }
            break;
        default:
            IllegalInstructionHandler("unknown instruction", instruction);
            break;
    }
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

    m_reg[rd] = m_reg[rs1] >> shamt;

#ifdef DEBUG
    printf("srli x%u, x%u, 0x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_SRAI(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction);

    m_reg[rd] = (int64_t)m_reg[rs1] >> shamt;

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

    m_reg[rd] = (int64_t)(m_pc - 4) + (int64_t)imm;

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

void CPU::Exec_ADD(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] + (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("add x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SUB(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] - (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("sub x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SLL(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] << (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("sll x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SLT(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    m_reg[rd] = ((int64_t)m_reg[rs1] < (int64_t)m_reg[rs2]) ? 1 : 0;

#ifdef DEBUG
    printf("slt x%u, x%u, x%u\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SLTU(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    m_reg[rd] = ((uint64_t)m_reg[rs1] < (uint64_t)m_reg[rs2]) ? 1 : 0;

#ifdef DEBUG
    printf("sltu x%u, x%u, x%u\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_XOR(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] ^ (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("xor x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SRL(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)(m_reg[rs1] >> m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("srl x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SRA(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] >> (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("sra x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_OR(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] | (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("or x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_AND(const uint32_t instruction) {

    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)m_reg[rs1] & (int64_t)m_reg[rs2];

    m_reg[rd] = val;

#ifdef DEBUG
    printf("and x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_ADDIW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const int32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = (int64_t)((int32_t)m_reg[rs1] + imm);

#ifdef DEBUG
    printf("addiw x%u, x%u, x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SLLIW(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction, true);

    m_reg[rd] = (int64_t)((int32_t)m_reg[rs1] << shamt);

#ifdef DEBUG
    printf("slliw x%u, x%u, x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_SRLIW(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction, true);

    m_reg[rd] = (int64_t)(int32_t)(m_reg[rs1] >> shamt);

#ifdef DEBUG
    printf("srliw x%u, x%u, x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_SRAIW(const uint32_t instruction) {
    const uint8_t rd    = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1   = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t shamt = rv64_emulator::decoder::GetShamt(instruction, true);

    m_reg[rd] = (int64_t)((int32_t)m_reg[rs1] >> shamt);

#ifdef DEBUG
    printf("sraiw x%u, x%u, x%x\n", rd, rs1, shamt);
#endif
}

void CPU::Exec_ADDW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)((int32_t)m_reg[rs1] + (int32_t)m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("addw x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SUBW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)((int32_t)m_reg[rs1] - (int32_t)m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("subw x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SLLW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)((int32_t)m_reg[rs1] << (int32_t)m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("sllw x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SRLW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)((uint32_t)m_reg[rs1] >> (uint32_t)m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("srlw x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_SRAW(const uint32_t instruction) {
    const uint8_t rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint8_t rs2 = rv64_emulator::decoder::GetRs2(instruction);

    const int64_t val = (int64_t)((int32_t)m_reg[rs1] >> (int32_t)m_reg[rs2]);

    m_reg[rd] = val;

#ifdef DEBUG
    printf("sraw x%u, x%u, x%x\n", rd, rs1, rs2);
#endif
}

void CPU::Exec_MULW(const uint32_t instruction) {
#ifdef DEBUG
    printf("mulw not implement\n");
#endif
}

void CPU::Exec_DIVW(const uint32_t instruction) {
#ifdef DEBUG
    printf("divw not implement\n");
#endif
}

void CPU::Exec_DIVUW(const uint32_t instruction) {
#ifdef DEBUG
    printf("divuw not implement\n");
#endif
}

void CPU::Exec_REMW(const uint32_t instruction) {
#ifdef DEBUG
    printf("remw not implement\n");
#endif
}

void CPU::Exec_REMUW(const uint32_t instruction) {
#ifdef DEBUG
    printf("remuw not implement\n");
#endif
}

void CPU::Exec_FENCE(const uint32_t instruction) {
#ifdef DEBUG
    printf("fence not implement\n");
#endif
}

void CPU::Exec_FENCE_I(const uint32_t instruction) {
#ifdef DEBUG
    printf("fencei not implement\n");
#endif
}

void CPU::Exec_ECALL(const uint32_t instruction) {
#ifdef DEBUG
    printf("ecall not implement\n");
#endif
}

void CPU::Exec_EBREAK(const uint32_t instruction) {
#ifdef DEBUG
    printf("ebreak not implement\n");
#endif
}

void CPU::Exec_CSRRW(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrw not implement\n");
#endif
}

void CPU::Exec_CSRRS(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrs not implement\n");
#endif
}

void CPU::Exec_CSRRC(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrc not implement\n");
#endif
}

void CPU::Exec_CSRRWI(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrwi not implement\n");
#endif
}

void CPU::Exec_CSRRSI(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrsi not implement\n");
#endif
}

void CPU::Exec_CSRRCI(const uint32_t instruction) {
#ifdef DEBUG
    printf("csrrci not implement\n");
#endif
}

CPU::~CPU() {
#ifdef DEBUG
    printf("destroy a cpu\n");
#endif
}

#ifdef DEBUG
void CPU::Dump() const {
    // Application Binary Interface registers
    const char* abi[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
        "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };

    const uint32_t instruction = m_bus->Load(m_pc, RV64_INSTRUCTION_BIT_SIZE);
    printf("%#-13.2lx -> %#-13.2lx\n", m_pc, instruction);

    for (int i = 0; i < 8; i++) {
        printf("   %4s: %#-13.2lx  ", abi[i], m_reg[i]);
        printf("   %2s: %#-13.2lx  ", abi[i + 8], m_reg[i + 8]);
        printf("   %2s: %#-13.2lx  ", abi[i + 16], m_reg[i + 16]);
        printf("   %3s: %#-13.2lx\n", abi[i + 24], m_reg[i + 24]);
    }
}
#endif

void CPU::IllegalInstructionHandler(const char* info, const uint32_t instruction) const {
#ifdef DEBUG
    printf("[0x%x]: %s\n", instruction, info);
    Dump();
#endif
    assert(false);
}

} // namespace cpu
} // namespace rv64_emulator
