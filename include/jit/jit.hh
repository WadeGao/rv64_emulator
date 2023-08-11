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

  /**
   * @brief emit corresponding a64 reg instruction by token
   *
   * @param rd
   * @param rs1
   * @param rs2
   * @param token
   */
  void SelectA64RegInstruction(const asmjit::arm::Gp& rd,
                               const asmjit::arm::Gp& rs1,
                               const asmjit::arm::Gp& rs2,
                               cpu::decode::InstToken token);
};

}  // namespace rv64_emulator::jit