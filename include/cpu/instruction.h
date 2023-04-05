#pragma once

#include <cstdint>
#include <cstdlib>

#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "error_code.h"
#include "fmt/core.h"
#include "libs/arithmetic.hpp"

static bool CheckPcAlign(const uint64_t pc, const uint64_t isa) {
  using rv64_emulator::cpu::csr::MisaDesc;
  const auto* kMisaDesc = reinterpret_cast<const MisaDesc*>(&isa);
  const uint64_t kAlignBytes = kMisaDesc->C ? 2 : 4;
  return (pc & (kAlignBytes - 1)) == 0;
}

#define CHECK_MISALIGN_INSTRUCTION(pc, proc)                        \
  const uint64_t kMisaVal = (proc)->state_.Read(csr::kCsrMisa);     \
  const bool kNewPcAlign = CheckPcAlign((pc), kMisaVal);            \
  if (!kNewPcAlign) {                                               \
    return {                                                        \
        .trap_type = trap::TrapType::kInstructionAddressMisaligned, \
        .trap_val = (pc),                                           \
    };                                                              \
  }

#define CHECK_CSR_ACCESS_PRIVILEGE(csr_num, write, proc)  \
  bool is_privileged = false;                             \
  if (cpu::PrivilegeMode(((csr_num) >> 8) & 0b11) <=      \
      (proc)->GetPrivilegeMode()) {                       \
    is_privileged = true;                                 \
  }                                                       \
  const bool kReadOnly = ((csr_num) >> 10) == 0b11;       \
  if (!is_privileged || ((write) && kReadOnly)) {         \
    return {                                              \
        .trap_type = trap::TrapType::kIllegalInstruction, \
        .trap_val = (proc)->GetPC() - 4,                  \
    };                                                    \
  }

#define LOAD_VIRTUAL_MEMORY(type, data)                            \
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&(data));              \
  const trap::Trap kLoadTrap = cpu->Load(addr, sizeof(type), ptr); \
  if (kLoadTrap.trap_type != trap::TrapType::kNone) {              \
    return kLoadTrap;                                              \
  }

#define STORE_VIRTUAL_MEMORY(type, data)                              \
  const uint8_t* kPtr = reinterpret_cast<const uint8_t*>(&(data));    \
  const trap::Trap kStoreTrap = cpu->Store(addr, sizeof(type), kPtr); \
  if (kStoreTrap.trap_type != trap::TrapType::kNone) {                \
    return kStoreTrap;                                                \
  }

namespace rv64_emulator::cpu {

class CPU;

namespace instruction {

using Instruction = struct Instruction {
  uint32_t m_mask;
  uint32_t m_data;
  const char* m_name;
  trap::Trap (*Exec)(CPU* cpu, const uint32_t inst_word);
};

const Instruction kInstructionTable[] = {
    /*********** rv_i instructions ***********/
    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000037,
        .m_name = "LUI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatU(inst_word);

          const uint64_t val = (int64_t)f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000017,
        .m_name = "AUIPC",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatU(inst_word);
          const int64_t val = (int64_t)(cpu->GetPC() - 4) + (int64_t)f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x0000006f,
        .m_name = "JAL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatJ(inst_word);

          const uint64_t kOriginPc = cpu->GetPC();
          const uint64_t kNewPc = (int64_t)cpu->GetPC() + (int64_t)f.imm - 4;

          CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);

          cpu->SetGeneralPurposeRegVal(f.rd, kOriginPc);
          cpu->SetPC(kNewPc);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000067,
        .m_name = "JALR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t kOriginPc = cpu->GetPC();
          const uint64_t kNewPc =
              ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm) &
              0xfffffffffffffffe;

          CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);

          cpu->SetPC(kNewPc);
          cpu->SetGeneralPurposeRegVal(f.rd, kOriginPc);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000063,
        .m_name = "BEQ",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) ==
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001063,
        .m_name = "BNE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) !=
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004063,
        .m_name = "BLT",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) <
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005063,
        .m_name = "BGE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >=
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006063,
        .m_name = "BLTU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) <
              (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007063,
        .m_name = "BGEU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >=
              (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000003,
        .m_name = "LB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const int8_t   data = (int8_t)(cpu->Load(addr, sizeof(int8_t)));

          int8_t data = 0;
          LOAD_VIRTUAL_MEMORY(int8_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001003,
        .m_name = "LH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const int16_t  data = (int16_t)cpu->Load(addr, sizeof(int16_t));

          int16_t data = 0;
          LOAD_VIRTUAL_MEMORY(int16_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002003,
        .m_name = "LW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const int32_t  data = (int32_t)cpu->Load(addr, sizeof(int32_t));

          int32_t data = 0;
          LOAD_VIRTUAL_MEMORY(int32_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004003,
        .m_name = "LBU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const uint64_t data = cpu->Load(addr, sizeof(uint8_t));

          uint8_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint8_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005003,
        .m_name = "LHU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const uint64_t data = cpu->Load(addr, sizeof(uint16_t));

          uint16_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint16_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000023,
        .m_name = "SB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;

          const uint64_t kRegVal = cpu->GetGeneralPurposeRegVal(f.rs2);
          STORE_VIRTUAL_MEMORY(int8_t, kRegVal);

          // cpu->Store(addr, sizeof(int8_t),
          // cpu->GetGeneralPurposeRegVal(f.rs2));
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001023,
        .m_name = "SH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;

          const uint64_t kRegVal = cpu->GetGeneralPurposeRegVal(f.rs2);
          STORE_VIRTUAL_MEMORY(int16_t, kRegVal);

          // cpu->Store(addr, sizeof(int16_t),
          // cpu->GetGeneralPurposeRegVal(f.rs2));
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002023,
        .m_name = "SW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;

          const uint64_t kRegVal = cpu->GetGeneralPurposeRegVal(f.rs2);
          STORE_VIRTUAL_MEMORY(int32_t, kRegVal);

          // cpu->Store(addr, sizeof(int32_t),
          // cpu->GetGeneralPurposeRegVal(f.rs2));
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000013,
        .m_name = "ADDI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002013,
        .m_name = "SLTI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)f.imm ? 1
                                                                            : 0;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003013,
        .m_name = "SLTIU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)f.imm
                  ? 1
                  : 0;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004013,
        .m_name = "XORI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = cpu->GetGeneralPurposeRegVal(f.rs1) ^ f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006013,
        .m_name = "ORI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007013,
        .m_name = "ANDI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)f.imm;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00000033,
        .m_name = "ADD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) +
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40000033,
        .m_name = "SUB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) -
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00001033,
        .m_name = "SLL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1)
                              << (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00002033,
        .m_name = "SLT",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) <
                      (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)
                  ? 1
                  : 0;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00003033,
        .m_name = "SLTU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) <
                      (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)
                  ? 1
                  : 0;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00004033,
        .m_name = "XOR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) ^
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00005033,
        .m_name = "SRL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const uint64_t rs1 = cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint64_t rs2 = cpu->GetGeneralPurposeRegVal(f.rs2);
          const int64_t val = rs1 >> rs2;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40005033,
        .m_name = "SRA",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >>
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00006033,
        .m_name = "OR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) |
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00007033,
        .m_name = "AND",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) &
                              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000000f,
        .m_name = "FENCE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfff0707f,
        .m_data = 0x8330000f,
        .m_name = "FENCE.TSO",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x100000f,
        .m_name = "PAUSE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00000073,
        .m_name = "ECALL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const PrivilegeMode kPrivMode = cpu->GetPrivilegeMode();
          trap::TrapType exception_type = trap::TrapType::kNone;
          switch (kPrivMode) {
            case PrivilegeMode::kUser:
              exception_type = trap::TrapType::kEnvironmentCallFromUMode;
              break;
            case PrivilegeMode::kSupervisor:
              exception_type = trap::TrapType::kEnvironmentCallFromSMode;
              break;
            case PrivilegeMode::kMachine:
              exception_type = trap::TrapType::kEnvironmentCallFromMMode;
              break;
            case PrivilegeMode::kReserved:
            default:
#ifdef DEBUG
              fmt::print("ECALL unknown privilege mode[{}], now abort\n",
                         static_cast<int>(kPrivMode));
#endif
              exit(static_cast<int>(
                  rv64_emulator::errorcode::CpuErrorCode::kExecuteFailure));
              break;
          }
          return {
              .trap_type = exception_type,
              .trap_val = cpu->GetPC(),
          };
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00100073,
        .m_name = "EBREAK",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          return {
              .trap_type = trap::TrapType::kBreakpoint,
              .trap_val = cpu->GetPC(),
          };
        },
    },

    /*********** rv64_i instructions ***********/
    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006003,
        .m_name = "LWU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const uint64_t data = cpu->Load(addr, sizeof(uint32_t));

          uint32_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint32_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003003,
        .m_name = "LD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
          // const int64_t  data = (int64_t)cpu->Load(addr, sizeof(int64_t));

          int64_t data = 0;
          LOAD_VIRTUAL_MEMORY(int64_t, data);
          cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003023,
        .m_name = "SD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;

          const uint64_t kRegVal = cpu->GetGeneralPurposeRegVal(f.rs2);
          STORE_VIRTUAL_MEMORY(int64_t, kRegVal);

          // cpu->Store(addr, sizeof(int64_t),
          // cpu->GetGeneralPurposeRegVal(f.rs2));
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00001013,
        .m_name = "SLLI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1)
                              << (int64_t)shamt;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00005013,
        .m_name = "SRLI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val = cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x40005013,
        .m_name = "SRAI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000001b,
        .m_name = "ADDIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + f.imm);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000101b,
        .m_name = "SLLIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << shamt);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x0000501b,
        .m_name = "SRLIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint32_t rs1 = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val = (int64_t)(int32_t)(rs1 >> shamt);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x4000501b,
        .m_name = "SRAIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000003b,
        .m_name = "ADDW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) +
                        (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000003b,
        .m_name = "SUBW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) -
                        (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000103b,
        .m_name = "SLLW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1)
                        << (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000503b,
        .m_name = "SRLW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const uint32_t rs1 = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint32_t rs2 = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          const int64_t val = (int64_t)(int32_t)(rs1 >> rs2);
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000503b,
        .m_name = "SRAW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t val =
              (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >>
                        (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_system instructions ***********/

    {
        .m_mask = 0xffffffff,
        .m_data = 0x30200073,
        .m_name = "MRET",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const uint64_t kOriginMstatusVal = cpu->state_.Read(csr::kCsrMstatus);
          const csr::MstatusDesc kOriginMstatusDesc =
              *reinterpret_cast<const csr::MstatusDesc*>(&kOriginMstatusVal);

          if (cpu->GetPrivilegeMode() < PrivilegeMode::kMachine) {
            return {
                .trap_type = trap::TrapType::kIllegalInstruction,
                .trap_val = cpu->GetPC() - 4,
            };
          }
          csr::MstatusDesc new_mstatus_desc = kOriginMstatusDesc;

          new_mstatus_desc.mpp = 0;
          new_mstatus_desc.mpie = 1;
          new_mstatus_desc.mie = kOriginMstatusDesc.mpie;
          new_mstatus_desc.mprv =
              (static_cast<PrivilegeMode>(kOriginMstatusDesc.mpp) <
                       PrivilegeMode::kMachine
                   ? 0
                   : kOriginMstatusDesc.mprv);

          const uint64_t kNewMstatusVal =
              *reinterpret_cast<const uint64_t*>(&new_mstatus_desc);
          cpu->state_.Write(csr::kCsrMstatus, kNewMstatusVal);
          cpu->SetPrivilegeMode(
              static_cast<PrivilegeMode>(kOriginMstatusDesc.mpp));

          const uint64_t kNewPc = cpu->state_.Read(csr::kCsrMepc);
          cpu->SetPC(kNewPc);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x10500073,
        .m_name = "WFI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          cpu->SetWfi(true);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_zicsr instructions ***********/

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001073,
        .m_name = "CSRRW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, true, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);

          const uint64_t new_csr_val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs);
          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
          cpu->state_.Write(f.csr, new_csr_val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002073,
        .m_name = "CSRRS",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, f.rs != 0, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);

          const uint64_t new_csr_val =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs) | (int64_t)csr_val;
          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

          cpu->state_.Write(f.csr, new_csr_val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003073,
        .m_name = "CSRRC",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, f.rs != 0, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);
          const uint64_t new_csr_val =
              (int64_t)csr_val &
              (~((int64_t)cpu->GetGeneralPurposeRegVal(f.rs)));
          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
          cpu->state_.Write(f.csr, new_csr_val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005073,
        .m_name = "CSRRWI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, true, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);

          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

          cpu->state_.Write(f.csr, f.rs);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006073,
        .m_name = "CSRRSI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, f.rs != 0, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);

          const uint64_t new_csr_val = (int64_t)csr_val | (int64_t)f.rs;
          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

          cpu->state_.Write(f.csr, new_csr_val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007073,
        .m_name = "CSRRCI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatCsr(inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(f.csr, f.rs != 0, cpu);

          const uint64_t csr_val = cpu->state_.Read(f.csr);

          const uint64_t new_csr_val = (int64_t)csr_val & (~((int64_t)f.rs));
          cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
          cpu->state_.Write(f.csr, new_csr_val);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_zifencei ***********/

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000100f,
        .m_name = "FENCE.I",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    /*********** rv_m ***********/
    // https://tclin914.github.io/f37f836/
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2000033,
        .m_name = "MUL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int64_t b = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          const int64_t val = a * b;

          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2001033,
        .m_name = "MULH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int64_t b = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          const bool kNegativeRes = (a < 0) ^ (b < 0);
          const uint64_t abs_a = static_cast<uint64_t>(a < 0 ? -a : a);
          const uint64_t abs_b = static_cast<uint64_t>(b < 0 ? -b : b);
          const uint64_t res =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(abs_a, abs_b);

          // use ~res directly because of UINT64^$_MAX^2 =
          // 0xfffffffffffffffe0000000000000001
          const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2002033,
        .m_name = "MULHSU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

          const bool kNegativeRes = a < 0;
          const uint64_t abs_a = static_cast<uint64_t>(a < 0 ? -a : a);
          const uint64_t res =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(abs_a, b);

          const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2003033,
        .m_name = "MULHU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const uint64_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);
          const uint64_t val =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(a, b);

          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2004033,
        .m_name = "DIV",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int64_t b = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          if (b == 0) {
            cpu->SetGeneralPurposeRegVal(f.rd, UINT64_MAX);
          } else if (a == INT64_MIN && b == -1) {
            cpu->SetGeneralPurposeRegVal(f.rd, INT64_MIN);
          } else {
            const int64_t val = a / b;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2005033,
        .m_name = "DIVU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const uint64_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

          const uint64_t val = b == 0 ? UINT64_MAX : a / b;
          cpu->SetGeneralPurposeRegVal(f.rd, val);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2006033,
        .m_name = "REM",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int64_t b = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          if (b == 0) {
            cpu->SetGeneralPurposeRegVal(f.rd, a);
          } else if (a == INT64_MIN && b == -1) {
            cpu->SetGeneralPurposeRegVal(f.rd, 0);
          } else {
            const int64_t val = a % b;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2007033,
        .m_name = "REMU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const uint64_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

          const uint64_t val = b == 0 ? a : a % b;

          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    /*********** rv64_m ***********/
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200003b,
        .m_name = "MULW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          const int64_t val = a * b;
          cpu->SetGeneralPurposeRegVal(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200403b,
        .m_name = "DIVW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          if (b == 0) {
            cpu->SetGeneralPurposeRegVal(f.rd, UINT64_MAX);
          } else if (a == INT32_MIN && b == -1) {
            cpu->SetGeneralPurposeRegVal(f.rd, INT32_MIN);
          } else {
            const int64_t val = (int64_t)(a / b);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200503b,
        .m_name = "DIVUW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const uint32_t a = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint32_t b = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          if (b == 0) {
            cpu->SetGeneralPurposeRegVal(f.rd, UINT64_MAX);
          } else {
            const int64_t val = (int32_t)(a / b);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200603b,
        .m_name = "REMW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

          if (b == 0) {
            const int64_t val = a;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          } else if (a == INT32_MIN && b == -1) {
            cpu->SetGeneralPurposeRegVal(f.rd, 0);
          } else {
            const int64_t val = (int64_t)((int32_t)(a % b));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200703b,
        .m_name = "REMUW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto& f = decode::ParseFormatR(inst_word);
          const uint32_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
          const uint32_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

          const int64_t val = (int32_t)(b == 0 ? a : a % b);
          cpu->SetGeneralPurposeRegVal(f.rd, val);

          return trap::kNoneTrap;
        },
    },

    /*********** rv_s instructions ***********/
    {
        .m_mask = 0xffffffff,
        .m_data = 0x10200073,
        .m_name = "SRET",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const uint64_t kOriginSstatusVal = cpu->state_.Read(csr::kCsrSstatus);
          const csr::MstatusDesc* kOriginSsDesc =
              reinterpret_cast<const csr::MstatusDesc*>(&kOriginSstatusVal);

          // 当TSR=1时，尝试在s模式下执行SRET将引发非法的指令异常
          if (cpu->GetPrivilegeMode() < PrivilegeMode::kSupervisor ||
              kOriginSsDesc->tsr) {
            return {
                .trap_type = trap::TrapType::kIllegalInstruction,
                .trap_val = cpu->GetPC() - 4,
            };
          }

          csr::MstatusDesc new_status_desc = *kOriginSsDesc;

          new_status_desc.spp = 0;
          new_status_desc.spie = 1;
          new_status_desc.sie = kOriginSsDesc->spie;
          new_status_desc.mprv =
              (static_cast<PrivilegeMode>(kOriginSsDesc->spp) <
               PrivilegeMode::kMachine)
                  ? 0
                  : kOriginSsDesc->mprv;

          const uint64_t kNewSstatusVal =
              *reinterpret_cast<const uint64_t*>(&new_status_desc);
          cpu->state_.Write(csr::kCsrSstatus, kNewSstatusVal);
          cpu->SetPrivilegeMode(static_cast<PrivilegeMode>(kOriginSsDesc->spp));

          const uint64_t kNewPc = cpu->state_.Read(csr::kCsrSepc);
          cpu->SetPC(kNewPc);

          return trap::kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe007fff,
        .m_data = 0x12000073,
        .m_name = "SFENCE.VMA",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatR(inst_word);
          const int64_t kVirtAddr =
              (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
          const int64_t kAsid = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
          cpu->FlushTlb(kVirtAddr, kAsid & 0xffff);
          return trap::kNoneTrap;
        },
    },
};

}  // namespace instruction
}  // namespace rv64_emulator::cpu
