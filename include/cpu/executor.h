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
 public:
  void SetProcessor(CPU* cpu);
  trap::Trap Exec(decode::DecodeResDesc desc);

 private:
  CPU* cpu_;
  AmoReservation reservation_;

  trap::Trap RegTypeExec(decode::DecodeResDesc desc);
  trap::Trap ImmTypeExec(decode::DecodeResDesc desc);
  trap::Trap LuiTypeExec(decode::DecodeResDesc desc);
  trap::Trap BranchTypeExec(decode::DecodeResDesc desc);
  trap::Trap StoreTypeExec(decode::DecodeResDesc desc);
  trap::Trap LoadTypeExec(decode::DecodeResDesc desc);
  trap::Trap SystemTypeExec(decode::DecodeResDesc desc);
  trap::Trap AuipcTypeExec(decode::DecodeResDesc desc);
  trap::Trap JalTypeExec(decode::DecodeResDesc desc);
  trap::Trap JalrTypeExec(decode::DecodeResDesc desc);
  trap::Trap Imm32TypeExec(decode::DecodeResDesc desc);
  trap::Trap Rv32TypeExec(decode::DecodeResDesc desc);
  trap::Trap FenceTypeExec(decode::DecodeResDesc desc);
  trap::Trap CsrTypeExec(decode::DecodeResDesc desc);
  trap::Trap AmoTypeExec(decode::DecodeResDesc desc);

  trap::Trap ECallExec(decode::DecodeResDesc desc);
  trap::Trap SfenceVmaExec(decode::DecodeResDesc desc);
  trap::Trap MRetExec(decode::DecodeResDesc desc);
  trap::Trap SRetExec(decode::DecodeResDesc desc);

  // A Extensions
  trap::Trap AmoLrExec(decode::DecodeResDesc desc);
  trap::Trap AmoScExec(decode::DecodeResDesc desc);
};
}  // namespace executor
}  // namespace rv64_emulator::cpu
