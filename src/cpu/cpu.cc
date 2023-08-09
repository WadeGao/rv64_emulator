#include "cpu/cpu.h"

#include <cstdint>
#include <memory>
#include <tuple>

#include "conf.h"
#include "cpu/csr.h"
#include "cpu/decode.h"
#include "cpu/executor.h"
#include "cpu/mmu.h"
#include "cpu/trap.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "libs/arithmetic.h"
#include "libs/utils.h"

namespace rv64_emulator::cpu {

constexpr uint64_t kCauseReg[] = {
    [uint64_t(PrivilegeMode::kUser)] = csr::kCsrUcause,
    [uint64_t(PrivilegeMode::kSupervisor)] = csr::kCsrScause,
    [uint64_t(PrivilegeMode::kReserved)] = UINT64_MAX,
    [uint64_t(PrivilegeMode::kMachine)] = csr::kCsrMcause,
};

constexpr uint64_t kEpcReg[] = {
    [uint64_t(PrivilegeMode::kUser)] = csr::kCsrUepc,
    [uint64_t(PrivilegeMode::kSupervisor)] = csr::kCsrSepc,
    [uint64_t(PrivilegeMode::kReserved)] = UINT64_MAX,
    [uint64_t(PrivilegeMode::kMachine)] = csr::kCsrMepc,
};

constexpr uint64_t kTvalReg[] = {
    [uint64_t(PrivilegeMode::kUser)] = csr::kCsrUtval,
    [uint64_t(PrivilegeMode::kSupervisor)] = csr::kCsrStval,
    [uint64_t(PrivilegeMode::kReserved)] = UINT64_MAX,
    [uint64_t(PrivilegeMode::kMachine)] = csr::kCsrMtval,
};

constexpr uint64_t kTvecReg[] = {
    [uint64_t(PrivilegeMode::kUser)] = csr::kCsrUtvec,
    [uint64_t(PrivilegeMode::kSupervisor)] = csr::kCsrStvec,
    [uint64_t(PrivilegeMode::kReserved)] = UINT64_MAX,
    [uint64_t(PrivilegeMode::kMachine)] = csr::kCsrMtvec,
};

uint32_t hart_id = 0;

CPU::CPU(std::unique_ptr<mmu::Mmu> mmu)
    : clock_(0),
      instret_(0),
      pc_(0),
      hart_id_(hart_id++),
      priv_mode_(PrivilegeMode::kMachine),
      mmu_(std::move(mmu)) {
  reg_file_.xregs[10] = hart_id_;
  executor_ = std::make_unique<executor::Executor>();
  executor_->SetProcessor(this);
  mmu_->SetProcessor(this);
}

void CPU::Reset() {
  clock_ = 0;
  instret_ = 0;
  priv_mode_ = PrivilegeMode::kMachine;
  pc_ = 0;

  reg_file_.Reset();

  mmu_->Reset();
  // dlb_.Reset();
  state_.Reset();
}

trap::Trap CPU::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) const {
  return mmu_->Load(addr, bytes, buffer);
}

trap::Trap CPU::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  return mmu_->Store(addr, bytes, buffer);
}

void CPU::HandleTrap(trap::Trap trap, uint64_t epc) {
  if (trap.type == trap::TrapType::kNone) {
    return;
  }

  const PrivilegeMode kOriginPM = priv_mode_;

  const csr::CauseDesc kCauseBits = {
      .cause = trap::kTrapToCauseTable[static_cast<uint64_t>(trap.type)],
      .interrupt = trap.type >= trap::TrapType::kUserSoftwareInterrupt,
  };

  const uint64_t kMxdeleg =
      state_.Read(kCauseBits.interrupt ? csr::kCsrMideleg : csr::kCsrMedeleg);

  bool trap_to_s = false;
  if (priv_mode_ != PrivilegeMode::kMachine) {
    if ((kMxdeleg & (1 << kCauseBits.cause)) != 0) {
      trap_to_s = true;
    }
  }

  const auto kNewPM =
      trap_to_s ? PrivilegeMode::kSupervisor : PrivilegeMode::kMachine;

  const uint64_t kCsrTvecAddr = kTvecReg[static_cast<uint64_t>(kNewPM)];
  const uint64_t kCsrEpcAddr = kEpcReg[static_cast<uint64_t>(kNewPM)];
  const uint64_t kCsrCauseAddr = kCauseReg[static_cast<uint64_t>(kNewPM)];
  const uint64_t kCstTvalAddr = kTvalReg[static_cast<uint64_t>(kNewPM)];
  const uint64_t kCsrStatusAddr =
      trap_to_s ? csr::kCsrSstatus : csr::kCsrMstatus;

  const uint64_t kStatus = state_.Read(kCsrStatusAddr);
  const uint64_t kCsrTvecVal = state_.Read(kCsrTvecAddr);
  const uint64_t kTrapPc = trap::GetTrapPC(kCsrTvecVal, kCauseBits.cause);

  pc_ = kTrapPc;
  state_.Write(kCsrEpcAddr, epc);
  state_.Write(kCsrCauseAddr, *reinterpret_cast<const uint64_t*>(&kCauseBits));
  state_.Write(kCstTvalAddr, trap.val);

  if (trap_to_s) {
    auto ss_desc = *reinterpret_cast<const csr::SstatusDesc*>(&kStatus);
    ss_desc.spie = ss_desc.sie;
    ss_desc.sie = 0;
    ss_desc.spp = static_cast<uint64_t>(kOriginPM);
    state_.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&ss_desc));
  } else {
    auto ms_desc = *reinterpret_cast<const csr::MstatusDesc*>(&kStatus);
    ms_desc.mpie = ms_desc.mie;
    ms_desc.mie = 0;
    ms_desc.mpp = static_cast<uint64_t>(kOriginPM);
    state_.Write(kCsrStatusAddr, *reinterpret_cast<const uint64_t*>(&ms_desc));
  }

  priv_mode_ = kNewPM;
}

