#pragma once

#include "cpu/cpu.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "libs/utils.hpp"

namespace rv64_emulator::cpu {

class CPU;

namespace instruction {

constexpr trap::Trap kNoneTrap = {
    .m_trap_type = trap::TrapType::kNone,
    .m_val       = 0,
};

typedef struct Instruction {
    uint32_t    m_mask;
    uint32_t    m_data;
    const char* m_name;
    trap::Trap (*Exec)(CPU* cpu, const uint32_t inst_word);
} Instruction;

const Instruction kInstructionTable[] = {
    /*********** rv_i instructions ***********/
    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000037,
        .m_name = "LUI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatU(inst_word);

            const uint64_t val = (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000017,
        .m_name = "AUIPC",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatU(inst_word);
            const int64_t val = (int64_t)(cpu->GetPC() - 4) + (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x0000006f,
        .m_name = "JAL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatJ(inst_word);
            cpu->SetGeneralPurposeRegVal(f.rd, cpu->GetPC());
            const uint64_t new_pc = (int64_t)cpu->GetPC() + (int64_t)f.imm - 4;
            cpu->SetPC(new_pc);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000067,
        .m_name = "JALR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f        = decode::ParseFormatI(inst_word);
            const uint64_t saved_pc = cpu->GetPC();
            const uint64_t new_pc   = ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm) & 0xfffffffffffffffe;

            cpu->SetPC(new_pc);
            cpu->SetGeneralPurposeRegVal(f.rd, saved_pc);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000063,
        .m_name = "BEQ",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) == (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001063,
        .m_name = "BNE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) != (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004063,
        .m_name = "BLT",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005063,
        .m_name = "BGE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006063,
        .m_name = "BLTU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007063,
        .m_name = "BGEU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000003,
        .m_name = "LB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int8_t   data = (int8_t)(cpu->Load(addr, 8));
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001003,
        .m_name = "LH",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int16_t  data = (int16_t)cpu->Load(addr, 16);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002003,
        .m_name = "LW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int32_t  data = (int32_t)cpu->Load(addr, 32);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004003,
        .m_name = "LBU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 8);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005003,
        .m_name = "LHU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 16);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000023,
        .m_name = "SB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 8, cpu->GetGeneralPurposeRegVal(f.rs2));
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001023,
        .m_name = "SH",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 16, cpu->GetGeneralPurposeRegVal(f.rs2));
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002023,
        .m_name = "SW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 32, cpu->GetGeneralPurposeRegVal(f.rs2));
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000013,
        .m_name = "ADDI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002013,
        .m_name = "SLTI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)f.imm ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003013,
        .m_name = "SLTIU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)f.imm ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004013,
        .m_name = "XORI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = cpu->GetGeneralPurposeRegVal(f.rs1) ^ f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006013,
        .m_name = "ORI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007013,
        .m_name = "ANDI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00000033,
        .m_name = "ADD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40000033,
        .m_name = "SUB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00001033,
        .m_name = "SLL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00002033,
        .m_name = "SLT",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00003033,
        .m_name = "SLTU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00004033,
        .m_name = "XOR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) ^ (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00005033,
        .m_name = "SRL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f   = decode::ParseFormatR(inst_word);
            const uint64_t rs1 = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t rs2 = cpu->GetGeneralPurposeRegVal(f.rs2);
            const int64_t  val = rs1 >> rs2;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40005033,
        .m_name = "SRA",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00006033,
        .m_name = "OR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00007033,
        .m_name = "AND",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000000f,
        .m_name = "FENCE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // TODO: implement
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfff0707f,
        .m_data = 0x8330000f,
        .m_name = "FENCE.TSO",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // TODO: implement
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x100000f,
        .m_name = "PAUSE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // TODO: implement
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00000073,
        .m_name = "ECALL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const PrivilegeMode pm             = cpu->GetPrivilegeMode();
            trap::TrapType      exception_type = trap::TrapType::kNone;
            switch (pm) {
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
                    assert(false);
                    break;
            }
            return {
                .m_trap_type = exception_type,
                .m_val       = cpu->GetPC(),
            };
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00100073,
        .m_name = "EBREAK",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // TODO: implement
            return kNoneTrap;
        },
    },

