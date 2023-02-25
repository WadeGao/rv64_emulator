#include "cpu/cpu.h"
#include "bus.h"
#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/instruction.h"
#include "cpu/trap.h"

#include "libs/LRU.hpp"
#include "libs/software_arithmetic.hpp"

#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/format.h"

#include <cassert>
#include <cstdint>
#include <map>
#include <tuple>

namespace rv64_emulator::cpu {

const std::map<PrivilegeMode, uint64_t> kCauseReg = {
    { PrivilegeMode::kMachine, csr::kCsrMcause },
    { PrivilegeMode::kSupervisor, csr::kCsrScause },
    { PrivilegeMode::kUser, csr::kCsrUcause },
};

const std::map<PrivilegeMode, uint64_t> kEpcReg = {
    { PrivilegeMode::kMachine, csr::kCsrMepc },
    { PrivilegeMode::kSupervisor, csr::kCsrSepc },
    { PrivilegeMode::kUser, csr::kCsrUepc },
};

const std::map<PrivilegeMode, uint64_t> kTvalReg = {
    { PrivilegeMode::kMachine, csr::kCsrMtval },
    { PrivilegeMode::kSupervisor, csr::kCsrStval },
    { PrivilegeMode::kUser, csr::kCsrUtval },
};

const std::map<PrivilegeMode, uint64_t> kTvecReg = {
    { PrivilegeMode::kMachine, csr::kCsrMtvec },
    { PrivilegeMode::kSupervisor, csr::kCsrStvec },
    { PrivilegeMode::kUser, csr::kCsrUtvec },
};

const std::map<PrivilegeMode, uint64_t> kStatusReg = {
    { PrivilegeMode::kMachine, csr::kCsrMstatus },
    { PrivilegeMode::kSupervisor, csr::kCsrSstatus },
    { PrivilegeMode::kUser, csr::kCsrUstatus },
};

const std::map<PrivilegeMode, uint64_t> kInterruptEnableReg = {
    { PrivilegeMode::kMachine, csr::kCsrMie },
    { PrivilegeMode::kSupervisor, csr::kCsrSie },
    { PrivilegeMode::kUser, csr::kCsrUie },
};

uint32_t gpr_change_record = 0;

CPU::CPU(PrivilegeMode privilege_mode, std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_clock(0)
    , m_instruction_count(0)
    , m_privilege_mode(privilege_mode)
    , m_bus(std::move(bus))
    , m_decode_cache(decode::kDecodeCacheEntryNum) {
    static_assert(sizeof(float) == 4, "float is not 4 bytes, can't assure the bit width of floating point reg");
    m_reg[0] = 0;
    m_reg[2] = kDramBaseAddr + kDramSize;
#ifdef DEBUG
    fmt::print("cpu init, bus addr is {}\n", fmt::ptr(m_bus.get()));
#endif
}

void CPU::Reset() {
    m_clock             = 0;
    m_instruction_count = 0;
    m_pc                = kDramBaseAddr;

    m_state.Reset();
    m_decode_cache.Reset();
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

bool CPU::CheckInterruptBitsValid(const PrivilegeMode cur_pm, const PrivilegeMode new_pm, const trap::TrapType trap_type) const {
    // https://dingfen.github.io/assets/img/mie.png
    const uint64_t cur_status = m_state.Read(kStatusReg.at(cur_pm));
    const uint64_t ie         = m_state.Read(kInterruptEnableReg.at(new_pm));

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
#ifdef DEBUG
                fmt::print("Unknown privilege mode[{}] when check interrupt bits valid, now abort\n", static_cast<int>(cur_pm));
#endif
                exit(static_cast<int>(rv64_emulator::errorcode::CpuErrorCode::kReservedPrivilegeMode));
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
        case trap::TrapType::kUserSoftwareInterrupt:
            if (!usie) {
                return false;
            }
            break;
        case trap::TrapType::kSupervisorSoftwareInterrupt:
            if (!ssie) {
                return false;
            }
            break;
        case trap::TrapType::kMachineSoftwareInterrupt:
            if (!msie) {
                return false;
            }
            break;
        case trap::TrapType::kUserTimerInterrupt:
            if (!utie) {
                return false;
            }
            break;
        case trap::TrapType::kSupervisorTimerInterrupt:
            if (!stie) {
                return false;
            }
            break;
        case trap::TrapType::kMachineTimerInterrupt:
            if (!mtie) {
                return false;
            }
            break;
        case trap::TrapType::kUserExternalInterrupt:
            if (!ueie) {
                return false;
            }
            break;
        case trap::TrapType::kSupervisorExternalInterrupt:
            if (!seie) {
                return false;
            }
            break;
        case trap::TrapType::kMachineExternalInterrupt:
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
            const uint64_t status = m_state.Read(csr::kCsrMstatus);

            bool mie = (status >> 3) & 1;
            // 1. clear MPP(BIT 12, 11), MPIE(BIT 7), MIE(BIT 3)
            // 2. override MPP[12:11] with current privilege encoding
            // 3. set MIE(bit 3) to MPIE(bit 7)
            const uint64_t new_status = (status & (~0x1888)) | (mie << 7) | ((uint64_t)cur_pm << 11);
            m_state.Write(csr::kCsrMstatus, new_status);
        } break;
        case PrivilegeMode::kSupervisor: {
            uint64_t status = m_state.Read(csr::kCsrSstatus);

            bool sie = (status >> 1) & 1;
            // 1. clear SIE(bit 1), SPIE(bit 5), SPP(bit 8)
            // 2. override SPP(bit 8) with current privilege encoding
            // 3. set SIE(bit 1) to SPIE(bit 5)
            const uint64_t new_status = (status & (~0x122)) | (sie << 5) | (((uint64_t)cur_pm & 1) << 8);
            m_state.Write(csr::kCsrSstatus, new_status);
        } break;
        case PrivilegeMode::kUser:
        case PrivilegeMode::kReserved:
        default:
#ifdef DEBUG
            fmt::print("Unknown privilege mode[{}] when modify csr status reg, now abort\n", static_cast<int>(new_pm));
#endif
            exit(static_cast<int>(rv64_emulator::errorcode::CpuErrorCode::kReservedPrivilegeMode));
            break;
    }
}

uint64_t CPU::GetTrapVectorNewPC(const uint64_t csr_tvec_addr, const uint64_t exception_code) const {
    const uint64_t csr_tvec_val = m_state.Read(csr_tvec_addr);
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
#ifdef DEBUG
            fmt::print("Unknown mode[{}] when cget tvec new pc, csr_tvec_val[{}], now abort\n", static_cast<int>(mode), csr_tvec_val);
#endif
            exit(static_cast<int>(rv64_emulator::errorcode::CpuErrorCode::kTrapVectorModeUndefined));
            break;
    }
    return new_pc;
}

