#pragma once

#include "conf.h"

#include <cstdint>
#include <string>

namespace rv64_emulator::cpu::decode {

using FormatR = struct FormatR {
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
};

using FormatI = struct FormatI {
    uint8_t rd;
    uint8_t rs1;
    int32_t imm;
};

using FormatS = struct FormatS {
    uint8_t rs1;
    uint8_t rs2;
    int32_t imm;
};

using FormatB = struct FormatB {
    uint8_t rs1;
    uint8_t rs2;
    int32_t imm;
};

typedef struct FormatU {
    uint8_t rd;
    int32_t imm;
} FormatU;

using FormatJ = struct FormatJ {
    uint8_t rd;
    int32_t imm;
};

using FormatCsr = struct FormatCsr {
    uint16_t csr;
    uint8_t  rd;
    uint8_t  rs;
};

uint16_t GetCsr(const uint32_t inst_word);
uint8_t  GetShamt(const uint32_t inst_word, const bool kRv32Arch /* = false */);

// instruction format parser
FormatR   ParseFormatR(const uint32_t inst_word);
FormatI   ParseFormatI(const uint32_t inst_word);
FormatS   ParseFormatS(const uint32_t inst_word);
FormatB   ParseFormatB(const uint32_t inst_word);
FormatU   ParseFormatU(const uint32_t inst_word);
FormatJ   ParseFormatJ(const uint32_t inst_word);
FormatCsr ParseFormatCsr(const uint32_t inst_word);

} // namespace rv64_emulator::cpu::decode
