#include "cpu/cpu.h"
#include "bus.h"
#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/instruction.h"
#include "cpu/trap.h"
#include "mmu.h"

#include "libs/arithmetic.hpp"
#include "libs/lru.hpp"
#include "libs/utils.h"

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

CPU::CPU(std::unique_ptr<mmu::Mmu> mmu)
    : m_clock(0)
    , m_instruction_count(0)
    , m_privilege_mode(PrivilegeMode::kMachine)
    , m_pc(0)
    , m_wfi(false)
    , m_mmu(std::move(mmu))
    , m_decode_cache(kDecodeCacheEntryNum) {
    static_assert(sizeof(float) == 4, "float is not 4 bytes, can't assure the bit width of floating point reg");
    m_mmu->SetProcessor(this);
#ifdef DEBUG
    fmt::print("cpu init, mmu addr is {}\n", fmt::ptr(m_mmu.get()));
#endif
}

void CPU::Reset() {
    m_clock             = 0;
    m_instruction_count = 0;
    m_privilege_mode    = PrivilegeMode::kMachine;
    m_pc                = 0;
    m_wfi               = false;

    memset(m_reg, 0, sizeof(m_reg));
    memset(m_fp_reg, 0, sizeof(m_fp_reg));

    m_mmu->Reset();
    m_decode_cache.Reset();
    m_state.Reset();
}

trap::Trap CPU::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) const {
    return m_mmu->VirtualAddressLoad(addr, bytes, buffer);
}

trap::Trap CPU::Store(const uint64_t addr, const uint64_t bytes, const uint8_t* buffer) {
    return m_mmu->VirtualAddressStore(addr, bytes, buffer);
}

void CPU::SetGeneralPurposeRegVal(const uint64_t reg_num, const uint64_t val) {
    assert(reg_num <= kGeneralPurposeRegNum - 1);
    m_reg[reg_num] = val;
}

uint64_t CPU::GetGeneralPurposeRegVal(const uint64_t reg_num) const {
    assert(reg_num <= kGeneralPurposeRegNum - 1);
    return m_reg[reg_num];
}

void CPU::HandleTrap(const trap::Trap trap, const uint64_t epc) {
    if (trap.m_trap_type == trap::TrapType::kNone) {
        return;
    }

    const PrivilegeMode kOriginPM = GetPrivilegeMode();
    assert(kOriginPM != PrivilegeMode::kReserved);

    const csr::CauseDesc kCauseBits = {
        .cause     = trap::kTrapToCauseTable.at(trap.m_trap_type),
        .interrupt = trap.m_trap_type >= trap::TrapType::kUserSoftwareInterrupt,
    };

    const uint64_t      kMxdeleg     = m_state.Read(kCauseBits.interrupt ? csr::kCsrMideleg : csr::kCsrMedeleg);
    const bool          kTrapToSMode = (kOriginPM <= PrivilegeMode::kSupervisor && (kMxdeleg & (1 << kCauseBits.cause)));
    const PrivilegeMode kNewPM       = kTrapToSMode ? PrivilegeMode::kSupervisor : PrivilegeMode::kMachine;

    const uint64_t kCsrTvecAddr   = kTvecReg.at(kNewPM);
    const uint64_t kCsrEpcAddr    = kEpcReg.at(kNewPM);
    const uint64_t kCsrCauseAddr  = kCauseReg.at(kNewPM);
    const uint64_t kCstTvalAddr   = kTvalReg.at(kNewPM);
    const uint64_t kCsrStatusAddr = kTrapToSMode ? csr::kCsrSstatus : csr::kCsrMstatus;

    const uint64_t kStatus     = m_state.Read(kCsrStatusAddr);
    const uint64_t kCsrTvecVal = m_state.Read(kCsrTvecAddr);
    const uint64_t kTrapPc     = trap::GetTrapPC(kCsrTvecVal, kCauseBits.cause);

    SetPC(kTrapPc);
    m_state.Write(kCsrEpcAddr, epc);
    m_state.Write(kCsrCauseAddr, *reinterpret_cast<const uint64_t*>(&kCauseBits));
    m_state.Write(kCstTvalAddr, trap.m_val);

    if (kTrapToSMode) {
        auto kSsDesc = *reinterpret_cast<const csr::SstatusDesc*>(&kStatus);
        kSsDesc.spie = kSsDesc.sie;
        kSsDesc.sie  = 0;
        kSsDesc.spp  = static_cast<uint64_t>(kOriginPM);
        m_state.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&kSsDesc));
    } else {
        auto kMsDesc = *reinterpret_cast<const csr::MstatusDesc*>(&kStatus);
        kMsDesc.mpie = kMsDesc.mie;
        kMsDesc.mie  = 0;
        kMsDesc.mpp  = static_cast<uint64_t>(kOriginPM);
        m_state.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&kMsDesc));
    }

    SetPrivilegeMode(kNewPM);
}

