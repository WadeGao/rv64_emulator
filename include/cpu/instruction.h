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
#include "libs/utils.h"

#define CHECK_MISALIGN_INSTRUCTION(pc, proc)                    \
  using rv64_emulator::libs::util::CheckPcAlign;                \
  const uint64_t kMisaVal = (proc)->state_.Read(csr::kCsrMisa); \
  const bool kNewPcAlign = CheckPcAlign((pc), kMisaVal);        \
  if (!kNewPcAlign) {                                           \
    return {                                                    \
        .type = trap::TrapType::kInstructionAddressMisaligned,  \
        .val = (pc),                                            \
    };                                                          \
  }

#define CHECK_CSR_ACCESS_PRIVILEGE(csr_num, write, proc) \
  bool is_privileged = false;                            \
  if (cpu::PrivilegeMode(((csr_num) >> 8) & 0b11) <=     \
      (proc)->GetPrivilegeMode()) {                      \
    is_privileged = true;                                \
  }                                                      \
  const bool kReadOnly = ((csr_num) >> 10) == 0b11;      \
  if (!is_privileged || ((write) && kReadOnly)) {        \
    return {                                             \
        .type = trap::TrapType::kIllegalInstruction,     \
        .val = (proc)->GetPC() - 4,                      \
    };                                                   \
  }

#define LOAD_VIRTUAL_MEMORY(T, data)                            \
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&(data));           \
  const trap::Trap kLoadTrap = cpu->Load(addr, sizeof(T), ptr); \
  if (kLoadTrap.type != trap::TrapType::kNone) {                \
    return kLoadTrap;                                           \
  }

#define STORE_VIRTUAL_MEMORY(T, data)                              \
  const uint8_t* kPtr = reinterpret_cast<const uint8_t*>(&(data)); \
  const trap::Trap kStoreTrap = cpu->Store(addr, sizeof(T), kPtr); \
  if (kStoreTrap.type != trap::TrapType::kNone) {                  \
    return kStoreTrap;                                             \
  }

namespace rv64_emulator::cpu {

class CPU;

namespace instruction {

using Instruction = struct Instruction {
  uint32_t mask;
  uint32_t signature;
  const char* name;
  trap::Trap (*Exec)(CPU* cpu, const uint32_t inst_word);
};

const Instruction kInstructionTable[] = {
    /*********** rv_i instructions ***********/
    {
        .mask = 0x0000007f,
        .signature = 0x00000037,
        .name = "LUI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::UTypeDesc*>(&inst_word);

          const uint64_t val = (int64_t)(kDesc.imm_31_12 << 12);
          cpu->SetReg(kDesc.rd, val);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000007f,
        .signature = 0x00000017,
        .name = "AUIPC",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::UTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)(cpu->GetPC() - 4) + (int64_t)(kDesc.imm_31_12 << 12);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000007f,
        .signature = 0x0000006f,
        .name = "JAL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatJ(inst_word);

          const uint64_t kOriginPc = cpu->GetPC();
          const uint64_t kNewPc = (int64_t)cpu->GetPC() + (int64_t)f.imm - 4;

          CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);

          cpu->SetReg(f.rd, kOriginPc);
          cpu->SetPC(kNewPc);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000067,
        .name = "JALR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t kOriginPc = cpu->GetPC();
          const uint64_t kNewPc =
              ((int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm) &
              0xfffffffffffffffe;

          CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);