bool CPU::HandleTrap(const trap::Trap trap, const uint64_t epc) {
    const PrivilegeMode cur_privilege_mode = GetPrivilegeMode();
    assert(cur_privilege_mode != PrivilegeMode::kReserved);
    const auto [is_interrupt, origin_cause_bits] = GetTrapCauseBits(trap);

    uint64_t cause_bits = origin_cause_bits;

    if (is_interrupt) {
        // clear interrupt bit
        const uint64_t interrupt_bit = static_cast<uint64_t>(1) << (GetMaxXLen() - 1);
        cause_bits &= (~interrupt_bit);
    }

    const uint64_t mxdeleg        = m_state.Read(is_interrupt ? csr::kCsrMideleg : csr::kCsrMedeleg);
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

    const uint64_t csr_tvec_addr  = kTvecReg.at(new_privilege_mode);
    const uint64_t csr_epc_addr   = kEpcReg.at(new_privilege_mode);
    const uint64_t csr_cause_addr = kCauseReg.at(new_privilege_mode);
    const uint64_t csr_tval_addr  = kTvalReg.at(new_privilege_mode);

    SetPC(GetTrapVectorNewPC(csr_tvec_addr, exception_code));

    m_state.Write(csr_epc_addr, epc);
    m_state.Write(csr_cause_addr, origin_cause_bits);
    m_state.Write(csr_tval_addr, trap.m_val);

    ModifyCsrStatusReg(cur_privilege_mode, new_privilege_mode);
    SetPrivilegeMode(new_privilege_mode);

    return true;
}

void CPU::HandleInterrupt(const uint64_t inst_addr) {
    const uint64_t mip = m_state.Read(csr::kCsrMip);
    const uint64_t mie = m_state.Read(csr::kCsrMie);

    const uint64_t machine_interrupts = mip & mie;

    trap::TrapType trap_type    = trap::TrapType::kNone;
    uint16_t       csr_mip_mask = 0;

    // 中断优先级：MEI > MSI > MTI > SEI > SSI > STI
    do {
        if (!machine_interrupts) {
            break;
        }

        if (machine_interrupts & csr::kCsrMeipMask) {
            trap_type    = trap::TrapType::kMachineExternalInterrupt;
            csr_mip_mask = csr::kCsrMeipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrMsipMask) {
            trap_type    = trap::TrapType::kMachineSoftwareInterrupt;
            csr_mip_mask = csr::kCsrMsipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrMtipMask) {
            trap_type    = trap::TrapType::kMachineTimerInterrupt;
            csr_mip_mask = csr::kCsrMtipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrSeipMask) {
            trap_type    = trap::TrapType::kSupervisorExternalInterrupt;
            csr_mip_mask = csr::kCsrSeipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrSsipMask) {
            trap_type    = trap::TrapType::kSupervisorSoftwareInterrupt;
            csr_mip_mask = csr::kCsrSsipMask;
            break;
        }

        if (machine_interrupts & csr::kCsrStipMask) {
            trap_type    = trap::TrapType::kSupervisorTimerInterrupt;
            csr_mip_mask = csr::kCsrStipMask;
            break;
        }
    } while (false);

    const trap::Trap trap = {
        .m_trap_type = trap_type,
        .m_val       = GetPC(),
    };

    if (trap.m_trap_type != trap::TrapType::kNone) {
        if (HandleTrap(trap, inst_addr)) {
            m_state.Write(csr::kCsrMip, mip & (~csr_mip_mask));
            m_wfi = false;
            return;
        } else {
#ifdef DEBUG
            fmt::print("Interrupt bits invalid when handling trap\n");
#endif
        }
    }
}

