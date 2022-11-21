#include "include/decoder.h"
#include "include/conf.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace rv64_emulator {
namespace decoder {

uint8_t GetOpCode(const uint32_t instruction) {
    const uint8_t opcode = static_cast<uint8_t>(instruction & OPCODE_MASK);
    return opcode;
}

uint8_t GetFunct3(const uint32_t instruction) {
    const uint8_t funct3 = static_cast<uint8_t>((instruction >> 12) & 0x7);
    return funct3;
}

uint8_t GetFunct7(const uint32_t instruction) {
    const uint8_t funct7 = static_cast<uint8_t>((instruction >> 25) & 0x7f);
    return funct7;
}

uint8_t GetRd(const uint32_t instruction) {
    const uint8_t rd = static_cast<uint8_t>(instruction >> 7 & REG_MASK);
    return rd;
}

uint8_t GetRs1(const uint32_t instruction) {
    const uint8_t rd = static_cast<uint8_t>(instruction >> 15 & REG_MASK);
    return rd;
}

uint8_t GetRs2(const uint32_t instruction) {
    const uint8_t rd = static_cast<uint8_t>(instruction >> 20 & REG_MASK);
    return rd;
}

int32_t GetImm(const uint32_t instruction, const RV64InstructionFormatType type) {
    int32_t imm       = 0;
    uint8_t imm_width = 12;

    switch (type) {
        case RV64InstructionFormatType::R_Type:
#ifdef DEBUG
            printf("Imm not exists in R Type Instruction!\n");
            assert(false);
#endif
            break;
        case RV64InstructionFormatType::I_Type:
            // imm[11:0] = inst[31:20]
            imm = ((instruction & 0xfff00000) >> 20);
            break;
        case RV64InstructionFormatType::S_Type:
            // imm[11:5|4:0] = inst[31:25|11:7]
            imm = (((instruction & 0xfe000000) >> 20) | ((instruction >> 7) & 0x1f));
            break;
        case RV64InstructionFormatType::B_Type:
            // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
            imm =
                (((instruction & 0x80000000) >> 19) | ((instruction & 0x80) << 4) | ((instruction >> 20) & 0x7e0) |
                 ((instruction >> 7) & 0x1e));
            break;
        case RV64InstructionFormatType::U_Type:
            // imm[31:12] = inst[31:12]
            imm       = (instruction & 0xfffff000);
            imm_width = 20;
            break;
        case RV64InstructionFormatType::J_Type:
            // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
            imm =
                (((instruction & 0x80000000) >> 11) | ((instruction & 0xff000)) | ((instruction >> 9) & 0x800) |
                 ((instruction >> 20) & 0x7fe));
            imm_width = 20;
            break;
        default:
            break;
    }

    bool is_negative = (instruction >= 0x80000000);

    if (is_negative && imm_width == 12) {
        imm |= 0xfffff000;
        return imm;
    }

    if (is_negative && type == RV64InstructionFormatType::J_Type) {
        imm |= 0xfff00000;
        return imm;
    }

    return imm;
}

uint8_t GetShamt(const uint32_t instruction, const bool is_rv32_arch) {
    // rv64: shamt[5:0] = imm[5:0]
    // rv32: shamt[4:0] = imm[4:0]
    const uint32_t shamt_mask = is_rv32_arch ? SHAMT_IN_IMM32_MASK : SHAMT_IN_IMM64_MASK;
    const uint8_t  shamt      = static_cast<uint8_t>(GetImm(instruction, RV64InstructionFormatType::I_Type) & shamt_mask);
    const uint8_t  max_shamt  = is_rv32_arch ? 0b11111 : 0b111111;
    assert(shamt <= max_shamt);
    return shamt;
}

uint16_t GetCsr(const uint32_t instruction) {
    const uint16_t csr = static_cast<uint16_t>((instruction & 0xfff00000) >> 20);
    return csr;
}

FormatR ParseFormatR(const uint32_t instruction) {
    return {
        .rd  = GetRd(instruction),
        .rs1 = GetRs1(instruction),
        .rs2 = GetRs2(instruction),
    };
}

FormatI ParseFormatI(const uint32_t instruction) {
    return {
        .rd  = GetRd(instruction),
        .rs1 = GetRs1(instruction),
        .imm = GetImm(instruction, RV64InstructionFormatType::I_Type),
    };
}

FormatS ParseFormatS(const uint32_t instruction) {
    return {
        .rs1 = GetRs1(instruction),
        .rs2 = GetRs2(instruction),
        .imm = GetImm(instruction, RV64InstructionFormatType::S_Type),
    };
}

FormatB ParseFormatB(const uint32_t instruction) {
    return {
        .rs1 = GetRs1(instruction),
        .rs2 = GetRs2(instruction),
        .imm = GetImm(instruction, RV64InstructionFormatType::B_Type),
    };
}

FormatU ParseFormatU(const uint32_t instruction) {
    return {
        .rd  = GetRd(instruction),
        .imm = GetImm(instruction, RV64InstructionFormatType::U_Type),
    };
}

FormatJ ParseFormatJ(const uint32_t instruction) {
    return {
        .rd  = GetRd(instruction),
        .imm = GetImm(instruction, RV64InstructionFormatType::J_Type),
    };
}

} // namespace decoder
} // namespace rv64_emulator
