#include "cpu/cpu.h"

#include <cstdint>
#include <map>
#include <tuple>

#include "bus.h"
#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/instruction.h"
#include "cpu/trap.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "libs/arithmetic.hpp"
#include "libs/lru.hpp"
#include "libs/utils.h"
#include "mmu.h"

namespace rv64_emulator::cpu {

const std::map<PrivilegeMode, uint64_t> kCauseReg = {
    {PrivilegeMode::kMachine, csr::kCsrMcause},
    {PrivilegeMode::kSupervisor, csr::kCsrScause},
    {PrivilegeMode::kUser, csr::kCsrUcause},
};

const std::map<PrivilegeMode, uint64_t> kEpcReg = {
    {PrivilegeMode::kMachine, csr::kCsrMepc},
    {PrivilegeMode::kSupervisor, csr::kCsrSepc},
    {PrivilegeMode::kUser, csr::kCsrUepc},
};

const std::map<PrivilegeMode, uint64_t> kTvalReg = {
    {PrivilegeMode::kMachine, csr::kCsrMtval},
    {PrivilegeMode::kSupervisor, csr::kCsrStval},
    {PrivilegeMode::kUser, csr::kCsrUtval},
};

const std::map<PrivilegeMode, uint64_t> kTvecReg = {
    {PrivilegeMode::kMachine, csr::kCsrMtvec},
    {PrivilegeMode::kSupervisor, csr::kCsrStvec},
    {PrivilegeMode::kUser, csr::kCsrUtvec},
};

const std::map<PrivilegeMode, uint64_t> kStatusReg = {
    {PrivilegeMode::kMachine, csr::kCsrMstatus},
    {PrivilegeMode::kSupervisor, csr::kCsrSstatus},
    {PrivilegeMode::kUser, csr::kCsrUstatus},
};

const std::map<PrivilegeMode, uint64_t> kInterruptEnableReg = {
    {PrivilegeMode::kMachine, csr::kCsrMie},
    {PrivilegeMode::kSupervisor, csr::kCsrSie},
    {PrivilegeMode::kUser, csr::kCsrUie},
};

uint32_t gpr_change_record = 0;

CPU::CPU(std::unique_ptr<mmu::Mmu> mmu)
    : clock_(0),
      instret_(0),
      priv_mode_(PrivilegeMode::kMachine),
      pc_(0),
      mmu_(std::move(mmu)),
      dlb_(kDecodeCacheEntryNum) {
  static_assert(
      sizeof(float) == 4,
      "float is not 4 bytes, can't assure the bit width of floating point reg");
  mmu_->SetProcessor(this);
#ifdef DEBUG
  fmt::print("cpu init, mmu addr is {}\n", fmt::ptr(mmu_.get()));
#endif
}

void CPU::Reset() {
  clock_ = 0;
  instret_ = 0;
  priv_mode_ = PrivilegeMode::kMachine;
  pc_ = 0;

  memset(reg_, 0, sizeof(reg_));
  memset(freg_, 0, sizeof(freg_));

  mmu_->Reset();
  dlb_.Reset();
  state_.Reset();
}

trap::Trap CPU::Load(const uint64_t addr, const uint64_t bytes,
                     uint8_t* buffer) const {
  return mmu_->VirtualAddressLoad(addr, bytes, buffer);
}

trap::Trap CPU::Store(const uint64_t addr, const uint64_t bytes,
                      const uint8_t* buffer) {
  return mmu_->VirtualAddressStore(addr, bytes, buffer);
}

void CPU::SetReg(const uint64_t reg_num, const uint64_t val) {
  reg_[reg_num] = val;
}

uint64_t CPU::GetReg(const uint64_t reg_num) const { return reg_[reg_num]; }

void CPU::HandleTrap(const trap::Trap trap, const uint64_t epc) {
  if (trap.type == trap::TrapType::kNone) {
    return;
  }

  const PrivilegeMode kOriginPM = GetPrivilegeMode();

  const csr::CauseDesc kCauseBits = {
      .cause = trap::kTrapToCauseTable.at(trap.type),
      .interrupt = trap.type >= trap::TrapType::kUserSoftwareInterrupt,
  };

  const uint64_t kMxdeleg =
      state_.Read(kCauseBits.interrupt ? csr::kCsrMideleg : csr::kCsrMedeleg);
  const bool kTrapToSMode = (kOriginPM <= PrivilegeMode::kSupervisor &&
                             (kMxdeleg & (1 << kCauseBits.cause)));
  const PrivilegeMode kNewPM =
      kTrapToSMode ? PrivilegeMode::kSupervisor : PrivilegeMode::kMachine;

  const uint64_t kCsrTvecAddr = kTvecReg.at(kNewPM);
  const uint64_t kCsrEpcAddr = kEpcReg.at(kNewPM);
  const uint64_t kCsrCauseAddr = kCauseReg.at(kNewPM);
  const uint64_t kCstTvalAddr = kTvalReg.at(kNewPM);
  const uint64_t kCsrStatusAddr =
      kTrapToSMode ? csr::kCsrSstatus : csr::kCsrMstatus;

  const uint64_t kStatus = state_.Read(kCsrStatusAddr);
  const uint64_t kCsrTvecVal = state_.Read(kCsrTvecAddr);
  const uint64_t kTrapPc = trap::GetTrapPC(kCsrTvecVal, kCauseBits.cause);

  SetPC(kTrapPc);
  state_.Write(kCsrEpcAddr, epc);
  state_.Write(kCsrCauseAddr, *reinterpret_cast<const uint64_t*>(&kCauseBits));
  state_.Write(kCstTvalAddr, trap.val);

  if (kTrapToSMode) {
    auto kSsDesc = *reinterpret_cast<const csr::SstatusDesc*>(&kStatus);
    kSsDesc.spie = kSsDesc.sie;
    kSsDesc.sie = 0;
    kSsDesc.spp = static_cast<uint64_t>(kOriginPM);
    state_.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&kSsDesc));
  } else {
    auto kMsDesc = *reinterpret_cast<const csr::MstatusDesc*>(&kStatus);
    kMsDesc.mpie = kMsDesc.mie;
    kMsDesc.mie = 0;
    kMsDesc.mpp = static_cast<uint64_t>(kOriginPM);
    state_.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&kMsDesc));
  }

  SetPrivilegeMode(kNewPM);
}