void CPU::HandleInterrupt(uint64_t inst_addr) {
  const uint64_t kMip = state_.Read(csr::kCsrMip);
  const uint64_t kMie = state_.Read(csr::kCsrMie);
  const uint64_t kMsVal = state_.Read(csr::kCsrMstatus);
  const uint64_t kMidelegVal = state_.Read(csr::kCsrMideleg);

  const auto kCurPM = priv_mode_;
  const auto kMsDesc = *reinterpret_cast<const csr::MstatusDesc*>(&kMsVal);

  const uint64_t kInterruptBits = kMip & kMie;

  trap::Trap final_interrupt = {
      .type = trap::TrapType::kNone,
      .val = pc_,
  };

  using libs::util::InterruptBitsToTrap;
  using libs::util::TrapToMask;

  if (kCurPM == PrivilegeMode::kMachine) {
    const uint64_t kMmodeIntBits = kInterruptBits & (~kMidelegVal);
    if (kMmodeIntBits && kMsDesc.mie) {
      final_interrupt.type = InterruptBitsToTrap(kMmodeIntBits);
    }
  } else {
    const trap::TrapType kInterruptType = InterruptBitsToTrap(kInterruptBits);
    if (TrapToMask(kInterruptType) & kMidelegVal) {
      if (kMsDesc.sie || kCurPM < PrivilegeMode::kSupervisor) {
        final_interrupt.type = kInterruptType;
      }
    } else {
      if (kMsDesc.mie || kCurPM < PrivilegeMode::kMachine) {
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

trap::Trap CPU::Fetch(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  const uint64_t kMisaVal = state_.Read(csr::kCsrMisa);
  if (!libs::util::CheckPcAlign(addr, kMisaVal)) {
    return {
        .type = trap::TrapType::kInstructionAddressMisaligned,
        .val = addr,
    };
  }

  return mmu_->Fetch(addr, bytes, buffer);
}

trap::Trap CPU::TickOperate() {
  if (state_.GetWfi()) {
    const uint64_t kMie = state_.Read(csr::kCsrMie);
    const uint64_t kMip = state_.Read(csr::kCsrMip);
    if (kMie & kMip) {
      state_.SetWfi(false);
    }
    return trap::kNoneTrap;
  }

  // fetch stage
  uint32_t word = 0;
  const trap::Trap kFetchTrap =
      Fetch(pc_, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&word));
  if (kFetchTrap.type != trap::TrapType::kNone) {
    return kFetchTrap;
  }

  // decode stage
  auto info = decode::DecodeInfo(word, pc_);
  pc_ += info.size;
  if (info.token == decode::InstToken::UNKNOWN) {
    return {.type = trap::TrapType::kIllegalInstruction, .val = word};
  }

  // execute stage
  const trap::Trap kExecTrap = executor_->Exec(info);
  reg_file_.xregs[0] = 0;
  return kExecTrap;
}

void CPU::Tick(bool meip, bool seip, bool msip, bool mtip, bool update) {
  // pre exec
  state_.Write(csr::kCsrMCycle, ++clock_);

  if (update) {
    uint64_t mip_val = state_.Read(csr::kCsrMip);
    auto* mip_desc = reinterpret_cast<csr::MipDesc*>(&mip_val);
    mip_desc->meip = meip;
    mip_desc->seip = seip;
    mip_desc->msip = msip;
    mip_desc->mtip = mtip;
    state_.Write(csr::kCsrMip, *reinterpret_cast<const uint64_t*>(mip_desc));
  }

  const uint64_t kEpc = pc_;
  const trap::Trap kTrap = TickOperate();

  HandleTrap(kTrap, kEpc);
  HandleInterrupt(pc_);

  // post exec
  state_.Write(csr::kCsrMinstret, ++instret_);
}

void CPU::Tick() { Tick(false, false, false, false, false); }

void CPU::FlushTlb(uint64_t vaddr, uint64_t asid) {
  mmu_->FlushTlb(vaddr, asid);
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
    for (const auto kBias : kBiasTable) {
      const int kIndex = i + kBias;
      fmt::print("{:>28}", fmt::format("{}: {:#018x}", abi[kIndex],
                                       reg_file_.xregs[kIndex]));
    }
    fmt::print("\n");
  }
}

uint64_t CPU::GetInstret() const { return instret_; }

}  // namespace rv64_emulator::cpu