uint32_t CPU::Fetch() {
    uint32_t inst_word = m_bus->Load(GetPC(), kInstructionBits);
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
    for (const auto& inst : instruction::kInstructionTable) {
        if ((inst_word & inst.m_mask) == inst.m_data) {
            m_decode_cache.Set(inst_word, inst_table_index);
            return inst_table_index;
        }
        inst_table_index++;
    }

    return -1;
}

trap::Trap CPU::TickOperate() {
    if (m_wfi) {
        const uint64_t mie = m_state.Read(csr::kCsrMie);
        const uint64_t mip = m_state.Read(csr::kCsrMip);
        if (mie & mip) {
            m_wfi = false;
        }
        return {
            .m_trap_type = trap::TrapType::kNone,
            .m_val       = 0,
        };
    }

    const uint64_t inst_addr = GetPC();
    const uint32_t inst_word = Fetch();
    SetPC(inst_addr + 4);

    int64_t instruction_index = Decode(inst_word);
    if (instruction_index == -1) {
        return {
            .m_trap_type = trap::TrapType::kIllegalInstruction,
            .m_val       = inst_addr,
        };
    }

#ifdef DEBUG
    Disassemble(inst_addr, inst_word, instruction_index);
    uint64_t backup_reg[kGeneralPurposeRegNum] = { 0 };
    memcpy(backup_reg, m_reg, kGeneralPurposeRegNum * sizeof(uint64_t));
#endif

    const trap::Trap trap = instruction::kInstructionTable[instruction_index].Exec(this, inst_word);
    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;

#ifdef DEBUG
    gpr_change_record = 0;
    for (uint64_t i = 0; i < kGeneralPurposeRegNum; i++) {
        if (backup_reg[i] != m_reg[i]) {
            gpr_change_record |= (1 << i);
        }
    }
#endif

    return trap;
}

void CPU::Tick() {
    const uint64_t   epc  = GetPC();
    const trap::Trap trap = TickOperate();
    if (trap.m_trap_type != trap::TrapType::kNone) {
        if (!HandleTrap(trap, epc)) {
            fmt::print("Interrupt bits invalid when handling trap\n");
        }
    }

    HandleInterrupt(GetPC());

    // rust style 'wrapping_add' not need
    // risc-v cycle, time, instret regs: risc-v1.com/thread-968-1-1.html
    m_state.Write(csr::kCsrMCycle, ++m_clock);
    m_state.Write(csr::kCsrMinstret, ++m_instruction_count);
}

CPU::~CPU() {
#ifdef DEBUG
    fmt::print("destroy a cpu\n");
#endif
}

void CPU::Disassemble(const uint64_t pc, const uint32_t word, const int64_t instruction_table_index) const {
    fmt::print("{:#018x} {:#010x} {}\n", pc, word, instruction::kInstructionTable[instruction_table_index].m_name);
}

void CPU::DumpRegisters() const {
    // Application Binary Interface registers
    const char* abi[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
        "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };

    constexpr int kBiasTable[4] = { 0, 8, 16, 24 };

    for (int i = 0; i < 8; i++) {
        for (const auto bias : kBiasTable) {
            const int index = i + bias;
            if (gpr_change_record & (1 << index)) {
                fmt::print(
                    "      {:>28}",
                    fmt::format(fmt::bg(fmt::color::green) | fmt::fg(fmt::color::red), "{}: {:#018x}", abi[index], m_reg[index]));
                //  fmt::bg(fmt::color::green) | fmt::fg(fmt::color::red)
            } else {
                fmt::print("{:>28}", fmt::format("{}: {:#018x}", abi[index], m_reg[index]));
            }
        }
        fmt::print("\n");
    }
}

} // namespace rv64_emulator::cpu
