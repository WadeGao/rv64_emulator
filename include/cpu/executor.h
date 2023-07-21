#pragma once
#include <cstdint>

#include "cpu/cpu.h"
#include "cpu/decode.h"
#include "cpu/trap.h"

namespace rv64_emulator::cpu {
class CPU;
namespace executor {

using AmoReservation = struct AmoReservation {
  uint64_t addr;
  uint32_t size : 7;
  uint32_t valid : 1;
  uint32_t hart_id;
};

class Executor {
 private:
  CPU* cpu_;
  AmoReservation reservation_;

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
  trap::Trap FenceTypeExec(const decode::DecodeResDesc desc);
  trap::Trap CsrTypeExec(const decode::DecodeResDesc desc);
  trap::Trap AmoTypeExec(const decode::DecodeResDesc desc);

  trap::Trap ECallExec(const decode::DecodeResDesc desc);
  trap::Trap SfenceVmaExec(const decode::DecodeResDesc desc);
  trap::Trap MRetExec(const decode::DecodeResDesc desc);
  trap::Trap SRetExec(const decode::DecodeResDesc desc);

  // A Extensions
  trap::Trap AmoLrExec(const decode::DecodeResDesc desc);

 public:
  void SetProcessor(CPU* cpu);
  trap::Trap Exec(const decode::DecodeResDesc desc);
};
}  // namespace executor
}  // namespace rv64_emulator::cpu
