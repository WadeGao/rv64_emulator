#pragma once

#include <cstdint>

namespace rv64_emulator::cpu::decode {

enum class OpCode : uint32_t {
  kUndef = 0b0000000,
  kReg = 0b0110011,
  kImm = 0b0010011,
  kLui = 0b0110111,
  kBranch = 0b1100011,
  kStore = 0b0100011,
  kLoad = 0b0000011,
  kSystem = 0b1110011,
  kAuipc = 0b0010111,
  kJal = 0b1101111,
  kJalr = 0b1100111,
  kImm32 = 0b0011011,
  kRv32 = 0b0111011,
};

enum class InstToken {
  UNKNOWN = -1,
  LUI,
  AUIPC,
  JAL,
  JALR,
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,
  LB,
  LH,
  LW,
  LBU,
  LHU,
  SB,
  SH,
  SW,
  ADDI,
  SLTI,
  SLTIU,
  XORI,
  ORI,
  ANDI,
  ADD,
  SUB,
  SLL,
  SLT,
  SLTU,
  XOR,
  SRL,
  SRA,
  OR,
  AND,
  FENCE,
  FENCE_TSO,
  PAUSE,
  ECALL,
  EBREAK,
  LWU,
  LD,
  SD,
  SLLI,
  SRLI,
  SRAI,
  ADDIW,
  SLLIW,
  SRLIW,
  SRAIW,
  ADDW,
  SUBW,
  SLLW,
  SRLW,
  SRAW,
  MRET,
  WFI,
  CSRRW,
  CSRRS,
  CSRRC,
  CSRRWI,
  CSRRSI,
  CSRRCI,
  FENCE_I,
  MUL,
  MULH,
  MULHSU,
  MULHU,
  DIV,
  DIVU,
  REM,
  REMU,
  MULW,
  DIVW,
  DIVUW,
  REMW,
  REMUW,
  SRET,
  SFENCE_VMA,
};

using DecodeResDesc = struct DecodeResDesc {
  OpCode opcode;    // instruction opcode
  InstToken token;  // instruction token
  int32_t index;    // instruction index in decode::kInstTable
  uint32_t word;    // instruction word
  uint64_t addr;    // instruction addr
};

using InstDesc = struct InstDesc {
  uint32_t mask;
  uint32_t signature;
  const char* name;
  InstToken token;
};

constexpr InstDesc kInstTable[] = {
    {
        .mask = 0x0000007f,
        .signature = 0x00000037,
        .name = "LUI",
        .token = InstToken::LUI,
    },

    {
        .mask = 0x0000007f,
        .signature = 0x00000017,
        .name = "AUIPC",
        .token = InstToken::AUIPC,
    },

    {
        .mask = 0x0000007f,
        .signature = 0x0000006f,
        .name = "JAL",
        .token = InstToken::JAL,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000067,
        .name = "JALR",
        .token = InstToken::JALR,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000063,
        .name = "BEQ",
        .token = InstToken::BEQ,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001063,
        .name = "BNE",
        .token = InstToken::BNE,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004063,
        .name = "BLT",
        .token = InstToken::BLT,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005063,
        .name = "BGE",
        .token = InstToken::BGE,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006063,
        .name = "BLTU",
        .token = InstToken::BLTU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007063,
        .name = "BGEU",
        .token = InstToken::BGEU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000003,
        .name = "LB",
        .token = InstToken::LB,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001003,
        .name = "LH",
        .token = InstToken::LH,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002003,
        .name = "LW",
        .token = InstToken::LW,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004003,
        .name = "LBU",
        .token = InstToken::LBU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005003,
        .name = "LHU",
        .token = InstToken::LHU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000023,
        .name = "SB",
        .token = InstToken::SB,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001023,
        .name = "SH",
        .token = InstToken::SH,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002023,
        .name = "SW",
        .token = InstToken::SW,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000013,
        .name = "ADDI",
        .token = InstToken::ADDI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002013,
        .name = "SLTI",
        .token = InstToken::SLTI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003013,
        .name = "SLTIU",
        .token = InstToken::SLTIU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004013,
        .name = "XORI",
        .token = InstToken::XORI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006013,
        .name = "ORI",
        .token = InstToken::ORI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007013,
        .name = "ANDI",
        .token = InstToken::ANDI,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00000033,
        .name = "ADD",
        .token = InstToken::ADD,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x40000033,
        .name = "SUB",
        .token = InstToken::SUB,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00001033,
        .name = "SLL",
        .token = InstToken::SLL,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00002033,
        .name = "SLT",
        .token = InstToken::SLT,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00003033,
        .name = "SLTU",
        .token = InstToken::SLTU,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00004033,
        .name = "XOR",
        .token = InstToken::XOR,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00005033,
        .name = "SRL",
        .token = InstToken::SRL,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x40005033,
        .name = "SRA",
        .token = InstToken::SRA,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00006033,
        .name = "OR",
        .token = InstToken::OR,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00007033,
        .name = "AND",
        .token = InstToken::AND,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x0000000f,
        .name = "FENCE",
        .token = InstToken::FENCE,
    },

    {
        .mask = 0xfff0707f,
        .signature = 0x8330000f,
        .name = "FENCE.TSO",
        .token = InstToken::FENCE_TSO,
    },

    {
        .mask = 0xffffffff,
        .signature = 0x100000f,
        .name = "PAUSE",
        .token = InstToken::PAUSE,
    },

    {
        .mask = 0xffffffff,
        .signature = 0x00000073,
        .name = "ECALL",
        .token = InstToken::ECALL,
    },

    {
        .mask = 0xffffffff,
        .signature = 0x00100073,
        .name = "EBREAK",
        .token = InstToken::EBREAK,
    },

    /*********** rv64_i instructions ***********/
    {
        .mask = 0x0000707f,
        .signature = 0x00006003,
        .name = "LWU",
        .token = InstToken::LWU,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003003,
        .name = "LD",
        .token = InstToken::LD,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003023,
        .name = "SD",
        .token = InstToken::SD,
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x00001013,
        .name = "SLLI",
        .token = InstToken::SLLI,
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x00005013,
        .name = "SRLI",
        .token = InstToken::SRLI,
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x40005013,
        .name = "SRAI",
        .token = InstToken::SRAI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x0000001b,
        .name = "ADDIW",
        .token = InstToken::ADDIW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000101b,
        .name = "SLLIW",
        .token = InstToken::SLLIW,
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x0000501b,
        .name = "SRLIW",
        .token = InstToken::SRLIW,
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x4000501b,
        .name = "SRAIW",
        .token = InstToken::SRAIW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000003b,
        .name = "ADDW",
        .token = InstToken::ADDW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x4000003b,
        .name = "SUBW",
        .token = InstToken::SUBW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000103b,
        .name = "SLLW",
        .token = InstToken::SLLW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000503b,
        .name = "SRLW",
        .token = InstToken::SRLW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x4000503b,
        .name = "SRAW",
        .token = InstToken::SRAW,
    },

    /*********** rv_system instructions ***********/

    {
        .mask = 0xffffffff,
        .signature = 0x30200073,
        .name = "MRET",
        .token = InstToken::MRET,
    },

    {
        .mask = 0xffffffff,
        .signature = 0x10500073,
        .name = "WFI",
        .token = InstToken::WFI,
    },

    /*********** rv_zicsr instructions ***********/

    {
        .mask = 0x0000707f,
        .signature = 0x00001073,
        .name = "CSRRW",
        .token = InstToken::CSRRW,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002073,
        .name = "CSRRS",
        .token = InstToken::CSRRS,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003073,
        .name = "CSRRC",
        .token = InstToken::CSRRC,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005073,
        .name = "CSRRWI",
        .token = InstToken::CSRRWI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006073,
        .name = "CSRRSI",
        .token = InstToken::CSRRSI,
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007073,
        .name = "CSRRCI",
        .token = InstToken::CSRRCI,
    },

    /*********** rv_zifencei ***********/

    {
        .mask = 0x0000707f,
        .signature = 0x0000100f,
        .name = "FENCE.I",
        .token = InstToken::FENCE_I,
    },

    /*********** rv_m ***********/
    {
        .mask = 0xfe00707f,
        .signature = 0x2000033,
        .name = "MUL",
        .token = InstToken::MUL,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2001033,
        .name = "MULH",
        .token = InstToken::MULH,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2002033,
        .name = "MULHSU",
        .token = InstToken::MULHSU,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2003033,
        .name = "MULHU",
        .token = InstToken::MULHU,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2004033,
        .name = "DIV",
        .token = InstToken::DIV,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2005033,
        .name = "DIVU",
        .token = InstToken::DIVU,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2006033,
        .name = "REM",
        .token = InstToken::REM,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2007033,
        .name = "REMU",
        .token = InstToken::REMU,
    },

    /*********** rv64_m ***********/
    {
        .mask = 0xfe00707f,
        .signature = 0x200003b,
        .name = "MULW",
        .token = InstToken::MULW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200403b,
        .name = "DIVW",
        .token = InstToken::DIVW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200503b,
        .name = "DIVUW",
        .token = InstToken::DIVUW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200603b,
        .name = "REMW",
        .token = InstToken::REMW,
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200703b,
        .name = "REMUW",
        .token = InstToken::REMUW,
    },

    /*********** rv_s instructions ***********/
    {
        .mask = 0xffffffff,
        .signature = 0x10200073,
        .name = "SRET",
        .token = InstToken::SRET,
    },

    {
        .mask = 0xfe007fff,
        .signature = 0x12000073,
        .name = "SFENCE.VMA",
        .token = InstToken::SFENCE_VMA,
    },
};

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
