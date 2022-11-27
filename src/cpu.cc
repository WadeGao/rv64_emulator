#include "include/cpu.h"
#include "include/bus.h"
#include "include/conf.h"
#include "include/csr.h"
#include "include/decode.h"
#include "libs/LRU.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <tuple>

namespace rv64_emulator {
namespace cpu {

const Instruction RV64I_Instructions[] = {
    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00000033,
        .m_name = "ADD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000013,
        .m_name = "ADDI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000001b,
        .m_name = "ADDIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + f.imm);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000003b,
        .m_name = "ADDW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00007033,
        .m_name = "AND",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007013,
        .m_name = "ANDI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) & (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000017,
        .m_name = "AUIPC",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatU(inst_word);
            const int64_t val = (int64_t)(cpu->GetPC() - 4) + (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000063,
        .m_name = "BEQ",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) == (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005063,
        .m_name = "BGE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007063,
        .m_name = "BGEU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >= (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004063,
        .m_name = "BLT",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006063,
        .m_name = "BLTU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001063,
        .m_name = "BNE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatB(inst_word);
            if ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) != (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2)) {
                const uint64_t new_pc = cpu->GetPC() + ((int64_t)f.imm - 4);
                cpu->SetPC(new_pc);
            }
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x0000006f,
        .m_name = "JAL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatJ(inst_word);
            cpu->SetGeneralPurposeRegVal(f.rd, cpu->GetPC());
            const uint64_t new_pc = (int64_t)cpu->GetPC() + (int64_t)f.imm - 4;
            cpu->SetPC(new_pc);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000067,
        .m_name = "JALR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f        = decode::ParseFormatI(inst_word);
            const uint64_t saved_pc = cpu->GetPC();
            const uint64_t new_pc   = ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm) & 0xfffffffe;

            cpu->SetPC(new_pc);
            cpu->SetGeneralPurposeRegVal(f.rd, saved_pc);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000003,
        .m_name = "LB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int8_t   data = (int8_t)(cpu->Load(addr, 8));
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004003,
        .m_name = "LBU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 8);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003003,
        .m_name = "LD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int64_t  data = (int64_t)cpu->Load(addr, 64);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001003,
        .m_name = "LH",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int16_t  data = (int16_t)cpu->Load(addr, 16);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005003,
        .m_name = "LHU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 16);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000007f,
        .m_data = 0x00000037,
        .m_name = "LUI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f = decode::ParseFormatU(inst_word);
            cpu->SetGeneralPurposeRegVal(f.rd, (uint64_t)f.imm);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002003,
        .m_name = "LW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const int32_t  data = (int32_t)cpu->Load(addr, 32);
            cpu->SetGeneralPurposeRegVal(f.rd, (int64_t)data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006003,
        .m_name = "LWU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatI(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            const uint64_t data = cpu->Load(addr, 32);
            cpu->SetGeneralPurposeRegVal(f.rd, data);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00006033,
        .m_name = "OR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006013,
        .m_name = "ORI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) | (int64_t)f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00000023,
        .m_name = "SB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 8, cpu->GetGeneralPurposeRegVal(f.rs2));
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003023,
        .m_name = "SD",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 64, cpu->GetGeneralPurposeRegVal(f.rs2));
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001023,
        .m_name = "SH",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 16, cpu->GetGeneralPurposeRegVal(f.rs2));
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00001033,
        .m_name = "SLL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00001013,
        .m_name = "SLLI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
            const int64_t  val       = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int64_t)shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000101b,
        .m_name = "SLLIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
            const int64_t  val       = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000103b,
        .m_name = "SLLW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) << (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00002033,
        .m_name = "SLT",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002013,
        .m_name = "SLTI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (int64_t)f.imm ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00003013,
        .m_name = "SLTIU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)f.imm ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00003033,
        .m_name = "SLTU",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs1) < (uint64_t)cpu->GetGeneralPurposeRegVal(f.rs2) ? 1 : 0;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40005033,
        .m_name = "SRA",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x40005013,
        .m_name = "SRAI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
            const int64_t  val       = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x4000501b,
        .m_name = "SRAIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
            const int64_t  val       = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000503b,
        .m_name = "SRAW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x00005013,
        .m_name = "SRLI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit32);
            const int64_t  val       = cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfc00707f,
        .m_data = 0x0000501b,
        .m_name = "SRLIW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f         = decode::ParseFormatI(inst_word);
            const ArchMode arch_mode = cpu->GetArchMode();
            const uint8_t  shamt     = decode::GetShamt(inst_word, arch_mode == ArchMode::kBit64);
            const int64_t  val       = (int64_t)(int32_t)(cpu->GetGeneralPurposeRegVal(f.rs1) >> shamt);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x0000503b,
        .m_name = "SRLW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((uint32_t)cpu->GetGeneralPurposeRegVal(f.rs1) >> (uint32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x40000033,
        .m_name = "SUB",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x4000003b,
        .m_name = "SUBW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)((int32_t)cpu->GetGeneralPurposeRegVal(f.rs1) - (int32_t)cpu->GetGeneralPurposeRegVal(f.rs2));
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002023,
        .m_name = "SW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f    = decode::ParseFormatS(inst_word);
            const uint64_t addr = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm;
            cpu->Store(addr, 32, cpu->GetGeneralPurposeRegVal(f.rs2));
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00004033,
        .m_name = "XOR",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatR(inst_word);
            const int64_t val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) ^ (int64_t)cpu->GetGeneralPurposeRegVal(f.rs2);
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00004013,
        .m_name = "XORI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&   f   = decode::ParseFormatI(inst_word);
            const int64_t val = cpu->GetGeneralPurposeRegVal(f.rs1) ^ f.imm;
            cpu->SetGeneralPurposeRegVal(f.rd, val);
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },
};

