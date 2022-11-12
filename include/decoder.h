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
    J_Type,
    CSR_Type
};

enum class RV64OpCode : uint8_t {
    Imm             = 0b0010011,
    ArithmeticLogic = 0b0110011,
    Load            = 0b0000011,
    Store           = 0b0100011,
    Branch          = 0b1100011,
    LUI             = 0b0110111,
    AUIPC           = 0b0010111,
    JAL             = 0b1101111,
    JALR            = 0b1100111,
    FENCE           = 0b0001111,
    CSR_Type        = 0b1110011 // also ecall, ebreak
};

enum class RV64Funct3 : uint8_t {
    ADDI  = 0b000,
    SLLI  = 0b001,
    SLTI  = 0b010,
    SLTIU = 0b011,
    XORI  = 0b100,
    SR_I  = 0b101,
    // SRLI 0x00
    // SRAI 0x20
    ORI  = 0b110,
    ANDI = 0b111
};

// decoder config
constexpr uint32_t OPCODE_MASK = 0x7f;

// masks
constexpr uint32_t REG_MASK          = 0x1f;
constexpr uint32_t SHAMT_IN_IMM_MASK = 0x1f;

uint8_t GetOpCode(const uint32_t instruction);
uint8_t GetFunct3(const uint32_t instruction);
uint8_t GetFunct7(const uint32_t instruction);

uint8_t GetRd(const uint32_t instruction);
uint8_t GetRs1(const uint32_t instruction);
uint8_t GetRs2(const uint32_t instruction);

uint32_t GetImm(const uint32_t instruction, const RV64InstructionFormatType type);
uint32_t GetShamt(const uint32_t instruction);
uint32_t GetCsr(const uint32_t instruction);

} // namespace decoder
} // namespace rv64_emulator

#endif