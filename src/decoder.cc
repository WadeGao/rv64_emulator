#include "include/conf.h"
#include "include/decode.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace rv64_emulator {
namespace cpu {
namespace decode {

enum class RV64InstructionFormatType : uint8_t {
    R_Type = 0,
    I_Type,
    S_Type,
    B_Type,
    U_Type,
    J_Type
};

// masks
static constexpr uint32_t REG_MASK            = 0x1f;
static constexpr uint32_t SHAMT_IN_IMM64_MASK = 0x3f;
static constexpr uint32_t SHAMT_IN_IMM32_MASK = 0x1f;

static uint8_t GetRd(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 7 & REG_MASK);
    return rd;
}

static uint8_t GetRs1(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 15 & REG_MASK);
    return rd;
}

static uint8_t GetRs2(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 20 & REG_MASK);
    return rd;
}

static int32_t GetImm(const uint32_t inst_word, const RV64InstructionFormatType type) {
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
            imm = ((inst_word & 0xfff00000) >> 20);
            break;
        case RV64InstructionFormatType::S_Type:
            // imm[11:5|4:0] = inst[31:25|11:7]
            imm = (((inst_word & 0xfe000000) >> 20) | ((inst_word >> 7) & 0x1f));
            break;
        case RV64InstructionFormatType::B_Type:
            // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
            imm = (((inst_word & 0x80000000) >> 19) | ((inst_word & 0x80) << 4) | ((inst_word >> 20) & 0x7e0) | ((inst_word >> 7) & 0x1e));
            break;
        case RV64InstructionFormatType::U_Type:
            // imm[31:12] = inst[31:12]
            imm       = (inst_word & 0xfffff000);
            imm_width = 20;
            break;
        case RV64InstructionFormatType::J_Type:
            // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
            imm = (((inst_word & 0x80000000) >> 11) | ((inst_word & 0xff000)) | ((inst_word >> 9) & 0x800) | ((inst_word >> 20) & 0x7fe));
            imm_width = 20;
            break;
        default:
            break;
    }

    bool is_negative = (inst_word >= 0x80000000);

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

uint8_t GetShamt(const uint32_t inst_word, const bool is_rv32_arch) {
    // rv64: shamt[5:0] = imm[5:0]
    // rv32: shamt[4:0] = imm[4:0]
    const uint32_t shamt_mask = is_rv32_arch ? SHAMT_IN_IMM32_MASK : SHAMT_IN_IMM64_MASK;
    const uint8_t  shamt      = static_cast<uint8_t>(GetImm(inst_word, RV64InstructionFormatType::I_Type) & shamt_mask);
    const uint8_t  max_shamt  = is_rv32_arch ? 0b11111 : 0b111111;
    assert(shamt <= max_shamt);
    return shamt;
}

uint16_t GetCsr(const uint32_t inst_word) {
    const uint16_t csr = static_cast<uint16_t>((inst_word & 0xfff00000) >> 20);
    return csr;
}

FormatR ParseFormatR(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .rs1 = GetRs1(inst_word),
        .rs2 = GetRs2(inst_word),
    };
}

FormatI ParseFormatI(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .rs1 = GetRs1(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::I_Type),
    };
}

FormatS ParseFormatS(const uint32_t inst_word) {
    return {
        .rs1 = GetRs1(inst_word),
        .rs2 = GetRs2(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::S_Type),
    };
}

FormatB ParseFormatB(const uint32_t inst_word) {
    return {
        .rs1 = GetRs1(inst_word),
        .rs2 = GetRs2(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::B_Type),
    };
}

FormatU ParseFormatU(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::U_Type),
    };
}

FormatJ ParseFormatJ(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::J_Type),
    };
}

FormatCsr ParseFormatCsr(const uint32_t inst_word) {
    return {
        .csr = GetCsr(inst_word),
        .rd  = GetRd(inst_word),
        .rs  = GetRs1(inst_word),
    };
}

} // namespace decode
} // namespace cpu
} // namespace rv64_emulator