          cpu->SetPC(kNewPc);
          cpu->SetReg(f.rd, kOriginPc);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000063,
        .name = "BEQ",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetReg(f.rs1) == (int64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001063,
        .name = "BNE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetReg(f.rs1) != (int64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004063,
        .name = "BLT",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetReg(f.rs1) < (int64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005063,
        .name = "BGE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((int64_t)cpu->GetReg(f.rs1) >= (int64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006063,
        .name = "BLTU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((uint64_t)cpu->GetReg(f.rs1) < (uint64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007063,
        .name = "BGEU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatB(inst_word);
          if ((uint64_t)cpu->GetReg(f.rs1) >= (uint64_t)cpu->GetReg(f.rs2)) {
            const uint64_t kNewPc = cpu->GetPC() + ((int64_t)f.imm - 4);
            CHECK_MISALIGN_INSTRUCTION(kNewPc, cpu);
            cpu->SetPC(kNewPc);
          }
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000003,
        .name = "LB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          // const int8_t   data = (int8_t)(cpu->Load(addr, sizeof(int8_t)));

          int8_t data = 0;
          LOAD_VIRTUAL_MEMORY(int8_t, data);
          cpu->SetReg(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001003,
        .name = "LH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          // const int16_t  data = (int16_t)cpu->Load(addr, sizeof(int16_t));

          int16_t data = 0;
          LOAD_VIRTUAL_MEMORY(int16_t, data);
          cpu->SetReg(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002003,
        .name = "LW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          // const int32_t  data = (int32_t)cpu->Load(addr, sizeof(int32_t));

          int32_t data = 0;
          LOAD_VIRTUAL_MEMORY(int32_t, data);
          cpu->SetReg(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004003,
        .name = "LBU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          // const uint64_t data = cpu->Load(addr, sizeof(uint8_t));

          uint8_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint8_t, data);
          cpu->SetReg(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005003,
        .name = "LHU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          // const uint64_t data = cpu->Load(addr, sizeof(uint16_t));

          uint16_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint16_t, data);
          cpu->SetReg(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000023,
        .name = "SB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          const uint64_t kRegVal = cpu->GetReg(f.rs2);
          STORE_VIRTUAL_MEMORY(int8_t, kRegVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00001023,
        .name = "SH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          const uint64_t kRegVal = cpu->GetReg(f.rs2);
          STORE_VIRTUAL_MEMORY(int16_t, kRegVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002023,
        .name = "SW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          const uint64_t kRegVal = cpu->GetReg(f.rs2);
          STORE_VIRTUAL_MEMORY(int32_t, kRegVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00000013,
        .name = "ADDI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002013,
        .name = "SLTI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(f.rs1) < (int64_t)f.imm ? 1 : 0;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003013,
        .name = "SLTIU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val =
              (uint64_t)cpu->GetReg(f.rs1) < (uint64_t)f.imm ? 1 : 0;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00004013,
        .name = "XORI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = cpu->GetReg(f.rs1) ^ f.imm;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006013,
        .name = "ORI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = (int64_t)cpu->GetReg(f.rs1) | (int64_t)f.imm;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007013,
        .name = "ANDI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = (int64_t)cpu->GetReg(f.rs1) & (int64_t)f.imm;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00000033,
        .name = "ADD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) + (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x40000033,
        .name = "SUB",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) - (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00001033,
        .name = "SLL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)cpu->GetReg(kDesc.rs1)
                              << (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00002033,
        .name = "SLT",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) < (int64_t)cpu->GetReg(kDesc.rs2)
                  ? 1
                  : 0;
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00003033,
        .name = "SLTU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (uint64_t)cpu->GetReg(kDesc.rs1) <
                                      (uint64_t)cpu->GetReg(kDesc.rs2)
                                  ? 1
                                  : 0;
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00004033,
        .name = "XOR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) ^ (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00005033,
        .name = "SRL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint64_t rs1 = cpu->GetReg(kDesc.rs1);
          const uint64_t rs2 = cpu->GetReg(kDesc.rs2);
          const int64_t val = rs1 >> rs2;
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x40005033,
        .name = "SRA",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)cpu->GetReg(kDesc.rs1) >>
                              (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00006033,
        .name = "OR",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) | (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x00007033,
        .name = "AND",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val =
              (int64_t)cpu->GetReg(kDesc.rs1) & (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x0000000f,
        .name = "FENCE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfff0707f,
        .signature = 0x8330000f,
        .name = "FENCE.TSO",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xffffffff,
        .signature = 0x100000f,
        .name = "PAUSE",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xffffffff,
        .signature = 0x00000073,
        .name = "ECALL",
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
              .type = exception_type,
              .val = cpu->GetPC(),
          };
        },
    },

    {
        .mask = 0xffffffff,
        .signature = 0x00100073,
        .name = "EBREAK",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          return {
              .type = trap::TrapType::kBreakpoint,
              .val = cpu->GetPC(),
          };
        },
    },

    /*********** rv64_i instructions ***********/
    {
        .mask = 0x0000707f,
        .signature = 0x00006003,
        .name = "LWU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;

          uint32_t data = 0;
          LOAD_VIRTUAL_MEMORY(uint32_t, data);
          cpu->SetReg(f.rd, data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003003,
        .name = "LD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;

          int64_t data = 0;
          LOAD_VIRTUAL_MEMORY(int64_t, data);
          cpu->SetReg(f.rd, (int64_t)data);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003023,
        .name = "SD",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatS(inst_word);
          const uint64_t addr = (int64_t)cpu->GetReg(f.rs1) + (int64_t)f.imm;
          const uint64_t kRegVal = cpu->GetReg(f.rs2);
          STORE_VIRTUAL_MEMORY(int64_t, kRegVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x00001013,
        .name = "SLLI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val = (int64_t)cpu->GetReg(f.rs1) << (int64_t)shamt;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x00005013,
        .name = "SRLI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val = cpu->GetReg(f.rs1) >> shamt;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x40005013,
        .name = "SRAI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, false);
          const int64_t val = (int64_t)cpu->GetReg(f.rs1) >> shamt;
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x0000001b,
        .name = "ADDIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(f.rs1) + f.imm);
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000101b,
        .name = "SLLIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(f.rs1) << shamt);
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x0000501b,
        .name = "SRLIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint32_t rs1 = (uint32_t)cpu->GetReg(f.rs1);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val = (int64_t)(int32_t)(rs1 >> shamt);
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfc00707f,
        .signature = 0x4000501b,
        .name = "SRAIW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto& f = decode::ParseFormatI(inst_word);
          const uint8_t shamt = decode::GetShamt(inst_word, true);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(f.rs1) >> shamt);
          cpu->SetReg(f.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000003b,
        .name = "ADDW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(kDesc.rs1) +
                                        (int32_t)cpu->GetReg(kDesc.rs2));
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x4000003b,
        .name = "SUBW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(kDesc.rs1) -
                                        (int32_t)cpu->GetReg(kDesc.rs2));
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000103b,
        .name = "SLLW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(kDesc.rs1)
                                        << (int32_t)cpu->GetReg(kDesc.rs2));
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x0000503b,
        .name = "SRLW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint32_t rs1 = (uint32_t)cpu->GetReg(kDesc.rs1);
          const uint32_t rs2 = (uint32_t)cpu->GetReg(kDesc.rs2);
          const int64_t val = (int64_t)(int32_t)(rs1 >> rs2);
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x4000503b,
        .name = "SRAW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t val = (int64_t)((int32_t)cpu->GetReg(kDesc.rs1) >>
                                        (int32_t)cpu->GetReg(kDesc.rs2));
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_system instructions ***********/

    {
        .mask = 0xffffffff,
        .signature = 0x30200073,
        .name = "MRET",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const uint64_t kOriginMstatusVal = cpu->state_.Read(csr::kCsrMstatus);
          const csr::MstatusDesc kOriginMstatusDesc =
              *reinterpret_cast<const csr::MstatusDesc*>(&kOriginMstatusVal);

          if (cpu->GetPrivilegeMode() < PrivilegeMode::kMachine) {
            return {
                .type = trap::TrapType::kIllegalInstruction,
                .val = cpu->GetPC() - 4,
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
        .mask = 0xffffffff,
        .signature = 0x10500073,
        .name = "WFI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          cpu->state_.SetWfi(true);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_zicsr instructions ***********/

    {
        .mask = 0x0000707f,
        .signature = 0x00001073,
        .name = "CSRRW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);

          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, true, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);
          const uint64_t kNewCsrVal = (int64_t)cpu->GetReg(kDesc.rs1);
          cpu->SetReg(kDesc.rd, kCsrVal);
          cpu->state_.Write(kDesc.imm, kNewCsrVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00002073,
        .name = "CSRRS",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);

          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, kDesc.rs1 != 0, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);

          const uint64_t kNewCsrVal =
              (int64_t)cpu->GetReg(kDesc.rs1) | (int64_t)kCsrVal;
          cpu->SetReg(kDesc.rd, kCsrVal);

          cpu->state_.Write(kDesc.imm, kNewCsrVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00003073,
        .name = "CSRRC",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);

          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, kDesc.rs1 != 0, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);
          const uint64_t kNewCsrVal =
              (int64_t)kCsrVal & (~((int64_t)cpu->GetReg(kDesc.rs1)));
          cpu->SetReg(kDesc.rd, kCsrVal);
          cpu->state_.Write(kDesc.imm, kNewCsrVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00005073,
        .name = "CSRRWI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);

          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, true, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);

          cpu->SetReg(kDesc.rd, kCsrVal);

          cpu->state_.Write(kDesc.imm, kDesc.rs1);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00006073,
        .name = "CSRRSI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);

          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, kDesc.rs1 != 0, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);

          const uint64_t kNewCsrVal = (int64_t)kCsrVal | (int64_t)kDesc.rs1;
          cpu->SetReg(kDesc.rd, kCsrVal);

          cpu->state_.Write(kDesc.imm, kNewCsrVal);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0x0000707f,
        .signature = 0x00007073,
        .name = "CSRRCI",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::CsrTypeDesc*>(&inst_word);
          CHECK_CSR_ACCESS_PRIVILEGE(kDesc.imm, kDesc.rs1 != 0, cpu);

          const uint64_t kCsrVal = cpu->state_.Read(kDesc.imm);

          const uint64_t kNewCsrVal =
              (int64_t)kCsrVal & (~((int64_t)kDesc.rs1));
          cpu->SetReg(kDesc.rd, kCsrVal);
          cpu->state_.Write(kDesc.imm, kNewCsrVal);
          return trap::kNoneTrap;
        },
    },

    /*********** rv_zifencei ***********/

    {
        .mask = 0x0000707f,
        .signature = 0x0000100f,
        .name = "FENCE.I",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // TODO: implement
          return trap::kNoneTrap;
        },
    },

    /*********** rv_m ***********/
    // https://tclin914.github.io/f37f836/
    {
        .mask = 0xfe00707f,
        .signature = 0x2000033,
        .name = "MUL",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t a = (int64_t)cpu->GetReg(kDesc.rs1);
          const int64_t b = (int64_t)cpu->GetReg(kDesc.rs2);
          const int64_t val = a * b;

          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2001033,
        .name = "MULH",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t a = (int64_t)cpu->GetReg(kDesc.rs1);
          const int64_t b = (int64_t)cpu->GetReg(kDesc.rs2);

          const bool kNegativeRes = (a < 0) ^ (b < 0);
          const uint64_t abs_a = static_cast<uint64_t>(a < 0 ? -a : a);
          const uint64_t abs_b = static_cast<uint64_t>(b < 0 ? -b : b);
          const uint64_t res =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(abs_a, abs_b);

          // use ~res directly because of UINT64_MAX^2 =
          // 0xfffffffffffffffe0000000000000001
          const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2002033,
        .name = "MULHSU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t a = (int64_t)cpu->GetReg(kDesc.rs1);
          const uint64_t b = cpu->GetReg(kDesc.rs2);

          const bool kNegativeRes = a < 0;
          const uint64_t abs_a = static_cast<uint64_t>(a < 0 ? -a : a);
          const uint64_t res =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(abs_a, b);

          const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2003033,
        .name = "MULHU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint64_t a = cpu->GetReg(kDesc.rs1);
          const uint64_t b = cpu->GetReg(kDesc.rs2);
          const uint64_t val =
              rv64_emulator::libs::arithmetic::MulUnsignedHi(a, b);

          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2004033,
        .name = "DIV",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t a = (int64_t)cpu->GetReg(kDesc.rs1);
          const int64_t b = (int64_t)cpu->GetReg(kDesc.rs2);

          if (b == 0) {
            cpu->SetReg(kDesc.rd, UINT64_MAX);
          } else if (a == INT64_MIN && b == -1) {
            cpu->SetReg(kDesc.rd, INT64_MIN);
          } else {
            const int64_t val = a / b;
            cpu->SetReg(kDesc.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2005033,
        .name = "DIVU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint64_t a = cpu->GetReg(kDesc.rs1);
          const uint64_t b = cpu->GetReg(kDesc.rs2);

          const uint64_t val = b == 0 ? UINT64_MAX : a / b;
          cpu->SetReg(kDesc.rd, val);

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2006033,
        .name = "REM",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t a = (int64_t)cpu->GetReg(kDesc.rs1);
          const int64_t b = (int64_t)cpu->GetReg(kDesc.rs2);

          if (b == 0) {
            cpu->SetReg(kDesc.rd, a);
          } else if (a == INT64_MIN && b == -1) {
            cpu->SetReg(kDesc.rd, 0);
          } else {
            const int64_t val = a % b;
            cpu->SetReg(kDesc.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x2007033,
        .name = "REMU",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint64_t a = cpu->GetReg(kDesc.rs1);
          const uint64_t b = cpu->GetReg(kDesc.rs2);

          const uint64_t val = b == 0 ? a : a % b;

          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    /*********** rv64_m ***********/
    {
        .mask = 0xfe00707f,
        .signature = 0x200003b,
        .name = "MULW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int32_t a = (int32_t)cpu->GetReg(kDesc.rs1);
          const int32_t b = (int32_t)cpu->GetReg(kDesc.rs2);

          const int64_t val = a * b;
          cpu->SetReg(kDesc.rd, val);
          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200403b,
        .name = "DIVW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int32_t a = (int32_t)cpu->GetReg(kDesc.rs1);
          const int32_t b = (int32_t)cpu->GetReg(kDesc.rs2);

          if (b == 0) {
            cpu->SetReg(kDesc.rd, UINT64_MAX);
          } else if (a == INT32_MIN && b == -1) {
            cpu->SetReg(kDesc.rd, INT32_MIN);
          } else {
            const int64_t val = (int64_t)(a / b);
            cpu->SetReg(kDesc.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200503b,
        .name = "DIVUW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint32_t a = (uint32_t)cpu->GetReg(kDesc.rs1);
          const uint32_t b = (uint32_t)cpu->GetReg(kDesc.rs2);

          if (b == 0) {
            cpu->SetReg(kDesc.rd, UINT64_MAX);
          } else {
            const int64_t val = (int32_t)(a / b);
            cpu->SetReg(kDesc.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200603b,
        .name = "REMW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int32_t a = (int32_t)cpu->GetReg(kDesc.rs1);
          const int32_t b = (int32_t)cpu->GetReg(kDesc.rs2);

          if (b == 0) {
            const int64_t val = a;
            cpu->SetReg(kDesc.rd, val);
          } else if (a == INT32_MIN && b == -1) {
            cpu->SetReg(kDesc.rd, 0);
          } else {
            const int64_t val = (int64_t)((int32_t)(a % b));
            cpu->SetReg(kDesc.rd, val);
          }

          return trap::kNoneTrap;
        },
    },

    {
        .mask = 0xfe00707f,
        .signature = 0x200703b,
        .name = "REMUW",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          // reference:
          // https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const uint32_t a = cpu->GetReg(kDesc.rs1);
          const uint32_t b = cpu->GetReg(kDesc.rs2);

          const int64_t val = (int32_t)(b == 0 ? a : a % b);
          cpu->SetReg(kDesc.rd, val);

          return trap::kNoneTrap;
        },
    },

    /*********** rv_s instructions ***********/
    {
        .mask = 0xffffffff,
        .signature = 0x10200073,
        .name = "SRET",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const uint64_t kOriginSstatusVal = cpu->state_.Read(csr::kCsrSstatus);
          const csr::MstatusDesc* kOriginSsDesc =
              reinterpret_cast<const csr::MstatusDesc*>(&kOriginSstatusVal);

          // 当TSR=1时，尝试在s模式下执行SRET将引发非法的指令异常
          if (cpu->GetPrivilegeMode() < PrivilegeMode::kSupervisor ||
              kOriginSsDesc->tsr) {
            return {
                .type = trap::TrapType::kIllegalInstruction,
                .val = cpu->GetPC() - 4,
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
        .mask = 0xfe007fff,
        .signature = 0x12000073,
        .name = "SFENCE.VMA",
        .Exec = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
          const auto kDesc =
              *reinterpret_cast<const decode::RTypeDesc*>(&inst_word);
          const int64_t kVirtAddr = (int64_t)cpu->GetReg(kDesc.rs1);
          const int64_t kAsid = (int64_t)cpu->GetReg(kDesc.rs2);
          cpu->FlushTlb(kVirtAddr, kAsid & 0xffff);
          return trap::kNoneTrap;
        },
    },
};

}  // namespace instruction
}  // namespace rv64_emulator::cpu
