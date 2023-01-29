#include "cpu/cpu.h"
#include "bus.h"
#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/trap.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "libs/LRU.hpp"

#include <cassert>
#include <cstdint>
#include <tuple>

namespace rv64_emulator::cpu {

const Instruction kInstructionTable[] = {
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
        .m_mask = 0x0000707f,
        .m_data = 0x00003073,
        .m_name = "CSRRC",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            const uint64_t new_csr_val = (int64_t)csr_val & (~((int64_t)cpu->GetGeneralPurposeRegVal(f.rs)));
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, new_csr_val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00007073,
        .m_name = "CSRRCI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            const uint64_t new_csr_val = (int64_t)csr_val & (~((int64_t)f.rs));
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, new_csr_val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00002073,
        .m_name = "CSRRS",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            const uint64_t new_csr_val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs) | (int64_t)csr_val;
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, new_csr_val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00006073,
        .m_name = "CSRRSI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            const uint64_t new_csr_val = (int64_t)csr_val | (int64_t)f.rs;
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, new_csr_val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00001073,
        .m_name = "CSRRW",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            const uint64_t new_csr_val = (int64_t)cpu->GetGeneralPurposeRegVal(f.rs);
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, new_csr_val);
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x00005073,
        .m_name = "CSRRWI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& f               = decode::ParseFormatCsr(inst_word);
            const auto& [csr_val, trap] = cpu->ReadCsr(f.csr);
            if (trap.m_trap_type != TrapType::kNone) {
                return trap;
            }
            cpu->SetGeneralPurposeRegVal(f.rd, csr_val);
            return cpu->WriteCsr(f.csr, f.rs);
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00100073,
        .m_name = "EBREAK",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            // TODO: implement
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xffffffff,
        .m_data = 0x00000073,
        .m_name = "ECALL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const PrivilegeMode pm             = cpu->GetPrivilegeMode();
            TrapType            exception_type = TrapType::kNone;
            switch (pm) {
                case PrivilegeMode::kUser:
                    exception_type = TrapType::kEnvironmentCallFromUMode;
                    break;
                case PrivilegeMode::kSupervisor:
                    exception_type = TrapType::kEnvironmentCallFromSMode;
                    break;
                case PrivilegeMode::kMachine:
                    exception_type = TrapType::kEnvironmentCallFromMMode;
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
        .m_data = 0x0000000f,
        .m_name = "FENCE",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            // TODO: implement
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0x0000707f,
        .m_data = 0x0000100f,
        .m_name = "FENCE.I",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            // TODO: implement
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
            const uint64_t new_pc   = ((int64_t)cpu->GetGeneralPurposeRegVal(f.rs1) + (int64_t)f.imm) & 0xfffffffffffffffe;

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
        .m_mask = 0xffffffff,
        .m_data = 0x30200073,
        .m_name = "MRET",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& [new_pc, mepc_trap] = cpu->ReadCsr(csr::kCsrMepc);
            if (mepc_trap.m_trap_type != TrapType::kNone) {
                return mepc_trap;
            }

            cpu->SetPC(new_pc);

            const auto& [status, mstatus_trap] = cpu->ReadCsr(csr::kCsrMstatus);
            if (mstatus_trap.m_trap_type != TrapType::kNone) {
                return mstatus_trap;
            }

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
            cpu->WriteCsr(csr::kCsrMstatus, new_status);
            cpu->SetPrivilegeMode(PrivilegeMode(mpp));
            // TODO: update new mode
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
        .m_mask = 0xffffffff,
        .m_data = 0x10200073,
        .m_name = "SRET",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto& [new_pc, sepc_trap] = cpu->ReadCsr(csr::kCsrSepc);
            if (sepc_trap.m_trap_type != TrapType::kNone) {
                return sepc_trap;
            }

            cpu->SetPC(new_pc);

            const auto& [status, sstatus_trap] = cpu->ReadCsr(csr::kCsrSstatus);
            if (sstatus_trap.m_trap_type != TrapType::kNone) {
                return sstatus_trap;
            }

            const uint64_t spie = (status >> 5) & 1;
            const uint64_t spp  = (status >> 8) & 1;
            const uint64_t mprv = (PrivilegeMode(spp) < PrivilegeMode::kMachine) ? 0 : ((status >> 17) & 1);

            const uint64_t new_status = (status & ~0x20122) | (mprv << 17) | (spie << 1) | (1 << 5);
            cpu->WriteCsr(csr::kCsrSstatus, new_status);
            cpu->SetPrivilegeMode(PrivilegeMode(spp));
            // TODO: update new mode
            return { .m_trap_type = TrapType::kNone, .m_val = 0 };
        },
    },

    {
        .m_mask = 0xfe00707f,
        .m_data = 0x00005033,
        .m_name = "SRL",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            const auto&    f   = decode::ParseFormatR(inst_word);
            const uint64_t rs1 = cpu->GetGeneralPurposeRegVal(f.rs1);
            const uint64_t rs2 = cpu->GetGeneralPurposeRegVal(f.rs2);
            const int64_t  val = rs1 >> rs2;
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
        .m_mask = 0xffffffff,
        .m_data = 0x10500073,
        .m_name = "WFI",
        .Exec   = [](CPU* cpu, const uint32_t inst_word) -> Trap {
            cpu->SetWfi(true);
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

uint64_t CPU::GetCsrCauseReg(const PrivilegeMode pm) {
    switch (pm) {
        case PrivilegeMode::kMachine:
            return csr::kCsrMcause;
        case PrivilegeMode::kSupervisor:
            return csr::kCsrScause;
        case PrivilegeMode::kUser:
            return csr::kCsrUcause;
        case PrivilegeMode::kReserved:
        default:
            assert(false);
    }
    return 0;
}

uint64_t CPU::GetCsrEpcReg(const PrivilegeMode pm) {
    switch (pm) {
        case PrivilegeMode::kMachine:
            return csr::kCsrMepc;
        case PrivilegeMode::kSupervisor:
            return csr::kCsrSepc;
        case PrivilegeMode::kUser:
            return csr::kCsrUepc;
        case PrivilegeMode::kReserved:
        default:
            assert(false);
    }
    return 0;
}

uint64_t CPU::GetCsrTvalReg(const PrivilegeMode pm) {
    switch (pm) {
        case PrivilegeMode::kMachine:
            return csr::kCsrMtval;
        case PrivilegeMode::kSupervisor:
            return csr::kCsrStval;
        case PrivilegeMode::kUser:
            return csr::kCsrUtval;
        case PrivilegeMode::kReserved:
        default:
            assert(false);
    }
    return 0;
}

uint64_t CPU::GetCstTvecReg(const PrivilegeMode pm) {
    switch (pm) {
        case PrivilegeMode::kMachine:
            return csr::kCsrMtvec;
        case PrivilegeMode::kSupervisor:
            return csr::kCsrStvec;
        case PrivilegeMode::kUser:
            return csr::kCsrUtvec;
        case PrivilegeMode::kReserved:
        default:
            assert(false);
    }
    return 0;
}

CPU::CPU(ArchMode arch_mode, PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_clock(0)
    , m_instruction_count(0)
    , m_arch_mode(arch_mode)
    , m_privilege_mode(privilege_mode)
    , m_pc(kDramBaseAddr)
    , m_last_executed_pc(kDramBaseAddr)
    , m_mstatus(0)
    , m_csr(kCsrCapacity, 0)
    , m_bus(std::move(bus))
    , m_decode_cache(decode::kDecodeCacheEntryNum) {
    static_assert(sizeof(float) == 4, "float is not 4 bytes, can't assure the bit width of floating point reg");
    m_reg[0] = 0;
    m_reg[2] = kDramBaseAddr + kDramSize;
#ifdef DEBUG
    fmt::print("cpu init, bus addr is {}\n", fmt::ptr(m_bus.get()));
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
        case csr::kCsrMCycle:
        case csr::kCsrMinstret:
            return GetMaxXLen() == 64 ? m_csr[csr_addr] : m_csr[csr_addr] & 0x00000000ffffffff;
        case csr::kCsrMCycleH:
        case csr::kCsrMinstretH:
            return GetMaxXLen() == 64 ? m_csr[csr_addr] : m_csr[csr_addr] & 0xffffffff00000000;
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
        case csr::kCsrMCycle:
        case csr::kCsrMinstret:
            m_csr[csr_addr] = GetMaxXLen() == 64 ? val : val & 0x00000000ffffffff;
        case csr::kCsrMCycleH:
        case csr::kCsrMinstretH:
            m_csr[csr_addr] = GetMaxXLen() == 64 ? val : val & 0xffffffff00000000;
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

std::tuple<uint64_t, Trap> CPU::ReadCsr(const uint16_t csr_addr) const {
    bool is_privileged = HasCsrAccessPrivilege(csr_addr);
    if (!is_privileged) {
        return {
            0,
            {
                .m_trap_type = TrapType::kIllegalInstruction,
                .m_val       = GetPC() - 4,
            },
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

    WriteCsrDirectly(csr_addr, val);
    // TODO: UPDATE ADDRESS MODEL
    return {
        .m_trap_type = TrapType::kNone,
        .m_val       = 0,
    };
}

bool CPU::CheckInterruptBitsValid(const PrivilegeMode cur_pm, const PrivilegeMode new_pm, const TrapType trap_type) const {
    // https://dingfen.github.io/assets/img/mie.png
    const uint64_t cur_status = GetCsrStatusRegVal(cur_pm);
    const uint64_t ie         = GetInterruptEnable(new_pm);

    // get the interrupt enable bit
    bool cur_mie = (cur_status >> 3) & 1;
    bool cur_sie = (cur_status >> 1) & 1;
    bool cur_uie = cur_status & 1;

    // 1. cur_privilege_mode < new_privilege_mode: Interrupt is always enabled
    // 2. cur_privilege_mode > new_privilege_mode: Interrupt is always disabled
    // 3. cur_privilege_mode == new_privilege_mode: Interrupt is enabled if xIE in xstatus is 1
    if (new_pm < cur_pm) {
        return false;
    } else if (new_pm == cur_pm) {
        switch (cur_pm) {
            case PrivilegeMode::kUser:
                if (!cur_uie) {
                    return false;
                }
                break;
            case PrivilegeMode::kSupervisor:
                if (!cur_sie) {
                    return false;
                }
                break;
            case PrivilegeMode::kMachine:
                if (!cur_mie) {
                    return false;
                }
                break;
            case PrivilegeMode::kReserved:
                assert(false);
            default:
                break;
        }
    }

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

    switch (trap_type) {
        case TrapType::kUserSoftwareInterrupt:
            if (!usie) {
                return false;
            }
            break;
        case TrapType::kSupervisorSoftwareInterrupt:
            if (!ssie) {
                return false;
            }
            break;
        case TrapType::kMachineSoftwareInterrupt:
            if (!msie) {
                return false;
            }
            break;
        case TrapType::kUserTimerInterrupt:
            if (!utie) {
                return false;
            }
            break;
        case TrapType::kSupervisorTimerInterrupt:
            if (!stie) {
                return false;
            }
            break;
        case TrapType::kMachineTimerInterrupt:
            if (!mtie) {
                return false;
            }
            break;
        case TrapType::kUserExternalInterrupt:
            if (!ueie) {
                return false;
            }
            break;
        case TrapType::kSupervisorExternalInterrupt:
            if (!seie) {
                return false;
            }
            break;
        case TrapType::kMachineExternalInterrupt:
            if (!meie) {
                return false;
            }
            break;
        default:
            return false;
    }

    return true;
}

void CPU::ModifyCsrStatusReg(const PrivilegeMode cur_pm, const PrivilegeMode new_pm) {
    switch (new_pm) {
        case PrivilegeMode::kMachine: {
            const uint64_t status = ReadCsrDirectly(csr::kCsrMstatus);

            bool mie = (status >> 3) & 1;
            // 1. clear MPP(BIT 12, 11), MPIE(BIT 7), MIE(BIT 3)
            // 2. override MPP[12:11] with current privilege encoding
            // 3. set MIE(bit 3) to MPIE(bit 7)
            const uint64_t new_status = (status & (~0x1888)) | (mie << 7) | ((uint64_t)cur_pm << 11);
            WriteCsrDirectly(csr::kCsrMstatus, new_status);
        } break;
        case PrivilegeMode::kSupervisor: {
            uint64_t status = ReadCsrDirectly(csr::kCsrSstatus);

            bool sie = (status >> 1) & 1;
            // 1. clear SIE(bit 1), SPIE(bit 5), SPP(bit 8)
            // 2. override SPP(bit 8) with current privilege encoding
            // 3. set SIE(bit 1) to SPIE(bit 5)
            const uint64_t new_status = (status & (~0x122)) | (sie << 5) | (((uint64_t)cur_pm & 1) << 8);
            WriteCsrDirectly(csr::kCsrSstatus, new_status);
        } break;
        case PrivilegeMode::kUser:
        case PrivilegeMode::kReserved:
        default:
            assert(false);
            break;
    }
}

std::tuple<bool, uint64_t> CPU::GetTrapCause(const Trap trap) const {
    // https://dingfen.github.io/assets/img/mcause_table.png
    bool     is_interrupt = false;
    uint64_t cause_bits   = kTrapToCauseTable.at(trap.m_trap_type);

    // if current trap's type is interrupt, set the msb to 1
    if (trap.m_trap_type >= TrapType::kUserSoftwareInterrupt) {
        is_interrupt                 = true;
        const uint64_t interrupt_bit = static_cast<uint64_t>(1) << (GetMaxXLen() - 1);
        cause_bits |= interrupt_bit;
    }

    return { is_interrupt, cause_bits };
}

uint64_t CPU::GetCsrStatusRegVal(const PrivilegeMode mode) const {
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
uint64_t CPU::GetTrapVectorNewPC(const uint64_t csr_tvec_addr, const uint64_t exception_code) const {
    const uint64_t csr_tvec_val = ReadCsrDirectly(csr_tvec_addr);
    const uint8_t  mode         = csr_tvec_val & 0x3;
    uint64_t       new_pc       = 0;
    switch (mode) {
        case 0:
            new_pc = csr_tvec_val;
            break;
        case 1:
            new_pc = (csr_tvec_val & (~0x3)) + 4 * exception_code;
            break;
        default:
            // reversed
            assert(false);
            break;
    }
    return new_pc;
}

bool CPU::HandleTrap(const Trap trap, const uint64_t epc) {
    const PrivilegeMode cur_privilege_mode = GetPrivilegeMode();
    assert(cur_privilege_mode != PrivilegeMode::kReserved);
    const auto [is_interrupt, origin_cause_bits] = GetTrapCause(trap);

    uint64_t cause_bits = origin_cause_bits;

    if (is_interrupt) {
        // clear interrupt bit
        const uint64_t interrupt_bit = static_cast<uint64_t>(1) << (GetMaxXLen() - 1);
        cause_bits &= (~interrupt_bit);
    }

    const uint64_t mxdeleg        = ReadCsrDirectly(is_interrupt ? csr::kCsrMideleg : csr::kCsrMedeleg);
    const uint64_t exception_code = cause_bits;

    bool switch_to_s_mode =
        (cur_privilege_mode <= PrivilegeMode::kSupervisor && exception_code < GetMaxXLen() && mxdeleg & (1 << exception_code));
    PrivilegeMode new_privilege_mode = switch_to_s_mode ? PrivilegeMode::kSupervisor : PrivilegeMode::kMachine;

    if (is_interrupt) {
        if (!CheckInterruptBitsValid(cur_privilege_mode, new_privilege_mode, trap.m_trap_type)) {
            // TODO: 打印日志
            return false;
        }
    }

    const uint64_t csr_tvec_addr  = CPU::GetCstTvecReg(new_privilege_mode);
    const uint64_t csr_epc_addr   = CPU::GetCsrEpcReg(new_privilege_mode);
    const uint64_t csr_cause_addr = CPU::GetCsrCauseReg(new_privilege_mode);
    const uint64_t csr_tval_addr  = CPU::GetCsrTvalReg(new_privilege_mode);

    SetPC(GetTrapVectorNewPC(csr_tvec_addr, exception_code));
    WriteCsrDirectly(csr_epc_addr, epc);
    WriteCsrDirectly(csr_cause_addr, origin_cause_bits);
    WriteCsrDirectly(csr_tval_addr, trap.m_val);
    ModifyCsrStatusReg(cur_privilege_mode, new_privilege_mode);
    SetPrivilegeMode(new_privilege_mode);

    return true;
}

void CPU::HandleInterrupt(const uint64_t inst_addr) {
    const uint64_t mip = ReadCsrDirectly(csr::kCsrMip);
    const uint64_t mie = ReadCsrDirectly(csr::kCsrMie);

    const uint64_t machine_interrupts = mip & mie;

    TrapType trap_type    = TrapType::kNone;
    uint16_t csr_mip_mask = 0;

    // 中断优先级：MEI > MSI > MTI > SEI > SSI > STI
    do {
        if (!machine_interrupts) {
            break;
        }

        if (machine_interrupts & csr::kCsrMeipMask) {
            trap_type    = TrapType::kMachineExternalInterrupt;
            csr_mip_mask = csr::kCsrMeipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrMsipMask) {
            trap_type    = TrapType::kMachineSoftwareInterrupt;
            csr_mip_mask = csr::kCsrMsipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrMtipMask) {
            trap_type    = TrapType::kMachineTimerInterrupt;
            csr_mip_mask = csr::kCsrMtipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrSeipMask) {
            trap_type    = TrapType::kSupervisorExternalInterrupt;
            csr_mip_mask = csr::kCsrSeipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrSsipMask) {
            trap_type    = TrapType::kSupervisorSoftwareInterrupt;
            csr_mip_mask = csr::kCsrSsipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrStipMask) {
            trap_type    = TrapType::kSupervisorTimerInterrupt;
            csr_mip_mask = csr::kCsrStipMask;
            break;
        }
    } while (false);

    const Trap trap = {
        .m_trap_type = trap_type,
        .m_val       = GetPC(),
    };

    if (trap.m_trap_type != TrapType::kNone && HandleTrap(trap, inst_addr)) {
        WriteCsrDirectly(csr::kCsrMip, mip & (~csr_mip_mask));
        m_wfi = false;
        return;
    }

    // TODO: 中断处理失败，打印日志
}

uint32_t CPU::Fetch() {
    uint32_t inst_word = m_bus->Load(GetPC(), kInstructionBits);
    // m_pc += 4;
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
    for (auto& inst : kInstructionTable) {
        if ((inst_word & inst.m_mask) == inst.m_data) {
            m_decode_cache.Set(inst_word, inst_table_index);
            return inst_table_index;
        }
        inst_table_index++;
    }

    return -1;
}

Trap CPU::TickOperate() {
    if (m_wfi) {
        const uint64_t mie = ReadCsrDirectly(csr::kCsrMie);
        const uint64_t mip = ReadCsrDirectly(csr::kCsrMip);
        if (mie & mip) {
            m_wfi = false;
        }
        return {
            .m_trap_type = TrapType::kNone,
            .m_val       = 0,
        };
    }

    const uint64_t inst_addr = GetPC();
    const uint32_t inst_word = Fetch();
    SetPC(inst_addr + 4);

    int64_t instruction_index = Decode(inst_word);
    if (instruction_index == -1) {
        return {
            .m_trap_type = TrapType::kIllegalInstruction,
            .m_val       = inst_addr,
        };
    }

    const Trap trap    = kInstructionTable[instruction_index].Exec(this, inst_word);
    m_last_executed_pc = inst_addr;
    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;
    return trap;
}

void CPU::Tick() {
    const uint64_t epc  = GetPC();
    const Trap     trap = TickOperate();
    if (trap.m_trap_type != TrapType::kNone) {
        if (!HandleTrap(trap, epc)) {
            // TODO: 日志
        }
    }

    HandleInterrupt(GetPC());

    // rust style 'wrapping_add' not need
    // risc-v cycle, time, instret regs: risc-v1.com/thread-968-1-1.html
    WriteCsrDirectly(csr::kCsrMCycle, ++m_clock);
    WriteCsrDirectly(csr::kCsrMinstret, ++m_instruction_count);
}

CPU::~CPU() {
#ifdef DEBUG
    fmt::print("destroy a cpu\n");
#endif
}

void CPU::Dump() const {
    // Application Binary Interface registers
    const char* abi[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
        "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };

    const uint32_t inst_word = m_bus->Load(m_last_executed_pc, kInstructionBits);
    fmt::print("{:#016x} -> {:#08x}\n", m_last_executed_pc, inst_word);

    for (int i = 0; i < 8; i++) {
        fmt::print("   {}: {:#016x}  ", abi[i], m_reg[i]);
        fmt::print("   {}: {:#016x}  ", abi[i + 8], m_reg[i + 8]);
        fmt::print("   {}: {:#016x}  ", abi[i + 16], m_reg[i + 16]);
        fmt::print("   {}: {:#016x}  \n", abi[i + 24], m_reg[i + 24]);
    }
}

} // namespace rv64_emulator::cpu
