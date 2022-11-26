#include "include/cpu.h"
#include "include/bus.h"
#include "include/conf.h"
#include "include/decode.h"
#include "libs/LRU.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace rv64_emulator {
namespace cpu {

const Instruction RV64I_Instructions[] = {
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00000033,
        .m_name = "ADD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> void {
            const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000013,
        .m_name = "ADDI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000001b,
        .m_name = "ADDIW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + f.imm);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000003b,
        .m_name = "ADDW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00007033,
        .m_name = "AND",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007013,
        .m_name = "ANDI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)f.imm;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000017,
        .m_name = "AUIPC",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatU(inst_word);
                const int64_t val = (int64_t)(cpu->GetPC() - 4) + (int64_t)f.imm;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000063,
        .m_name = "BEQ",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) == (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005063,
        .m_name = "BGE",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007063,
        .m_name = "BGEU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004063,
        .m_name = "BLT",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006063,
        .m_name = "BLTU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001063,
        .m_name = "BNE",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatB(inst_word);
                if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) != (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                    const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                    cpu->SetPC(new_pc);
                }
            },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x0000006f,
        .m_name = "JAL",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatJ(inst_word);
                cpu->SetGeneralPurposeRegVal(f.rd, cpu->GetPC());
                const uint64_t new_pc = (int64_t)cpu->GetPC() + (int64_t)f.imm - 4;
                cpu->SetPC(new_pc);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000067,
        .m_name = "JALR",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f        = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t saved_pc = cpu->GetPC();
                const uint64_t new_pc   = ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm) & 0xfffffffe;

                cpu->SetPC(new_pc);
                cpu->SetGeneralPurposeRegVal(f.rd, saved_pc);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000003,
        .m_name = "LB",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const int8_t   data = (int8_t)(cpu->Load(addr, 8));
                cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004003,
        .m_name = "LBU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const uint64_t data = cpu->Load(addr, 8);
                cpu->SetGeneralPurposeRegVal(f.rd, data);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003003,
        .m_name = "LD",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const int64_t  data = (int64_t)cpu->Load(addr, 64);
                cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001003,
        .m_name = "LH",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const int16_t  data = (int16_t)cpu->Load(addr, 16);
                cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005003,
        .m_name = "LHU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const uint64_t data = cpu->Load(addr, 16);
                cpu->SetGeneralPurposeRegVal(f.rd, data);
            },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000037,
        .m_name = "LUI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto& f = rv64_emulator::decoder::ParseFormatU(inst_word);
                cpu->SetGeneralPurposeRegVal(f.rd, (uint64_t)f.imm);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002003,
        .m_name = "LW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const int32_t  data = (int32_t)cpu->Load(addr, 32);
                cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006003,
        .m_name = "LWU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatI(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                const uint64_t data = cpu->Load(addr, 32);
                cpu->SetGeneralPurposeRegVal(f.rd, data);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00006033,
        .m_name = "OR",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006013,
        .m_name = "ORI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)f.imm;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000023,
        .m_name = "SB",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatS(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                cpu->Store(addr, 8, cpu->GetGeneralPurposeRegVal(f.rs2));
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003023,
        .m_name = "SD",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatS(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                cpu->Store(addr, 64, cpu->GetGeneralPurposeRegVal(f.rs2));
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001023,
        .m_name = "SH",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatS(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                cpu->Store(addr, 16, cpu->GetGeneralPurposeRegVal(f.rs2));
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00001033,
        .m_name = "SLL",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00001013,
        .m_name = "SLLI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
                const int64_t  val       = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)shamt;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000101b,
        .m_name = "SLLIW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
                const int64_t  val       = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << shamt);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000103b,
        .m_name = "SLLW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00002033,
        .m_name = "SLT",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002013,
        .m_name = "SLTI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)f.imm ? 1 : 0;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003013,
        .m_name = "SLTIU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)f.imm ? 1 : 0;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00003033,
        .m_name = "SLTU",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40005033,
        .m_name = "SRA",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x40005013,
        .m_name = "SRAI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
                const int64_t  val       = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x4000501b,
        .m_name = "SRAIW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
                const int64_t  val       = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000503b,
        .m_name = "SRAW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00005013,
        .m_name = "SRLI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
                const int64_t  val       = cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x0000501b,
        .m_name = "SRLIW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f         = rv64_emulator::decoder::ParseFormatI(inst_word);
                const ArchMode arch_mode = cpu->GetArchMode();
                const uint8_t  shamt     = rv64_emulator::decoder::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
                const int64_t  val       = (int64_t)(int32_t)(cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000503b,
        .m_name = "SRLW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f = rv64_emulator::decoder::ParseFormatR(inst_word);
                const int64_t val =
                    (int64_t)((uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40000033,
        .m_name = "SUB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> void {
            const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000003b,
        .m_name = "SUBW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> void {
            const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002023,
        .m_name = "SW",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&    f    = rv64_emulator::decoder::ParseFormatS(inst_word);
                const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
                cpu->Store(addr, 32, cpu->GetGeneralPurposeRegVal(f.rs2));
            },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00004033,
        .m_name = "XOR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> void {
            const auto&   f   = rv64_emulator::decoder::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) ^ (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004013,
        .m_name = "XORI",
        .Exec =
            [](CPU* cpu, const uint32_t inst_word) {
                const auto&   f   = rv64_emulator::decoder::ParseFormatI(inst_word);
                const int64_t val = cpu->GetGeneralPurposeRegVal(f.rs1) ^ f.imm;
                cpu->SetGeneralPurposeRegVal(f.rd, val);
            },
    },
};

CPU::CPU(ArchMode arch_mode, PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_arch_mode(arch_mode)
    , m_privilege_mode(privilege_mode)
    , m_pc(kDramBaseAddr)
    , m_bus(std::move(bus))
    , m_decode_cache(rv64_emulator::decoder::kDecodeCacheEntryNum) {
    m_reg[0] = 0;
    m_reg[2] = kDramBaseAddr + kDramSize;
#ifdef DEBUG
    printf("cpu init, bus addr is %p\n", m_bus.get());
#endif
}

uint64_t CPU::Load(const uint64_t addr, const uint64_t bit_size) const {
    return m_bus->Load(addr, bit_size);
}

void CPU::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    m_bus->Store(addr, bit_size, val);
}

void CPU::SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val) {
    assert(reg_num <= kGeneralPurposeRegNum - 1);
    m_reg[reg_num] = val;
}