void CPU::HandleInterrupt(const uint64_t inst_addr) {
  const uint64_t kMip = state_.Read(csr::kCsrMip);
  const uint64_t kMie = state_.Read(csr::kCsrMie);
  const uint64_t kMsVal = state_.Read(csr::kCsrMstatus);
  const uint64_t kMidelegVal = state_.Read(csr::kCsrMideleg);

  const auto kCurPM = GetPrivilegeMode();
  const auto* kMsDesc = reinterpret_cast<const csr::MstatusDesc*>(&kMsVal);

  const uint64_t kInterruptBits = kMip & kMie;

  trap::Trap final_interrupt = {
      .type = trap::TrapType::kNone,
      .val = GetPC(),
  };

  using libs::util::InterruptBitsToTrap;
  using libs::util::TrapToMask;

  if (kCurPM == PrivilegeMode::kMachine) {
    const uint64_t kMmodeIntBits = kInterruptBits & (~kMidelegVal);
    if (kMmodeIntBits && kMsDesc->mie) {
      final_interrupt.type = InterruptBitsToTrap(kMmodeIntBits);
    }
  } else {
    const trap::TrapType kInterruptType = InterruptBitsToTrap(kInterruptBits);
    if (TrapToMask(kInterruptType) & kMidelegVal) {
      if (kMsDesc->sie || kCurPM < PrivilegeMode::kSupervisor) {
        final_interrupt.type = kInterruptType;
      }
    } else {
      if (kMsDesc->mie || kCurPM < PrivilegeMode::kMachine) {
        final_interrupt.type = kInterruptType;
      }
    }
  }

  const uint64_t kCsrMipMask = TrapToMask(final_interrupt.type);
  HandleTrap(final_interrupt, inst_addr);
  state_.Write(csr::kCsrMip, kMip & (~kCsrMipMask));

  if (kCsrMipMask != 0) {
    state_.SetWfi(false);
  }
}

