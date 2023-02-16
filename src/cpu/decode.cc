#include "cpu/decode.h"
#include "conf.h"
#include "error_code.h"

#include "fmt/core.h"

#include <cassert>
#include <cstdint>

namespace rv64_emulator::cpu::decode {

enum class RV64InstructionFormatType : uint8_t {
    kTypeR = 0,
    kTypeI,
    kTypeS,
    kTypeB,
    kTypeU,
    kTypeJ
};

// masks
static constexpr uint32_t kRegMask        = 0x1f;
static constexpr uint32_t kShamtImm64Mask = 0x3f;
static constexpr uint32_t kShamtImm32Mask = 0x1f;

static uint8_t GetRd(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 7 & kRegMask);
    return rd;
}

static uint8_t GetRs1(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 15 & kRegMask);
    return rd;
}

static uint8_t GetRs2(const uint32_t inst_word) {
    const uint8_t rd = static_cast<uint8_t>(inst_word >> 20 & kRegMask);
    return rd;
}

static int32_t GetImm(const uint32_t inst_word, const RV64InstructionFormatType type) {
    int32_t imm       = 0;
    uint8_t imm_width = 12;

    switch (type) {
        case RV64InstructionFormatType::kTypeR:
#ifdef DEBUG
            fmt::print("instruction word[{}] not R type, now abort\n", inst_word);
#endif
            exit(static_cast<int>(rv64_emulator::errorcode::DecodeErrorCode::kFieldUndefined));
            break;
        case RV64InstructionFormatType::kTypeI:
            // imm[11:0] = inst[31:20]
            imm = ((inst_word & 0xfff00000) >> 20);
            break;
        case RV64InstructionFormatType::kTypeS:
            // imm[11:5|4:0] = inst[31:25|11:7]
            imm = (((inst_word & 0xfe000000) >> 20) | ((inst_word >> 7) & 0x1f));
            break;
        case RV64InstructionFormatType::kTypeB:
            // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
            imm = (((inst_word & 0x80000000) >> 19) | ((inst_word & 0x80) << 4) | ((inst_word >> 20) & 0x7e0) | ((inst_word >> 7) & 0x1e));
            break;
        case RV64InstructionFormatType::kTypeU:
            // imm[31:12] = inst[31:12]
            imm       = (inst_word & 0xfffff000);
            imm_width = 20;
            break;
        case RV64InstructionFormatType::kTypeJ:
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

    if (is_negative && type == RV64InstructionFormatType::kTypeJ) {
        imm |= 0xfff00000;
        return imm;
    }

    return imm;
}

uint8_t GetShamt(const uint32_t inst_word, const bool kRv32Arch) {
    // rv64: shamt[5:0] = imm[5:0]
    // rv32: shamt[4:0] = imm[4:0]
    const uint32_t shamt_mask = kRv32Arch ? kShamtImm32Mask : kShamtImm64Mask;
    const uint8_t  shamt      = static_cast<uint8_t>(GetImm(inst_word, RV64InstructionFormatType::kTypeI) & shamt_mask);
    const uint8_t  max_shamt  = kRv32Arch ? 0b11111 : 0b111111;
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
        .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeI),
    };
}

FormatS ParseFormatS(const uint32_t inst_word) {
    return {
        .rs1 = GetRs1(inst_word),
        .rs2 = GetRs2(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeS),
    };
}

FormatB ParseFormatB(const uint32_t inst_word) {
    return {
        .rs1 = GetRs1(inst_word),
        .rs2 = GetRs2(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeB),
    };
}

FormatU ParseFormatU(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeU),
    };
}

FormatJ ParseFormatJ(const uint32_t inst_word) {
    return {
        .rd  = GetRd(inst_word),
        .imm = GetImm(inst_word, RV64InstructionFormatType::kTypeJ),
    };
}

FormatCsr ParseFormatCsr(const uint32_t inst_word) {
    return {
        .csr = GetCsr(inst_word),
        .rd  = GetRd(inst_word),
        .rs  = GetRs1(inst_word),
    };
}

} // namespace rv64_emulator::cpu::decode