    /*********** rv64_i instructions ***********/
    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006003,
        .m_name = "LWU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 32);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003003,
        .m_name = "LD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int64_t  data = (int64_t)cpu->Load(addr, 64);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003023,
        .m_name = "SD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 64, cpu->GetGeneralPurposeRegVal(f.rs2));
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00001013,
        .m_name = "SLLI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, false);
            const int64_t val   = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00005013,
        .m_name = "SRLI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, false);
            const int64_t val   = cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x40005013,
        .m_name = "SRAI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, false);
            const int64_t val   = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000001b,
        .m_name = "ADDIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + f.imm);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000101b,
        .m_name = "SLLIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, true);
            const int64_t val   = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x0000501b,
        .m_name = "SRLIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, true);
            const int64_t val   = (int64_t)(int32_t)(cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x4000501b,
        .m_name = "SRAIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f     = decode::ParseFormatI(inst_word);
            const uint8_t shamt = decode::GetShamt(inst_word, true);
            const int64_t val   = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000003b,
        .m_name = "ADDW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000003b,
        .m_name = "SUBW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000103b,
        .m_name = "SLLW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000503b,
        .m_name = "SRLW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000503b,
        .m_name = "SRAW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    /*********** rv_system instructions ***********/

    {
        .m_mask = 0xffffffff,
        .m_data = 0x30200073,
        .m_name = "MRET",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const uint64_t new_pc = cpu->m_state.Read(csr::kCsrMepc);

            cpu->SetPC(new_pc);

            const uint64_t status = cpu->m_state.Read(csr::kCsrMstatus);

            const uint64_t mpie = (status >> 7) & 1;
            const uint64_t mpp  = (status >> 11) & 3;

            /*
                MPRV：modify privilege。
                当MPRV=0，load和store行为和正常一样，用当前特权等级做地址转换和保护机制;
                当MPRV=1时，load和store使用MPP作为当前等级，进行translated和protected。
                指令地址转换和保护不受MPRV影响。
                MRET或者SRET指令改变privilege mode到低于M模式会设置MPRV=0。
            */
            const uint64_t mprv = (PrivilegeMode(mpp) < PrivilegeMode::kMachine) ? 0 : ((status >> 17) & 1);
            /*
                When executing an xRET instruction, supposing xPP holds the value y
                xIE is set to xPIE; (mpie << 3)
                xPIE is set to 1; (1 << 7)
                and xPP is set to U (or M if user-mode is not supported) (set MPP[12:11] to 0 by 'status & ~0x21888')
                the privilege mode is changed to y;
            */

            // new_status = (clear mprv mpp mpie mie) | (overwrite mprv) | (MIE is set to MPIE) | (MPIE enable)
            const uint64_t new_status = (status & ~0x21888) | (mprv << 17) | (mpie << 3) | (1 << 7);
            cpu->m_state.Write(csr::kCsrMstatus, new_status);
            cpu->SetPrivilegeMode(PrivilegeMode(mpp));
            // TODO: update new mode
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x10500073,
        .m_name = "WFI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            cpu->SetWfi(true);
            return kNoneTrap;
        },
    },

    /*********** rv_zicsr instructions ***********/

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001073,
        .m_name = "CSRRW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f       = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val = cpu->m_state.Read(f.csr);

