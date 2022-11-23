#ifndef RV64_EMULATOR_INCLUDE_DECODER_H_
#define RV64_EMULATOR_INCLUDE_DECODER_H_

#include "include/conf.h"

#include <cstdint>
#include <string>

namespace rv64_emulator {
namespace decoder {

constexpr uint64_t DecodeCacheEntrySize = 4096;

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

uint16_t GetCsr(const uint32_t instruction);
uint8_t  GetShamt(const uint32_t instruction, const bool is_rv32_arch = false); //

// instruction format parser
FormatR ParseFormatR(const uint32_t instruction);
FormatI ParseFormatI(const uint32_t instruction);
FormatS ParseFormatS(const uint32_t instruction);
FormatB ParseFormatB(const uint32_t instruction);
FormatU ParseFormatU(const uint32_t instruction);
FormatJ ParseFormatJ(const uint32_t instruction);

// instruction dumper
std::string DumpFormatR(const uint32_t instruction);
} // namespace decoder
} // namespace rv64_emulator

#endif