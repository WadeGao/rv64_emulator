#pragma once

#include <cstdint>
#include <string>

#include "conf.h"

namespace rv64_emulator::cpu::decode {

using FormatB = struct FormatB {
  uint8_t rs1;
  uint8_t rs2;
  int32_t imm;
};

using JTypeDesc = struct JTypeDesc {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t imm19_12 : 8;
  uint32_t imm11 : 1;
  uint32_t imm10_1 : 10;
  int32_t imm20 : 1;
};

using STypeDesc = struct STypeDesc {
  uint32_t opcode : 7;
  uint32_t imm4_0 : 5;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  uint32_t rs2 : 5;
  int32_t imm11_5 : 7;
};

using CsrTypeDesc = struct CsrTypeDesc {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  uint32_t imm : 12;
};

using UTypeDesc = struct UTypeDesc {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  int32_t imm_31_12 : 20;
};

using RTypeDesc = struct RTypeDesc {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  uint32_t rs2 : 5;
  int32_t funct7 : 7;
};

using ITypeDesc = struct ITypeDesc {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  int32_t imm : 12;
};

uint8_t GetShamt(const ITypeDesc desc, const bool kRv32Arch /* = false */);

// instruction format parser
FormatB ParseFormatB(const uint32_t inst_word);

}  // namespace rv64_emulator::cpu::decode