uint64_t CPU::GetGeneralPurposeRegVal(const uint64_t reg_num) const {
    assert(reg_num <= kGeneralPurposeRegNum - 1);
    return m_reg[reg_num];
}

void CPU::SetPC(const uint64_t new_pc) {
    m_pc = new_pc;
}

uint64_t CPU::GetPC() const {
    return m_pc;
}

ArchMode CPU::GetArchMode() const {
    return m_arch_mode;
}

PrivilegeMode CPU::GetPrivilegeMode() const {
    return m_privilege_mode;
}

bool CPU::HasCsrAccessPrivilege(const uint16_t csr_num) const {
    // 可以访问改csr寄存器的最低特权级别
    const uint16_t lowest_privilege_mode = (csr_num >> 8) & 0b11;
    return lowest_privilege_mode <= uint16_t(m_privilege_mode);
}

uint32_t CPU::Fetch() {
    uint32_t inst_word = m_bus->Load(m_pc, kInstructionBits);
    m_pc += 4;
    return inst_word;
}

int64_t CPU::Decode(const uint32_t inst_word) {
    int64_t inst_table_index = 0;

    // decode cache hit current instruction
    if (m_decode_cache.Get(inst_word, inst_table_index)) {
        return inst_table_index;
    }

    // decode cache miss
    inst_table_index = 0;
    for (auto& inst : RV64I_Instructions) {
        if ((inst_word & inst.m_mask) == inst.m_data) {
            m_decode_cache.Set(inst_word, inst_table_index);
            return inst_table_index;
        }
        inst_table_index++;
    }

    return -1;
}

uint64_t CPU::Execute(const uint32_t inst_word) {
    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;

    int32_t instruction_index = Decode(inst_word);
    if (instruction_index == -1) {
        printf("unknown instruction\n");
        // assert(false);
        return 0;
    }

    RV64I_Instructions[instruction_index].Exec(this, inst_word);
    return 0;
}

CPU::~CPU() {
#ifdef DEBUG
    printf("destroy a cpu\n");
#endif
}

#ifdef DEBUG

void CPU::Dump() const {
    // Application Binary Interface registers
    const char* abi[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
        "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };

    const uint32_t inst_word = m_bus->Load(m_pc, kInstructionBits);
    printf("0x%016llx -> 0x%08x\n", m_pc, inst_word);

    for (int i = 0; i < 8; i++) {
        printf("   %4s: 0x%016llx  ", abi[i], m_reg[i]);
        printf("   %2s: 0x%016llx  ", abi[i + 8], m_reg[i + 8]);
        printf("   %2s: 0x%016llx  ", abi[i + 16], m_reg[i + 16]);
        printf("   %3s: 0x%016llx\n", abi[i + 24], m_reg[i + 24]);
    }
}

#endif

} // namespace cpu
} // namespace rv64_emulator
