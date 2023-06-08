#pragma once
#include <cstdint>

#include "cpu/cpu.h"
#include "cpu/decode.h"
#include "cpu/trap.h"

namespace rv64_emulator::cpu::executor {

class Executor {
 private:
  CPU* cpu_;

  trap::Trap RegTypeExec(const decode::DecodeResDesc desc);
  trap::Trap ImmTypeExec(const decode::DecodeResDesc desc);
  trap::Trap LuiTypeExec(const decode::DecodeResDesc desc);
  trap::Trap BranchTypeExec(const decode::DecodeResDesc desc);
  trap::Trap StoreTypeExec(const decode::DecodeResDesc desc);
  trap::Trap LoadTypeExec(const decode::DecodeResDesc desc);
  trap::Trap SystemTypeExec(const decode::DecodeResDesc desc);
  trap::Trap AuipcTypeExec(const decode::DecodeResDesc desc);
  trap::Trap JalTypeExec(const decode::DecodeResDesc desc);
  trap::Trap JalrTypeExec(const decode::DecodeResDesc desc);
  trap::Trap Imm32TypeExec(const decode::DecodeResDesc desc);
  trap::Trap Rv32TypeExec(const decode::DecodeResDesc desc);

  trap::Trap CsrTypeExec(const decode::DecodeResDesc desc);

 public:
  explicit Executor(CPU* cpu);
  trap::Trap Exec(const decode::DecodeResDesc desc);
};

}  // namespace rv64_emulator::cpu::executor