trap::Trap CPU::Fetch(const uint64_t addr, const uint64_t bytes,
                      uint8_t* buffer) {
  CHECK_MISALIGN_INSTRUCTION(addr, this);

  return mmu_->VirtualFetch(addr, bytes, buffer);
}

trap::Trap CPU::Decode(const uint32_t word, int64_t* index) {
  int64_t res = 0;

  // decode cache hit current instruction
  if (dlb_.Get(word, &res)) {
    *index = res;
    return trap::kNoneTrap;
  }

  // decode cache miss, find the index in instruction table
  res = 0;
  for (const auto& inst : instruction::kInstructionTable) {
    if ((word & inst.mask) == inst.signature) {
      dlb_.Set(word, res);
      *index = res;
      return trap::kNoneTrap;
    }
    res++;
  }

  return {
      .type = trap::TrapType::kIllegalInstruction,
      .val = word,
  };
}

trap::Trap CPU::TickOperate() {
  if (state_.GetWfi()) {
    const uint64_t mie = state_.Read(csr::kCsrMie);
    const uint64_t mip = state_.Read(csr::kCsrMip);
    if (mie & mip) {
      state_.SetWfi(false);
    }
    return trap::kNoneTrap;
  }

  const uint64_t kInstructionAddr = GetPC();

  int64_t index = 0;
  uint32_t word = 0;

  // TODO
  const trap::Trap kFetchTrap = Fetch(kInstructionAddr, sizeof(uint32_t),
                                      reinterpret_cast<uint8_t*>(&word));
  if (kFetchTrap.type != trap::TrapType::kNone) {
    return kFetchTrap;
  }

  // TODO
  SetPC(kInstructionAddr + 4);

  const trap::Trap kDecodeTrap = Decode(word, &index);
  if (kDecodeTrap.type != trap::TrapType::kNone) {
    return kDecodeTrap;
  }

#ifdef DEBUG
  Disassemble(kInstructionAddr, word, index);
  uint64_t backup_reg[kGeneralPurposeRegNum] = {0};
  memcpy(backup_reg, reg_, kGeneralPurposeRegNum * sizeof(uint64_t));
#endif

  const trap::Trap kExecTrap =
      instruction::kInstructionTable[index].Exec(this, word);

  reg_[0] = 0;

#ifdef DEBUG
  gpr_change_record = 0;
  for (uint64_t i = 0; i < kGeneralPurposeRegNum; i++) {
    if (backup_reg[i] != reg_[i]) {
      gpr_change_record |= (1 << i);
    }
  }
#endif

  return kExecTrap;
}

void CPU::Tick() {
  // pre exec
  state_.Write(csr::kCsrMCycle, ++clock_);

  const uint64_t kEpc = GetPC();
  const trap::Trap kTrap = TickOperate();

  HandleTrap(kTrap, kEpc);
  HandleInterrupt(GetPC());

  // post exec
  state_.Write(csr::kCsrMinstret, ++instret_);
}

void CPU::FlushTlb(const uint64_t vaddr, const uint64_t asid) {
  mmu_->FlushTlb(vaddr, asid);
}

void CPU::Disassemble(const uint64_t pc, const uint32_t word,
                      const int64_t index) const {
  fmt::print("{:#018x} {:#010x} {}\n", pc, word,
             instruction::kInstructionTable[index].name);
}

void CPU::DumpRegs() const {
  // Application Binary Interface registers
  const char* abi[] = {
      "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
      "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
      "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
  };

  constexpr int kBiasTable[4] = {0, 8, 16, 24};

  for (int i = 0; i < 8; i++) {
    for (const auto bias : kBiasTable) {
      const int index = i + bias;
      if (gpr_change_record & (1 << index)) {
        fmt::print(
            "      {:>28}",
            fmt::format(fmt::bg(fmt::color::green) | fmt::fg(fmt::color::red),
                        "{}: {:#018x}", abi[index], reg_[index]));
        //  fmt::bg(fmt::color::green) | fmt::fg(fmt::color::red)
      } else {
        fmt::print("{:>28}",
                   fmt::format("{}: {:#018x}", abi[index], reg_[index]));
      }
    }
    fmt::print("\n");
  }
}

}  // namespace rv64_emulator::cpu
