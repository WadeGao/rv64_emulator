#include "include/decoder.h"
#include "include/conf.h"

#include <cassert>
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

uint32_t GetImm(const uint32_t instruction, const RV64InstructionFormatType type) {
    uint32_t imm = 0;
    switch (type) {
        case RV64InstructionFormatType::R_Type:
#ifdef DEBUG
            printf("Imm not exists in R Type Instruction!\n");
            assert(false);
#endif
            break;
        case RV64InstructionFormatType::I_Type:
            // imm[11:0] = inst[31:20]
            imm = static_cast<uint32_t>((instruction & 0xfff00000) >> 20);
            break;
        case RV64InstructionFormatType::S_Type:
            // imm[11:5|4:0] = inst[31:25|11:7]
            imm = static_cast<uint32_t>(((instruction & 0xfe000000) >> 20) | ((instruction >> 7) & 0x1f));
            break;
        case RV64InstructionFormatType::B_Type:
            // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
            imm = static_cast<uint32_t>(
                ((instruction & 0x80000000) >> 19) | ((instruction & 0x80) << 4) | ((instruction >> 20) & 0x7e0) |
                ((instruction >> 7) & 0x1e));
            break;
        case RV64InstructionFormatType::U_Type:
            // imm[31:12] = inst[31:12]
            imm = static_cast<uint32_t>(instruction & 0xfffff000);
            break;
        case RV64InstructionFormatType::J_Type:
            // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
            imm = static_cast<uint32_t>(
                ((instruction & 0x80000000) >> 11) | ((instruction & 0xff000)) | ((instruction >> 9) & 0x800) |
                ((instruction >> 20) & 0x7fe));
            break;
        default:
            break;
    }
    return imm;
}

uint32_t GetShamt(const uint32_t instruction) {
    // shamt[4:5] = imm[5:0]
    return GetImm(instruction, RV64InstructionFormatType::I_Type) & SHAMT_IN_IMM_MASK;
}

uint32_t GetCsr(const uint32_t instruction) {
    const uint32_t csr = static_cast<uint32_t>((instruction & 0xfff00000) >> 20);
    return csr;
}

} // namespace decoder
} // namespace rv64_emulator
