#pragma once

#include <cstdio>
#include <memory>

#include "asmjit/src/asmjit/arm/a64assembler.h"
#include "asmjit/src/asmjit/arm/armoperand.h"
#include "asmjit/src/asmjit/core/codeholder.h"
#include "asmjit/src/asmjit/core/jitruntime.h"
#include "cpu/cpu.h"
#include "cpu/decode.h"

namespace rv64_emulator::jit {

using Func_t = void (*)();

class JitEmitter {
 public:
  asmjit::JitRuntime rt_;
  asmjit::CodeHolder code_;
  std::unique_ptr<asmjit::a64::Assembler> as_;

  explicit JitEmitter(cpu::CPU* cpu);
  void EmitProlog();
  bool Emit(cpu::decode::DecodeInfo& info);
  void EmitEpilog();

 private:
  cpu::CPU* cpu_;
  void EmitMov(uint32_t rv_rd, uint32_t rv_rs);
  bool EmitReg(cpu::decode::DecodeInfo& info);
  bool EmitLui(cpu::decode::DecodeInfo& info);
  bool EmitAuipc(cpu::decode::DecodeInfo& info);
  bool EmitImm(cpu::decode::DecodeInfo& info);
  bool EmitReg32(cpu::decode::DecodeInfo& info);
  bool EmitJal(cpu::decode::DecodeInfo& info);
  bool EmitJalr(cpu::decode::DecodeInfo& info);

  void SelectA64RegInstruction(const asmjit::arm::GpX& rd,
                               const asmjit::arm::GpX& rs1,
                               const asmjit::arm::GpX& rs2,
                               cpu::decode::InstToken token);

  void SelectA64ImmInstruction(const asmjit::arm::GpX& rd,
                               const asmjit::arm::GpX& rs1, int32_t imm,
                               cpu::decode::InstToken token);

  void SelectA64Reg32Instruction(const asmjit::arm::GpW& wd,
                                 const asmjit::arm::GpW& ws1,
                                 const asmjit::arm::GpW& ws2,
                                 cpu::decode::InstToken token);
};

}  // namespace rv64_emulator::jit