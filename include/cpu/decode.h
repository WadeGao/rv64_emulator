#pragma once

#include <cstdint>
#include <string>

namespace rv64_emulator::cpu::decode {

using BTypeDesc = struct BTypeDesc {
  uint32_t opcode : 7;
  uint32_t imm11 : 1;
  uint32_t imm4_1 : 4;
  uint32_t funct3 : 3;
  uint32_t rs1 : 5;
  uint32_t rs2 : 5;
  uint32_t imm10_5 : 6;
  int32_t imm12 : 1;
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

}  // namespace rv64_emulator::cpu::decode
