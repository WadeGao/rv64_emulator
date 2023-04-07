#pragma once

#include <cstdint>
#include <string>

#include "conf.h"

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

using FormatJ = struct FormatJ {
  uint8_t rd;
  int32_t imm;
};

using CsrTypeDesc = struct {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  uint32_t imm : 12;
};

using UTypeDesc = struct {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  int32_t imm_31_12 : 20;
};

uint8_t GetShamt(const uint32_t inst_word, const bool kRv32Arch /* = false */);

// instruction format parser
FormatR ParseFormatR(const uint32_t inst_word);
FormatI ParseFormatI(const uint32_t inst_word);
FormatS ParseFormatS(const uint32_t inst_word);
FormatB ParseFormatB(const uint32_t inst_word);
FormatJ ParseFormatJ(const uint32_t inst_word);

}  // namespace rv64_emulator::cpu::decode