CPU::CPU(ArchMode arch_mode, PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_clock(0)
    , m_arch_mode(arch_mode)
    , m_privilege_mode(privilege_mode)
    , m_pc(kDramBaseAddr)
    , m_mstatus(0)
    , m_bus(std::move(bus))
    , m_decode_cache(decode::kDecodeCacheEntryNum) {
    m_reg[0] = 0;
    m_reg[2] = kDramBaseAddr + kDramSize;
#ifdef DEBUG
    printf("cpu init, bus addr is %p\n", m_bus.get());
#endif
}

uint64_t CPU::ReadCsrDirectly(const uint16_t csr_addr) const {
    switch (csr_addr) {
        case csr::kCsrFflags:
            return m_csr[csr::kCsrFcsr] & 0x1f;
        case csr::kCsrFrm:
            return (m_csr[csr::kCsrFcsr] >> 5) & 0x7;
        case csr::kCsrSstatus:
            return m_csr[csr::kCsrMstatus] & 0x80000003000de162;
        case csr::kCsrSie:
            return m_csr[csr::kCsrMie] & 0x222;
        case csr::kCsrSip:
            return m_csr[csr::kCsrMip] & 0x222;
        case csr::kCsrTime:
            // TODO: clint module
        default:
            break;
    }
    return m_csr[csr_addr];
}

void CPU::WriteCsrDirectly(const uint16_t csr_addr, const uint64_t val) {
    switch (csr_addr) {
        case csr::kCsrFflags:
            m_csr[csr::kCsrFcsr] &= (~0x1f);
            m_csr[csr::kCsrFcsr] |= (val & 0x1f);
            break;
        case csr::kCsrFrm:
            m_csr[csr::kCsrFcsr] &= (~0xe0);
            m_csr[csr::kCsrFcsr] |= (val << 5 & 0xe0);
            break;
        case csr::kCsrMstatus:
            m_csr[csr_addr] = val;
            UpdateMstatus(ReadCsrDirectly(csr::kCsrMstatus));
            break;
        case csr::kCsrSstatus:
            m_csr[csr::kCsrMstatus] &= (~0x80000003000de162);
            m_csr[csr::kCsrMstatus] |= (val & 0x80000003000de162);
            UpdateMstatus(ReadCsrDirectly(csr::kCsrMstatus));
            break;
        case csr::kCsrSie:
            m_csr[csr::kCsrMie] &= (~0x222);
            m_csr[csr::kCsrMie] |= (val & 0x222);
            break;
        case csr::kCsrSip:
            m_csr[csr::kCsrMip] &= (~0x222);
            m_csr[csr::kCsrMip] |= (val & 0x222);
            break;
        case csr::kCsrMideleg:
            m_csr[csr_addr] = val & 0x666; // from qemu
            break;
        case csr::kCsrTime:
            // TODO: clint module
            break;
        default:
            m_csr[csr_addr] = val;
            break;
    }
}

void CPU::UpdateMstatus(const uint64_t mstatus) {
    m_mstatus = mstatus;
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

std::tuple<uint64_t, Trap> CPU::ReadCsr(const uint16_t csr_addr) const {
    bool is_privileged = HasCsrAccessPrivilege(csr_addr);
    if (!is_privileged) {
        return {
            0,
            { .m_trap_type = TrapType::kIllegalInstruction, .m_val = GetPC() - 4 },
        };
    }

    return {
        ReadCsrDirectly(csr_addr),
        {
            .m_trap_type = TrapType::kNone,
            .m_val       = 0,
        },
    };
}

Trap CPU::WriteCsr(const uint16_t csr_addr, const uint64_t val) {
    bool is_privileged = HasCsrAccessPrivilege(csr_addr);
    if (!is_privileged) {
        return {
            .m_trap_type = TrapType::kIllegalInstruction,
            .m_val       = GetPC() - 4,
        };
    }

    return {
        .m_trap_type = TrapType::kNone,
        .m_val       = 0,
    };
}

uint64_t CPU::GetTrapCause(const Trap trap) const {
    // https://dingfen.github.io/assets/img/mcause_table.png
    const uint64_t interrupt_bit = (GetArchMode() == ArchMode::kBit64) ? 0x8000000000000000 : 0x80000000;

    uint64_t cause = 0;
    switch (trap.m_trap_type) {
        case TrapType::kInstructionAddressMisaligned:
            cause = 0;
            break;
        case TrapType::kInstructionAccessFault:
            cause = 1;
            break;
        case TrapType::kIllegalInstruction:
            cause = 2;
            break;
        case TrapType::kBreakpoint:
            cause = 3;
            break;
        case TrapType::kLoadAddressMisaligned:
            cause = 4;
            break;
        case TrapType::kLoadAccessFault:
            cause = 5;
            break;
        case TrapType::kStoreAddressMisaligned:
            cause = 6;
            break;
        case TrapType::kStoreAccessFault:
            cause = 7;
            break;
        case TrapType::kEnvironmentCallFromUMode:
            cause = 8;
            break;
        case TrapType::kEnvironmentCallFromSMode:
            cause = 9;
            break;
        case TrapType::kEnvironmentCallFromMMode:
            cause = 11;
            break;
        case TrapType::kInstructionPageFault:
            cause = 12;
            break;
        case TrapType::kLoadPageFault:
            cause = 13;
            break;
        case TrapType::kStorePageFault:
            cause = 15;
            break;
        // below are interrupts
        case TrapType::kUserSoftwareInterrupt:
            cause = interrupt_bit;
            break;
        case TrapType::kSupervisorSoftwareInterrupt:
            cause = interrupt_bit + 1;
            break;
        case TrapType::kMachineSoftwareInterrupt:
            cause = interrupt_bit + 3;
            break;
        case TrapType::kUserTimerInterrupt:
            cause = interrupt_bit + 4;
            break;
        case TrapType::kSupervisorTimerInterrupt:
            cause = interrupt_bit + 5;
            break;
        case TrapType::kMachineTimerInterrupt:
            cause = interrupt_bit + 7;
            break;
        case TrapType::kUserExternalInterrupt:
            cause = interrupt_bit + 8;
            break;
        case TrapType::kSupervisorExternalInterrupt:
            cause = interrupt_bit + 9;
            break;
        case TrapType::kMachineExternalInterrupt:
            cause = interrupt_bit + 11;
            break;
        default:
            assert(false);
            break;
    }

    return cause;
}

uint64_t CPU::GetCurrentStatus(const PrivilegeMode mode) const {
    uint64_t current_status = 0;
    switch (mode) {
        case PrivilegeMode::kUser:
            current_status = ReadCsrDirectly(csr::kCsrUstatus);
            break;
        case PrivilegeMode::kSupervisor:
            current_status = ReadCsrDirectly(csr::kCsrSstatus);
            break;
        case PrivilegeMode::kMachine:
            current_status = ReadCsrDirectly(csr::kCsrMstatus);
            break;
        default:
            assert(false);
            break;
    }

    return current_status;
}

uint64_t CPU::GetInterruptEnable(const PrivilegeMode mode) const {
    uint64_t ie = 0;
    switch (mode) {
        case PrivilegeMode::kUser:
            ie = ReadCsrDirectly(csr::kCsrUie);
            break;
        case PrivilegeMode::kSupervisor:
            ie = ReadCsrDirectly(csr::kCsrSie);
            break;
        case PrivilegeMode::kMachine:
            ie = ReadCsrDirectly(csr::kCsrMie);
            break;
        default:
            assert(false);
            break;
    }

    return ie;
}

bool CPU::HandleTrap(const Trap trap, const uint64_t inst_addr, bool is_interrupt) {
    const PrivilegeMode cur_privilege_mode = GetPrivilegeMode();
    assert(cur_privilege_mode != PrivilegeMode::kReserved);
    const uint64_t cur_status = GetCurrentStatus(cur_privilege_mode);

    const uint64_t cause   = GetTrapCause(trap);
    const uint64_t mxdeleg = is_interrupt ? ReadCsrDirectly(csr::kCsrMideleg) : ReadCsrDirectly(csr::kCsrMedeleg);
    const uint64_t sxdeleg = is_interrupt ? ReadCsrDirectly(csr::kCsrSideleg) : ReadCsrDirectly(csr::kCsrSedeleg);

    const uint16_t      pos                = cause & 0xffff;
    const PrivilegeMode new_privilege_mode = (((mxdeleg >> pos) & 1) == 0)   ? PrivilegeMode::kMachine
                                             : (((sxdeleg >> pos) & 1) == 0) ? PrivilegeMode::kSupervisor
                                                                             : PrivilegeMode::kUser;

    if (is_interrupt) {
        const uint64_t ie = GetInterruptEnable(new_privilege_mode);

        // https://dingfen.github.io/assets/img/mie.png

        // get the interrupt enable bit
        bool cur_mie = (cur_status >> 3) & 1;
        bool cur_sie = (cur_status >> 1) & 1;
        bool cur_uie = cur_status & 1;

        // software interrupt-enable in x mode
        bool msie = (ie >> 3) & 1;
        bool ssie = (ie >> 1) & 1;
        bool usie = ie & 1;

        // timer interrupt-enable bit in x mode
        bool mtie = (ie >> 7) & 1;
        bool stie = (ie >> 5) & 1;
        bool utie = (ie >> 4) & 1;

        // external interrupt-enable in x mode
        bool meie = (ie >> 11) & 1;
        bool seie = (ie >> 9) & 1;
        bool ueie = (ie >> 8) & 1;

        // 1. cur_privilege_mode < new_privilege_mode: Interrupt is always enabled
        // 2. cur_privilege_mode > new_privilege_mode: Interrupt is always disabled
        // 3. cur_privilege_mode = new_privilege_mode: Interrupt is enabled if xIE in xstatus is 1
        // TODO
    }
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

    // decode cache miss, find the index in instruction table
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

    int64_t instruction_index = Decode(inst_word);
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