void CPU::HandleInterrupt(const uint64_t inst_addr) {
    const uint64_t kMip        = m_state.Read(csr::kCsrMip);
    const uint64_t kMie        = m_state.Read(csr::kCsrMie);
    const uint64_t kMsVal      = m_state.Read(csr::kCsrMstatus);
    const uint64_t kMidelegVal = m_state.Read(csr::kCsrMideleg);

    const auto  kCurPM  = GetPrivilegeMode();
    const auto* kMsDesc = reinterpret_cast<const csr::MstatusDesc*>(&kMsVal);

    const uint64_t kInterruptBits = kMip & kMie;

    trap::Trap final_interrupt = {
        .m_trap_type = trap::TrapType::kNone,
        .m_val       = GetPC(),
    };

    using libs::util::InterruptBitsToTrap;
    using libs::util::TrapToMask;

    if (kCurPM == PrivilegeMode::kMachine) {
        const uint64_t kMmodeIntBits = kInterruptBits & (~kMidelegVal);
        if (kMmodeIntBits && kMsDesc->mie) {
            final_interrupt.m_trap_type = InterruptBitsToTrap(kMmodeIntBits);
        }
    } else {
        const trap::TrapType kInterruptType = InterruptBitsToTrap(kInterruptBits);
        if (TrapToMask(kInterruptType) & kMidelegVal) {
            if (kMsDesc->sie || kCurPM < PrivilegeMode::kSupervisor) {
                final_interrupt.m_trap_type = kInterruptType;
            }
        } else {
            if (kMsDesc->mie || kCurPM < PrivilegeMode::kMachine) {
                final_interrupt.m_trap_type = kInterruptType;
            }
        }
    }

    const uint64_t kCsrMipMask = TrapToMask(final_interrupt.m_trap_type);
    HandleTrap(final_interrupt, inst_addr);
    m_state.Write(csr::kCsrMip, kMip & (~kCsrMipMask));
    m_wfi = (kCsrMipMask != 0) ? false : m_wfi;
}

trap::Trap CPU::Fetch(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
    CHECK_MISALIGN_INSTRUCTION(addr, this);

    return m_mmu->VirtualFetch(addr, bytes, buffer);
}

trap::Trap CPU::Decode(const uint32_t word, int64_t& index) {
    int64_t inst_table_index = 0;

    // decode cache hit current instruction
    if (m_decode_cache.Get(word, inst_table_index)) {
        index = inst_table_index;
        return trap::kNoneTrap;
    }

    // decode cache miss, find the index in instruction table
    inst_table_index = 0;
    for (const auto& inst : instruction::kInstructionTable) {
        if ((word & inst.m_mask) == inst.m_data) {
            m_decode_cache.Set(word, inst_table_index);
            index = inst_table_index;
            return trap::kNoneTrap;
        }
        inst_table_index++;
    }

    return {
        .m_trap_type = trap::TrapType::kIllegalInstruction,
        .m_val       = word,
    };
}

trap::Trap CPU::TickOperate() {
    if (m_wfi) {
        const uint64_t mie = m_state.Read(csr::kCsrMie);
        const uint64_t mip = m_state.Read(csr::kCsrMip);
        if (mie & mip) {
            m_wfi = false;
        }
        return trap::kNoneTrap;
    }

    const uint64_t kInstructionAddr = GetPC();

    int64_t  index = 0;
    uint32_t word  = 0;

    // TODO
    const trap::Trap kFetchTrap = Fetch(kInstructionAddr, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&word));
    if (kFetchTrap.m_trap_type != trap::TrapType::kNone) {
        return kFetchTrap;
    }

    // TODO
    SetPC(kInstructionAddr + 4);

    const trap::Trap kDecodeTrap = Decode(word, index);
    if (kDecodeTrap.m_trap_type != trap::TrapType::kNone) {
        return kDecodeTrap;
    }

#ifdef DEBUG
    Disassemble(kInstructionAddr, word, index);
    uint64_t backup_reg[kGeneralPurposeRegNum] = { 0 };
    memcpy(backup_reg, m_reg, kGeneralPurposeRegNum * sizeof(uint64_t));
#endif

    const trap::Trap kExecTrap = instruction::kInstructionTable[index].Exec(this, word);

    m_reg[0] = 0;

#ifdef DEBUG
    gpr_change_record = 0;
    for (uint64_t i = 0; i < kGeneralPurposeRegNum; i++) {
        if (backup_reg[i] != m_reg[i]) {
            gpr_change_record |= (1 << i);
        }
    }
#endif

    return kExecTrap;
}

void CPU::Tick() {
    // pre exec
    m_state.Write(csr::kCsrMCycle, ++m_clock);

    const uint64_t   kEpc  = GetPC();
    const trap::Trap kTrap = TickOperate();

    HandleTrap(kTrap, kEpc);
    HandleInterrupt(GetPC());

    // post exec
    m_state.Write(csr::kCsrMinstret, ++m_instruction_count);
}

void CPU::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
    m_mmu->FlushTlb(vaddr, asid);
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
