#ifndef RV64_EMULATOR_INCLUDE_DECODER_H_
#define RV64_EMULATOR_INCLUDE_DECODER_H_

#include "include/conf.h"

#include <cstdint>
#include <map>

namespace rv64_emulator {
namespace decoder {

enum class RV64InstructionFormatType : uint8_t {
    R_Type = 0,
    I_Type,
    S_Type,
    B_Type,
    U_Type,
    J_Type
};

enum class RV64OpCode : uint8_t {
    Imm      = 0b0010011,
    Imm32    = 0b0011011,
    Reg      = 0b0110011,
    Reg32    = 0b0111011,
    Load     = 0b0000011,
    Store    = 0b0100011,
    Branch   = 0b1100011,
    LUI      = 0b0110111,
    AUIPC    = 0b0010111,
    JAL      = 0b1101111,
    JALR     = 0b1100111,
    FENCE    = 0b0001111,
    CSR_Type = 0b1110011
};

enum class RV64Funct7 : uint8_t {
    SRLI = 0b0000000,
    SRAI = 0b0100000,

    SRLIW = 0b0000000,
    SRAIW = 0b0100000,

    ADD = 0b0000000,
    SUB = 0b0100000,
    SRL = 0b0000000,
    SRA = 0b0100000,

    ADDW = 0b0000000,
    MULW = 0b0000001,
    SUBW = 0b0100000,

    SRLW  = 0x00,
    DIVUW = 0b0000001,
    SRAW  = 0b0100000,
};

enum class RV64Funct3 : uint8_t {
    // Imm
    ADDI      = 0b000,
    SLLI      = 0b001,
    SLTI      = 0b010,
    SLTIU     = 0b011,
    XORI      = 0b100,
    SRLI_SRAI = 0b101,
    ORI       = 0b110,
    ANDI      = 0b111,

    // Imm32
    ADDIW       = 0b000,
    SLLIW       = 0b001,
    SRLIW_SRAIW = 0b101,

    // B type
    BEQ  = 0b000,
    BNE  = 0b001,
    BLT  = 0b100,
    BGE  = 0b101,
    BLTU = 0b110,
    BGEU = 0b111,

    // Load
    LB  = 0b000,
    LH  = 0b001,
    LW  = 0b010,
    LD  = 0b011,
    LBU = 0b100,
    LHU = 0b101,
    LWU = 0b110,

    // Store
    SB = 0b000,
    SH = 0b001,
    SW = 0b010,
    SD = 0b011,

    // Reg Type
    ADD_SUB = 0b000,
    SLL     = 0b001,
    SLT     = 0b010,
    SLTU    = 0b011,
    XOR     = 0b100,
    SRL_SRA = 0b101,
    OR      = 0b110,
    AND     = 0b111,

    // Reg32 Type
    ADDW_MULW_SUBW  = 0b000,
    DIVW            = 0b001,
    SLLW            = 0b010,
    SRLW_SRAW_DIVUW = 0b011,
    REMW            = 0b100,
    REMUW           = 0b101,

    // Fence
    FENCE   = 0b000,
    FENCE_I = 0b001,
    // CSR Type
    ECALL_EBREAK = 0b000,
    CSRRW        = 0b001,
    CSRRS        = 0b010,
    CSRRC        = 0b011,
    CSRRWI       = 0b101,
    CSRRSI       = 0b110,
    CSRRCI       = 0b111,
};

typedef struct FormatR {
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
} FormatR;

typedef struct FormatI {
    uint8_t rd;
    uint8_t rs1;
    int32_t imm;
} FormatI;

typedef struct FormatS {
    uint8_t rs1;
    uint8_t rs2;
    int32_t imm;
} FormatS;

typedef struct FormatB {
    uint8_t rs1;
    uint8_t rs2;
    int32_t imm;
} FormatB;

typedef struct FormatU {
    uint8_t rd;
    int32_t imm;
} FormatU;

typedef struct FormatJ {
    uint8_t rd;
    int32_t imm;
} FormatJ;

// decoder config
constexpr uint32_t OPCODE_MASK = 0x7f;

// masks
constexpr uint32_t REG_MASK            = 0x1f;
constexpr uint32_t SHAMT_IN_IMM64_MASK = 0x3f;
constexpr uint32_t SHAMT_IN_IMM32_MASK = 0x1f;

uint8_t GetOpCode(const uint32_t instruction);
uint8_t GetFunct3(const uint32_t instruction);
uint8_t GetFunct7(const uint32_t instruction);

uint8_t GetRd(const uint32_t instruction);
uint8_t GetRs1(const uint32_t instruction);
uint8_t GetRs2(const uint32_t instruction);

int32_t  GetImm(const uint32_t instruction, const RV64InstructionFormatType type);
uint8_t  GetShamt(const uint32_t instruction, const bool is_rv32_arch = false);
uint16_t GetCsr(const uint32_t instruction);

FormatR ParseFormatR(const uint32_t instruction);
FormatI ParseFormatI(const uint32_t instruction);
FormatS ParseFormatS(const uint32_t instruction);
FormatB ParseFormatB(const uint32_t instruction);
FormatU ParseFormatU(const uint32_t instruction);
FormatJ ParseFormatJ(const uint32_t instruction);

} // namespace decoder
} // namespace rv64_emulator

#endif