            const uint64_t new_csr_val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs);
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            cpu->m_state.Write(f.csr, new_csr_val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002073,
        .m_name = "CSRRS",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f       = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val = cpu->m_state.Read(f.csr);

            const uint64_t new_csr_val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs) | (int64_t)csr_val;
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

            cpu->m_state.Write(f.csr, new_csr_val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003073,
        .m_name = "CSRRC",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f           = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val     = cpu->m_state.Read(f.csr);
            const uint64_t new_csr_val = (int64_t)csr_val & (~((int64_t)cpu->GetGeneralPurposeRegVal(f.rs)));
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            cpu->m_state.Write(f.csr, new_csr_val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005073,
        .m_name = "CSRRWI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f       = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val = cpu->m_state.Read(f.csr);

            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

            cpu->m_state.Write(f.csr, f.rs);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006073,
        .m_name = "CSRRSI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f       = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val = cpu->m_state.Read(f.csr);

            const uint64_t new_csr_val = (int64_t)csr_val | (int64_t)f.rs;
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);

            cpu->m_state.Write(f.csr, new_csr_val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007073,
        .m_name = "CSRRCI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f       = decode::ParseFormatCsr(inst_word);
            const uint64_t csr_val = cpu->m_state.Read(f.csr);

            const uint64_t new_csr_val = (int64_t)csr_val & (~((int64_t)f.rs));
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            cpu->m_state.Write(f.csr, new_csr_val);
            return kNoneTrap;
        },
    },

    /*********** rv_zifencei ***********/

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000100f,
        .m_name = "FENCE.I",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // TODO: implement
            return kNoneTrap;
        },
    },

    /*********** rv_m ***********/
    // https://tclin914.github.io/f37f836/
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2000033,
        .m_name = "MUL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t a   = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const int64_t b   = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            const int64_t val = a * b;

            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2001033,
        .m_name = "MULH",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f = decode::ParseFormatR(inst_word);
            const int64_t a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const int64_t b = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);

            const bool     kNegativeRes = (a < 0) ^ (b < 0);
            const uint64_t abs_a        = static_cast<uint64_t>(a < 0 ? -a : a);
            const uint64_t abs_b        = static_cast<uint64_t>(b < 0 ? -b : b);
            const uint64_t res          = rv64_emulator::libs::MulUnsignedHi(abs_a, abs_b);

            // use ~res directly because of UINT64^$_MAX^2 = 0xfffffffffffffffe0000000000000001
            const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2002033,
        .m_name = "MULHSU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f = decode::ParseFormatR(inst_word);
            const int64_t  a = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

            // TODO
            const bool     kNegativeRes = a < 0;
            const uint64_t abs_a        = static_cast<uint64_t>(a < 0 ? -a : a);
            const uint64_t res          = rv64_emulator::libs::MulUnsignedHi(abs_a, b);

            const int64_t val = kNegativeRes ? (~res + (a * b == 0)) : res;

            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2003033,
        .m_name = "MULHU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&    f   = decode::ParseFormatR(inst_word);
            const uint64_t a   = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t b   = cpu->GetGeneralPurposeRegVal(f.rs2);
            const uint64_t val = rv64_emulator::libs::MulUnsignedHi(a, b);

            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2004033,
        .m_name = "DIV",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&   f = decode::ParseFormatR(inst_word);
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

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2005033,
        .m_name = "DIVU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&    f = decode::ParseFormatR(inst_word);
            const uint64_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

            const uint64_t val = b == 0 ? UINT64_MAX : a / b;
            cpu->SetGeneralPurposeRegVal(f.rd, val);

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2006033,
        .m_name = "REM",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&   f = decode::ParseFormatR(inst_word);
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

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x2007033,
        .m_name = "REMU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const auto&    f = decode::ParseFormatR(inst_word);
            const uint64_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

            const uint64_t val = b == 0 ? a : a % b;

            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    /*********** rv64_m ***********/
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200003b,
        .m_name = "MULW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&   f = decode::ParseFormatR(inst_word);
            const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

            const int64_t val = a * b;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200403b,
        .m_name = "DIVW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&   f = decode::ParseFormatR(inst_word);
            const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

            if (b == 0) {
                cpu->SetGeneralPurposeRegVal(f.rd, UINT64_MAX);
            } else {
                const int64_t val = (int64_t)(a / b);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            }

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200503b,
        .m_name = "DIVUW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&    f = decode::ParseFormatR(inst_word);
            const uint32_t a = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint32_t b = (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

            if (b == 0) {
                cpu->SetGeneralPurposeRegVal(f.rd, UINT64_MAX);
            } else {
                const int64_t val = (int32_t)(a / b);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            }

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200603b,
        .m_name = "REMW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&   f = decode::ParseFormatR(inst_word);
            const int32_t a = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs1);
            const int32_t b = (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2);

            if (b == 0) {
                // const uint64_t val = cpu->GetGeneralPurposeRegVal(f.rs1);
                const int64_t val = a;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            } else {
                const int64_t val = (int64_t)((int32_t)(a % b));
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            }

            return kNoneTrap;
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x200703b,
        .m_name = "REMUW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            // reference: https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
            const auto&    f = decode::ParseFormatR(inst_word);
            const uint32_t a = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint32_t b = cpu->GetGeneralPurposeRegVal(f.rs2);

            const int64_t val = (int32_t)(b == 0 ? a : a % b);
            cpu->SetGeneralPurposeRegVal(f.rd, val);

            return kNoneTrap;
        },
    },

    /*********** TODO ***********/
    {
        .m_mask = 0xffffffff,
        .m_data = 0x10200073,
        .m_name = "SRET",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> trap::Trap {
            const uint64_t new_pc = cpu->m_state.Read(csr::kCsrSepc);

            cpu->SetPC(new_pc);

            const uint64_t status = cpu->m_state.Read(csr::kCsrSstatus);

            const uint64_t spie = (status >> 5) & 1;
            const uint64_t spp  = (status >> 8) & 1;
            const uint64_t mprv = (PrivilegeMode(spp) < PrivilegeMode::kMachine) ? 0 : ((status >> 17) & 1);

            const uint64_t new_status = (status & ~0x20122) | (mprv << 17) | (spie << 1) | (1 << 5);
            cpu->m_state.Write(csr::kCsrSstatus, new_status);
            cpu->SetPrivilegeMode(PrivilegeMode(spp));
            // TODO: update new mode
            return kNoneTrap;
        },
    },

};

} // namespace instruction
} // namespace rv64_emulator::cpu
