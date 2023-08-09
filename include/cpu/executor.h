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
  trap::Trap Exec(decode::DecodeInfo info);

 private:
  CPU* cpu_;
  AmoReservation reservation_;

  trap::Trap RegTypeExec(decode::DecodeInfo info);
  trap::Trap ImmTypeExec(decode::DecodeInfo info);
  trap::Trap LuiTypeExec(decode::DecodeInfo info);
  trap::Trap BranchTypeExec(decode::DecodeInfo info);
  trap::Trap StoreTypeExec(decode::DecodeInfo info);
  trap::Trap LoadTypeExec(decode::DecodeInfo info);
  trap::Trap SystemTypeExec(decode::DecodeInfo info);
  trap::Trap AuipcTypeExec(decode::DecodeInfo info);
  trap::Trap JalTypeExec(decode::DecodeInfo info);
  trap::Trap JalrTypeExec(decode::DecodeInfo info);
  trap::Trap Imm32TypeExec(decode::DecodeInfo info);
  trap::Trap Rv32TypeExec(decode::DecodeInfo info);
  trap::Trap FenceTypeExec(decode::DecodeInfo info);
  trap::Trap CsrTypeExec(decode::DecodeInfo info);
  trap::Trap AmoTypeExec(decode::DecodeInfo info);

  trap::Trap ECallExec(decode::DecodeInfo info);
  trap::Trap SfenceVmaExec(decode::DecodeInfo info);
  trap::Trap MRetExec(decode::DecodeInfo info);
  trap::Trap SRetExec(decode::DecodeInfo info);

  // A Extensions
  trap::Trap AmoLrExec(decode::DecodeInfo info);
  trap::Trap AmoScExec(decode::DecodeInfo info);
};
}  // namespace executor
}  // namespace rv64_emulator::cpu
