#include "jit/jit.hh"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asmjit/src/asmjit/arm/a64assembler.h"
#include "asmjit/src/asmjit/arm/a64operand.h"
#include "asmjit/src/asmjit/arm/armoperand.h"
#include "asmjit/src/asmjit/core/archcommons.h"
#include "asmjit/src/asmjit/core/operand.h"
#include "cpu/cpu.h"
#include "cpu/trap.h"

namespace rv64_emulator::jit {

using cpu::decode::InstToken;

constexpr uint64_t kXregBias = offsetof(cpu::CPU, reg_file_.xregs);
constexpr uint64_t kPcBias = offsetof(cpu::CPU, pc_);
constexpr uint64_t kInstretBias = offsetof(cpu::CPU, instret_);

constexpr int8_t kRv64ToA64Map[32] = {
    [0] = static_cast<int32_t>(asmjit::arm::Gp::Id::kIdZr),
    [1] = 8,    // ra -> x8
    [2] = 18,   // sp -> x18
    [3] = -1,   // gp -> ?
    [4] = -1,   // tp -> ?
    [5] = 9,    // t0 -> x9
    [6] = 10,   // t1 -> x10
    [7] = 11,   // t2 -> x11
    [8] = 16,   // s0 -> x16
    [9] = 17,   // s1 -> x17
    [10] = 0,   // a0 -> x0
    [11] = 1,   // a1 -> x1
    [12] = 2,   // a2 -> x2
    [13] = 3,   // a3 -> x3
    [14] = 4,   // a4 -> x4
    [15] = 5,   // a5 -> x5
    [16] = 6,   // a6 -> x6
    [17] = 7,   // a7 -> x7
    [18] = 19,  // s2 -> x19
    [19] = 20,  // s3 -> x20
    [20] = 21,  // s4 -> x21
    [21] = 22,  // s5 -> x22
    [22] = 23,  // s6 -> x23
    [23] = 24,  // s7 -> x24
    [24] = 25,  // s8 -> x25
    [25] = 26,  // s9 -> x26
    [26] = 27,  // s10 -> x27
    [27] = 28,  // s11 -> x28
    [28] = 12,  // t3 -> x12
    [29] = 13,  // t4 -> x13
    [30] = 14,  // t5 -> x14
    [31] = 15,  // t6 -> x15
};

constexpr int8_t kA64ToRv64Map[32] = {
    [0] = 10,  [1] = 11,  [2] = 12,  [3] = 13,  [4] = 14,  [5] = 15,  [6] = 16,
    [7] = 17,  [8] = 1,   [9] = 5,   [10] = 6,  [11] = 7,  [12] = 28, [13] = 29,
    [14] = 30, [15] = 31, [16] = 8,  [17] = 9,  [18] = 2,  [19] = 18, [20] = 19,
    [21] = 20, [22] = 21, [23] = 22, [24] = 23, [25] = 24, [26] = 25, [27] = 26,
    [28] = 27, [29] = -1, [30] = -1, [31] = -1,
};

/*
+───────────+───────────+────────────────────────────────────+────────+
| Register  | ABI Name  | Description                        | Saver  |
+───────────+───────────+────────────────────────────────────+────────+
| x0        | zero      | Hard-wired zero                    |  ----  |
| x1        | ra        | Return address                     | Caller |
| x2        | sp        | Stack pointer                      | Callee |
| x3        | gp        | Global pointer                     |  ----  |
| x4        | tp        | Thread pointer                     |  ----  |
| x5        | t0        | Temporary/alternate link register  | Caller |
| x6 - 7    | t1 - 2    | Temporaries                        | Caller |
| x8        | s0/fp     | Saved register/frame pointer       | Callee |
| x9        | s1        | Saved registers                    | Callee |
| x10 - 11  | a0 - 1    | Function args/return values        | Caller |
| x12 - 17  | a2 - 7    | Function args                      | Caller |
| x18 - 27  | s2 - 11   | Saved registers                    | Callee |
| x28 - 31  | t3 - 6    | Temporaries                        | Caller |
+───────────+───────────+────────────────────────────────────+────────+
*/
int32_t __attribute__((always_inline)) A64Reg(uint32_t rv_reg) {
  // TODO(wade): AArch unused reg: x30(lr) sp
  // AArch x29(fp) is used to hold the addr of cpu
  return kRv64ToA64Map[rv_reg];
}

asmjit::arm::Mem __attribute__((always_inline)) XRegToMem(uint32_t rv_reg) {
  return asmjit::arm::ptr(asmjit::a64::regs::x29, kXregBias + (rv_reg << 3));
}

uint64_t MmuLoad(cpu::CPU* p, uint64_t addr, uint64_t size,
                 cpu::trap::Trap* t) {
  uint64_t val = 0;
  *t = p->Load(addr, size, reinterpret_cast<uint8_t*>(&val));
  return val;
}

void MmuStore(cpu::CPU* p, uint64_t addr, uint64_t size, uint64_t data,
              cpu::trap::Trap* t) {
  *t = p->Store(addr, size, reinterpret_cast<const uint8_t*>(&data));
}

void JitEmitter::SaveVolatileA64Reg() {
  for (int i = 0; i <= 18; i++) {
    as_->str(asmjit::arm::GpX(i), XRegToMem(kA64ToRv64Map[i]));
  }
}

void JitEmitter::RestoreVolatileA64Reg() {
  for (int i = 0; i <= 18; i++) {
    as_->ldr(asmjit::arm::GpX(i), XRegToMem(kA64ToRv64Map[i]));
  }
}

JitEmitter::JitEmitter(cpu::CPU* cpu)
    : cpu_(cpu), delta_instret_(0), exit_trap_(cpu::trap::kNoneTrap) {
  code_.init(rt_.environment());
  as_ = std::make_unique<asmjit::a64::Assembler>(&code_);
}

void JitEmitter::EmitProlog() {
  // alloc stack space, align to 16 bytes
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32 * sizeof(uint64_t));

  // save all regs
  for (uint32_t i = 0; i <= 28; i += 2) {
    as_->stp(asmjit::arm::GpX(i), asmjit::arm::GpX(i + 1),
             asmjit::arm::ptr(asmjit::a64::regs::sp, sizeof(uint64_t) * i));
  }
  as_->str(asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp, sizeof(uint64_t) * 30));

  // set up cpu base addr to x29
  as_->mov(asmjit::a64::regs::x29, cpu_);

  // map risc-v regs to arm64 reg
  for (uint32_t i = 0; i < 32; i++) {
    if (kRv64ToA64Map[i] < 0) {
      continue;
    }
    as_->ldr(asmjit::arm::GpX(kRv64ToA64Map[i]), XRegToMem(i));
  }
}

void JitEmitter::EmitEpilog() {
  as_->bind(epilog_tag_);

  CommitInstret();

  // write arm64 reg to risc-v reg
  for (uint32_t i = 0; i < 32; i++) {
    if (kRv64ToA64Map[i] < 0) {
      continue;
    }
    as_->str(asmjit::arm::GpX(kRv64ToA64Map[i]), XRegToMem(i));
  }

  // restore all regs
  for (uint32_t i = 0; i <= 28; i += 2) {
    as_->ldp(asmjit::arm::GpX(i), asmjit::arm::GpX(i + 1),
             asmjit::arm::ptr(asmjit::a64::regs::sp, sizeof(uint64_t) * i));
  }
  as_->ldr(asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp, sizeof(uint64_t) * 30));

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32 * sizeof(uint64_t));

  as_->ret(asmjit::a64::regs::x30);
}

void JitEmitter::EmitMov(uint32_t rv_rd, uint32_t rv_rs) {
  if (rv_rd == 0 || rv_rd == rv_rs) {
    return;
  }

  int32_t a64_rd = A64Reg(rv_rd);
  int32_t a64_rs = A64Reg(rv_rs);

  if (a64_rd >= 0 && a64_rs >= 0) {
    as_->mov(asmjit::arm::GpX(a64_rd), asmjit::arm::GpX(a64_rs));
  } else if (a64_rd >= 0 && a64_rs < 0) {
    as_->ldr(asmjit::arm::GpX(a64_rd), XRegToMem(rv_rs));
  } else if (a64_rd < 0 && a64_rs >= 0) {
    as_->str(asmjit::arm::GpX(a64_rs), XRegToMem(rv_rd));
  } else {
    // +─────────+  <- old sp
    // | padding |
    // +─────────+
    // |    x0   |
    // +─────────+  <- new sp

    // alloc stack space
    as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);
    // save x0
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
    // load rs from mem to x0
    as_->ldr(asmjit::a64::regs::x0, XRegToMem(rv_rs));
    // store rd
    as_->str(asmjit::a64::regs::x0, XRegToMem(rv_rd));
    // recover x0
    as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
    // stack balance
    as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);
  }
}

void JitEmitter::SelectA64RegInstruction(const asmjit::arm::GpX& rd,
                                         const asmjit::arm::GpX& rs1,
                                         const asmjit::arm::GpX& rs2,
                                         cpu::decode::InstToken token) {
  switch (token) {
    case InstToken::ADD:
      as_->add(rd, rs1, rs2);
      break;
    case InstToken::SUB:
      as_->sub(rd, rs1, rs2);
      break;
    case InstToken::SLL:
      as_->lsl(rd, rs1, rs2);
      break;
    case InstToken::SLT:
      as_->cmp(rs1, rs2);
      as_->cset(rd, asmjit::arm::CondCode::kLT);
      break;
    case InstToken::SLTU:
      as_->cmp(rs1, rs2);
      as_->cset(rd, asmjit::arm::CondCode::kLO);
      break;
    case InstToken::XOR:
      as_->eor(rd, rs1, rs2);
      break;
    case InstToken::SRL:
      as_->lsr(rd, rs1, rs2);
      break;
    case InstToken::SRA:
      as_->asr(rd, rs1, rs2);
      break;
    case InstToken::OR:
      as_->orr(rd, rs1, rs2);
      break;
    case InstToken::AND:
      as_->and_(rd, rs1, rs2);
      break;
    case InstToken::MUL:
      as_->mul(rd, rs1, rs2);
      break;
    case InstToken::MULH:
      as_->smulh(rd, rs1, rs2);
      break;
    case InstToken::MULHSU: {
      asmjit::Label unsigned_rs1 = as_->newLabel();
      asmjit::Label zero = as_->newLabel();
      asmjit::Label signed_rs1 = as_->newLabel();
      asmjit::Label end = as_->newLabel();

      as_->cbz(rs1, zero);
      as_->cbz(rs2, zero);
      as_->tbnz(rs1, 63, signed_rs1);

      as_->bind(unsigned_rs1);
      as_->umulh(rd, rs1, rs2);
      as_->b(end);

      as_->bind(signed_rs1);
      as_->neg(rd, rs1);
      as_->umulh(rd, rd, rs2);
      as_->mvn(rd, rd);
      as_->b(end);

      as_->bind(zero);
      as_->mov(rd, 0);

      as_->bind(end);
    } break;
    case InstToken::MULHU:
      as_->umulh(rd, rs1, rs2);
      break;
    case InstToken::DIV: {
      asmjit::Label zero_divisor = as_->newLabel();
      asmjit::Label end = as_->newLabel();
      as_->cbz(rs2, zero_divisor);
      as_->sdiv(rd, rs1, rs2);
      as_->b(end);
      as_->bind(zero_divisor);
      as_->mov(rd, UINT64_MAX);
      as_->bind(end);
    } break;
    case InstToken::DIVU: {
      asmjit::Label overflow = as_->newLabel();
      asmjit::Label end = as_->newLabel();
      as_->cbz(rs2, overflow);
      as_->udiv(rd, rs1, rs2);
      as_->b(end);
      as_->bind(overflow);
      as_->mov(rd, UINT64_MAX);
      as_->bind(end);
    } break;
    case InstToken::REM: {
      asmjit::Label zero_divisor = as_->newLabel();
      asmjit::Label end = as_->newLabel();
      as_->cbz(rs2, zero_divisor);
      as_->sdiv(rd, rs1, rs2);
      as_->msub(rd, rd, rs2, rs1);
      as_->b(end);
      as_->bind(zero_divisor);
      as_->mov(rd, rs1);
      as_->bind(end);
    } break;
    case InstToken::REMU: {
      asmjit::Label overflow = as_->newLabel();
      asmjit::Label end = as_->newLabel();
      as_->cbz(rs2, overflow);
      as_->udiv(rd, rs1, rs2);
      as_->msub(rd, rd, rs2, rs1);
      as_->b(end);
      as_->bind(overflow);
      as_->mov(rd, rs1);
      as_->bind(end);
    } break;
    default:
      break;
  }
}

void JitEmitter::SelectA64ImmInstruction(const asmjit::arm::GpX& rd,
                                         const asmjit::arm::GpX& rs1,
                                         int32_t imm,
                                         cpu::decode::InstToken token) {
  switch (token) {
    case InstToken::ADDI:
      as_->mov(rd, imm);
      as_->add(rd, rs1, rd);
      break;
    case InstToken::SLTI:
      as_->mov(rd, imm);
      as_->cmp(rs1, rd);
      as_->cset(rd, asmjit::arm::CondCode::kLT);
      break;
    case InstToken::SLTIU:
      as_->mov(rd, imm);
      as_->cmp(rs1, rd);
      as_->cset(rd, asmjit::arm::CondCode::kLO);
      break;
    case InstToken::XORI:
      as_->mov(rd, imm);
      as_->eor(rd, rs1, rd);
      break;
    case InstToken::ORI:
      as_->mov(rd, imm);
      as_->orr(rd, rs1, rd);
      break;
    case InstToken::ANDI:
      as_->mov(rd, imm);
      as_->and_(rd, rs1, rd);
      break;
    case InstToken::SLLI:
      as_->mov(rd, imm);
      as_->lsl(rd, rs1, rd);
      break;
    case InstToken::SRLI:
      as_->mov(rd, imm);
      as_->lsr(rd, rs1, rd);
      break;
    case InstToken::SRAI:
      as_->mov(rd, imm);
      as_->asr(rd, rs1, rd);
      break;
    default:
      break;
  }
}

bool JitEmitter::EmitReg(cpu::decode::DecodeInfo& info) {
  if (info.rd == 0) {
    // nop
    return true;
  }

  int32_t a64_rd = A64Reg(info.rd);
  int32_t a64_rs1 = A64Reg(info.rs1);
  int32_t a64_rs2 = A64Reg(info.rs2);

  // all in arm64 reg
  if (a64_rd >= 0 && a64_rs1 >= 0 && a64_rs2 >= 0) {
    SelectA64RegInstruction(asmjit::arm::GpX(a64_rd), asmjit::arm::GpX(a64_rs1),
                            asmjit::arm::GpX(a64_rs2), info.token);
    return true;
  }

  // +────────+  <- old sp
  // | padding|
  // +────────+
  // |   x2   |
  // +────────+
  // |   x1   |
  // +────────+
  // |   x0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  if (a64_rs2 < 0) {
    as_->str(asmjit::a64::regs::x2,
             asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
    as_->ldr(asmjit::a64::regs::x2, XRegToMem(info.rs2));
  }
  if (a64_rs1 < 0) {
    as_->str(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  }

  if (a64_rd < 0) {
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

    SelectA64RegInstruction(
        asmjit::a64::regs::x0,
        a64_rs1 < 0 ? asmjit::a64::regs::x1 : asmjit::arm::GpX(a64_rs1),
        a64_rs2 < 0 ? asmjit::a64::regs::x2 : asmjit::arm::GpX(a64_rs2),
        info.token);
    as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

    as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
  } else {
    SelectA64RegInstruction(
        asmjit::arm::GpX(a64_rd),
        a64_rs1 < 0 ? asmjit::a64::regs::x1 : asmjit::arm::GpX(a64_rs1),
        a64_rs2 < 0 ? asmjit::a64::regs::x2 : asmjit::arm::GpX(a64_rs2),
        info.token);
  }

  if (a64_rs1 < 0) {
    as_->ldr(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
  }
  if (a64_rs2 < 0) {
    as_->ldr(asmjit::a64::regs::x2,
             asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
  }

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  return true;
}

void JitEmitter::SelectA64Reg32Instruction(const asmjit::arm::GpW& wd,
                                           const asmjit::arm::GpW& ws1,
                                           const asmjit::arm::GpW& ws2,
                                           cpu::decode::InstToken token) {
  switch (token) {
    case InstToken::ADDW:
      as_->add(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::SUBW:
      as_->sub(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::SLLW:
      as_->lsl(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::SRLW:
      as_->lsr(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::SRAW:
      as_->asr(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::MULW:
      as_->mul(wd, ws1, ws2);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
      break;
    case InstToken::DIVW: {
      asmjit::Label zero_divsor = as_->newLabel();
      asmjit::Label end = as_->newLabel();

      as_->cbz(ws2, zero_divsor);

      as_->sdiv(wd, ws1, ws2);
      as_->b(end);

      as_->bind(zero_divsor);
      as_->mov(wd, UINT32_MAX);

      as_->bind(end);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
    } break;
    case InstToken::DIVUW: {
      asmjit::Label zero_divsor = as_->newLabel();
      asmjit::Label end = as_->newLabel();

      as_->cbz(ws2, zero_divsor);

      as_->udiv(wd, ws1, ws2);
      as_->b(end);

      as_->bind(zero_divsor);
      as_->mov(wd, UINT32_MAX);

      as_->bind(end);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
    } break;
    case InstToken::REMW: {
      asmjit::Label zero_divisor = as_->newLabel();
      asmjit::Label end = as_->newLabel();

      as_->cbz(ws2, zero_divisor);

      as_->sdiv(wd, ws1, ws2);
      as_->msub(wd, wd, ws2, ws1);
      as_->b(end);

      as_->bind(zero_divisor);
      as_->mov(wd, ws1);

      as_->bind(end);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
    } break;
    case InstToken::REMUW: {
      asmjit::Label overflow = as_->newLabel();
      asmjit::Label end = as_->newLabel();

      as_->cbz(ws2, overflow);

      as_->udiv(wd, ws1, ws2);
      as_->msub(wd, wd, ws2, ws1);
      as_->b(end);

      as_->bind(overflow);
      as_->mov(wd, ws1);

      as_->bind(end);
      as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
    } break;
    default:
      break;
  }
}

bool JitEmitter::EmitReg32(cpu::decode::DecodeInfo& info) {
  if (info.rd == 0) {
    // nop
    return true;
  }

  int32_t a64_wd = A64Reg(info.rd);
  int32_t a64_ws1 = A64Reg(info.rs1);
  int32_t a64_ws2 = A64Reg(info.rs2);

  // all in arm64 reg
  if (a64_wd >= 0 && a64_ws1 >= 0 && a64_ws2 >= 0) {
    SelectA64Reg32Instruction(asmjit::arm::GpW(a64_wd),
                              asmjit::arm::GpW(a64_ws1),
                              asmjit::arm::GpW(a64_ws2), info.token);
    return true;
  }

  // +────────+  <- old sp
  // | padding|
  // +────────+
  // |   w2   |
  // +────────+
  // |   w1   |
  // +────────+
  // |   w0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  if (a64_ws2 < 0) {
    as_->str(asmjit::a64::regs::x2,
             asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
    as_->ldr(asmjit::a64::regs::x2, XRegToMem(info.rs2));
  }
  if (a64_ws1 < 0) {
    as_->str(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  }

  if (a64_wd < 0) {
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

    SelectA64Reg32Instruction(
        asmjit::a64::regs::w0,
        a64_ws1 < 0 ? asmjit::a64::regs::w1 : asmjit::arm::GpW(a64_ws1),
        a64_ws2 < 0 ? asmjit::a64::regs::w2 : asmjit::arm::GpW(a64_ws2),
        info.token);
    as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

    as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
  } else {
    SelectA64Reg32Instruction(
        asmjit::arm::GpW(a64_wd),
        a64_ws1 < 0 ? asmjit::a64::regs::w1 : asmjit::arm::GpW(a64_ws1),
        a64_ws2 < 0 ? asmjit::a64::regs::w2 : asmjit::arm::GpW(a64_ws2),
        info.token);
  }

  if (a64_ws1 < 0) {
    as_->ldr(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
  }
  if (a64_ws2 < 0) {
    as_->ldr(asmjit::a64::regs::x2,
             asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
  }

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  return true;
}

bool JitEmitter::EmitLui(cpu::decode::DecodeInfo& info) {
  if (info.rd == 0) {
    // nop
    return true;
  }

  int32_t a64_rd = A64Reg(info.rd);
  if (a64_rd >= 0) {
    as_->mov(asmjit::arm::GpX(a64_rd), info.imm);
    return true;
  }

  // +────────+  <- old sp
  // | padding|
  // +────────+
  // |   x0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  // save old x0
  as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

  // overwrite x0 and save into risc-v rd
  as_->mov(asmjit::a64::regs::x0, info.imm);
  as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

  // restore x0
  as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  return true;
}

bool JitEmitter::EmitAuipc(cpu::decode::DecodeInfo& info) {
  cpu::decode::DecodeInfo copy = info;
  copy.imm += copy.pc;
  return EmitLui(copy);
}

bool JitEmitter::EmitImm(cpu::decode::DecodeInfo& info) {
  if (info.rd == 0) {
    // nop
    return true;
  }

  int32_t a64_rd = A64Reg(info.rd);
  int32_t a64_rs1 = A64Reg(info.rs1);

  // all in arm64 reg
  if (a64_rd >= 0 && a64_rs1 >= 0) {
    SelectA64ImmInstruction(asmjit::arm::GpX(a64_rd), asmjit::arm::GpX(a64_rs1),
                            info.imm, info.token);
    return true;
  }

  // +────────+  <- old sp
  // |   x1   |
  // +────────+
  // |   x0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  if (a64_rs1 < 0) {
    as_->str(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  }

  if (a64_rd < 0) {
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

    SelectA64ImmInstruction(
        asmjit::a64::regs::x0,
        a64_rs1 < 0 ? asmjit::a64::regs::x1 : asmjit::arm::GpX(a64_rs1),
        info.imm, info.token);
    as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

    as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
  } else {
    SelectA64ImmInstruction(
        asmjit::arm::GpX(a64_rd),
        a64_rs1 < 0 ? asmjit::a64::regs::x1 : asmjit::arm::GpX(a64_rs1),
        info.imm, info.token);
  }

  if (a64_rs1 < 0) {
    as_->ldr(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
  }

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  return true;
}

bool JitEmitter::EmitJal(cpu::decode::DecodeInfo& info) {
  int32_t a64_rd = A64Reg(info.rd);
  const uint64_t kNewPC = (int64_t)info.pc + info.imm;

  if (a64_rd >= 0) {
    auto gpx_rd = asmjit::arm::GpX(a64_rd);

    // update pc
    as_->mov(gpx_rd, kNewPC);
    as_->str(gpx_rd, asmjit::arm::ptr(asmjit::a64::regs::x29, kPcBias));

    // save link addr
    as_->mov(gpx_rd, info.pc + info.size);

    return true;
  }

  // +────────+  <- old sp
  // | padding|
  // +────────+
  // |   x0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  // save old x0
  as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

  // update pc
  as_->mov(asmjit::a64::regs::x0, kNewPC);
  as_->str(asmjit::a64::regs::x0,
           asmjit::arm::ptr(asmjit::a64::regs::x29, kPcBias));

  // store link addr
  as_->mov(asmjit::a64::regs::x0, info.pc + info.size);
  as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

  // restore x0
  as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  return true;
}

bool JitEmitter::EmitJalr(cpu::decode::DecodeInfo& info) {
  int32_t a64_rd = A64Reg(info.rd);
  int32_t a64_rs1 = A64Reg(info.rs1);

  if (a64_rd >= 0 && a64_rs1 >= 0) {
    auto gpx_rd = asmjit::arm::GpX(a64_rd);
    auto gpx_rs1 = asmjit::arm::GpX(a64_rs1);

    // calc new pc
    as_->mov(gpx_rd, info.imm);
    as_->add(gpx_rd, gpx_rd, gpx_rs1);
    as_->and_(gpx_rd, gpx_rd, 0xfffffffffffffffe);

    // save new pc
    as_->str(gpx_rd, asmjit::arm::ptr(asmjit::a64::regs::x29, kPcBias));

    // save link addr
    as_->mov(gpx_rd, info.pc + info.size);

    return true;
  }

  // +────────+ <- old sp
  // |   x1   |
  // +────────+
  // |   x0   |
  // +────────+ <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  if (a64_rs1 < 0) {
    as_->str(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  }
  if (a64_rd < 0) {
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
    as_->ldr(asmjit::a64::regs::x0, XRegToMem(info.rd));
  }

  asmjit::arm::GpX gpx_rd =
      a64_rd < 0 ? asmjit::a64::regs::x0 : asmjit::arm::GpX(a64_rd);
  asmjit::arm::GpX gpx_rs1 =
      a64_rs1 < 0 ? asmjit::a64::regs::x1 : asmjit::arm::GpX(a64_rs1);

  as_->mov(gpx_rd, info.imm);
  as_->add(gpx_rd, gpx_rd, gpx_rs1);
  as_->and_(gpx_rd, gpx_rd, 0xfffffffffffffffe);

  as_->str(gpx_rd, asmjit::arm::ptr(asmjit::a64::regs::x29, kPcBias));

  as_->mov(gpx_rd, info.pc + info.size);

  if (a64_rd < 0) {
    as_->str(gpx_rd, XRegToMem(info.rd));
    as_->ldr(gpx_rd, asmjit::arm::ptr(asmjit::a64::regs::sp));
  }
  if (a64_rs1 < 0) {
    as_->ldr(gpx_rs1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
  }

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  return true;
}

void JitEmitter::SelectA64Imm32Instruction(const asmjit::arm::GpW& wd,
                                           const asmjit::arm::GpW& ws1,
                                           int32_t imm,
                                           cpu::decode::InstToken token) {
  switch (token) {
    case InstToken::ADDIW:
      as_->mov(wd, imm);
      as_->add(wd, ws1, wd);
      break;
    case InstToken::SLLIW:
      as_->mov(wd, imm);
      as_->lsl(wd, ws1, wd);
      break;
    case InstToken::SRLIW:
      as_->mov(wd, imm);
      as_->lsr(wd, ws1, wd);
      break;
    case InstToken::SRAIW:
      as_->mov(wd, imm);
      as_->asr(wd, ws1, wd);
      break;
    default:
      return;
  }

  as_->sxtw(asmjit::arm::GpX(wd.id()), wd);
}

bool JitEmitter::EmitImm32(cpu::decode::DecodeInfo& info) {
  if (info.rd == 0) {
    return true;
  }

  int32_t a64_rd = A64Reg(info.rd);
  int32_t a64_rs1 = A64Reg(info.rs1);

  // all in arm64 reg
  if (a64_rd >= 0 && a64_rs1 >= 0) {
    SelectA64Imm32Instruction(asmjit::arm::GpW(a64_rd),
                              asmjit::arm::GpW(a64_rs1), info.imm, info.token);
    return true;
  }

  // +────────+  <- old sp
  // |   x1   |
  // +────────+
  // |   x0   |
  // +────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  if (a64_rs1 < 0) {
    as_->str(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  }

  if (a64_rd < 0) {
    as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));

    SelectA64Imm32Instruction(
        asmjit::a64::regs::w0,
        a64_rs1 < 0 ? asmjit::a64::regs::w1 : asmjit::arm::GpW(a64_rs1),
        info.imm, info.token);
    as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));

    as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp));
  } else {
    SelectA64Imm32Instruction(
        asmjit::arm::GpW(a64_rd),
        a64_rs1 < 0 ? asmjit::a64::regs::w1 : asmjit::arm::GpW(a64_rs1),
        info.imm, info.token);
  }

  if (a64_rs1 < 0) {
    as_->ldr(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 8));
  }

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  return true;
}

void JitEmitter::CommitInstret() {
  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  // save x0 x1
  as_->stp(asmjit::a64::regs::x0, asmjit::a64::regs::x1,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // prepare operands
  as_->mov(asmjit::a64::regs::x0, delta_instret_);
  as_->ldr(asmjit::a64::regs::x1,
           asmjit::arm::ptr(asmjit::a64::regs::x29, kInstretBias));
  as_->add(asmjit::a64::regs::x0, asmjit::a64::regs::x0, asmjit::a64::regs::x1);

  // save new instret
  as_->str(asmjit::a64::regs::x0,
           asmjit::arm::ptr(asmjit::a64::regs::x29, kInstretBias));

  // restore x0 x1
  as_->ldp(asmjit::a64::regs::x0, asmjit::a64::regs::x1,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 16);

  delta_instret_ = 0;
}

void JitEmitter::SelectSignExtendInstruction(cpu::decode::InstToken token,
                                             const asmjit::arm::GpX& rd,
                                             const asmjit::arm::GpW& ws) {
  switch (token) {
    case InstToken::LB:
      as_->sxtb(rd, ws);
      break;
    case InstToken::LH:
      as_->sxth(rd, ws);
      break;
    case InstToken::LW:
      as_->sxtw(rd, ws);
      break;
    default:  // LD LBU LHU LWU and others
      break;
  }
}

bool JitEmitter::EmitLoad(cpu::decode::DecodeInfo& info) {
  asmjit::Label fail = as_->newLabel();
  asmjit::Label end = as_->newLabel();

  int32_t a64_rd = A64Reg(info.rd);
  int32_t a64_rs1 = A64Reg(info.rs1);

  SaveVolatileA64Reg();
  // +───────────────+  <- old sp
  // |    mem val    |
  // +───────────────+
  // | type == kNone |
  // +───────────────+
  // |      x30      |
  // +───────────────+
  // |      x29      |
  // +───────────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  // save x29 x30
  as_->stp(asmjit::a64::regs::x29, asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // cpu*, addr, size, trap*

  // prepare addr first, because x0 may be overwrite if prepare cpu* first
  if (a64_rs1 < 0) {
    as_->ldr(asmjit::a64::regs::x1, XRegToMem(info.rs1));
  } else {
    as_->mov(asmjit::a64::regs::x1, asmjit::arm::GpX(a64_rs1));
  }
  as_->mov(asmjit::a64::regs::x2, info.imm);
  as_->add(asmjit::a64::regs::x1, asmjit::a64::regs::x1, asmjit::a64::regs::x2);

  // prepare first arg
  as_->mov(asmjit::a64::regs::x0, asmjit::a64::regs::x29);
  // prepare third arg
  as_->mov(asmjit::a64::regs::x2, info.mem_size);
  // prepare forth arg
  as_->mov(asmjit::a64::regs::x3, &exit_trap_);

  // use x4 for blr, rather bl
  // relative addr range of bl: 128MB
  as_->mov(asmjit::a64::regs::x4, MmuLoad);
  as_->blr(asmjit::a64::regs::x4);

  // store mem val to stack
  as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp, 24));

  // tell if trap.type == kNone
  as_->mov(asmjit::a64::regs::x0, &exit_trap_);
  as_->ldr(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::x0));
  as_->sub(asmjit::a64::regs::w0, asmjit::a64::regs::w0,
           cpu::trap::TrapType::kNone);
  as_->str(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::sp, 16));

  // now x0 - x18, fp lr not changed compared with the beginning
  RestoreVolatileA64Reg();
  as_->ldp(asmjit::a64::regs::x29, asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // load (traptype - kNone) into w0
  as_->ldr(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
  as_->cbnz(asmjit::a64::regs::w0, fail);

  // load mem val into a64 x0
  as_->ldr(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp, 24));
  // sign extend to int64_t
  SelectSignExtendInstruction(info.token, asmjit::a64::regs::x0,
                              asmjit::a64::regs::w0);
  // write through into rv reg
  as_->str(asmjit::a64::regs::x0, XRegToMem(info.rd));
  // now recover x0
  as_->ldr(asmjit::a64::regs::x0, XRegToMem(kA64ToRv64Map[0]));
  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);

  // map modified rv64 reg to a64 reg
  if (a64_rd >= 0) {
    as_->ldr(asmjit::arm::GpX(a64_rd), XRegToMem(info.rd));
  }
  as_->b(end);

  // tag fail
  as_->bind(fail);
  // now recover x0
  as_->ldr(asmjit::a64::regs::x0, XRegToMem(kA64ToRv64Map[0]));
  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 32);
  as_->b(epilog_tag_);

  as_->bind(end);
  return true;
}

bool JitEmitter::EmitStore(cpu::decode::DecodeInfo& info) {
  asmjit::Label fail = as_->newLabel();
  asmjit::Label end = as_->newLabel();

  int32_t a64_rs1 = A64Reg(info.rs1);
  int32_t a64_rs2 = A64Reg(info.rs2);

  SaveVolatileA64Reg();
  // +───────────────+  <- old sp
  // |    padding    |
  // +───────────────+
  // | type == kNone |
  // +───────────────+
  // |      data     |
  // +───────────────+
  // |      addr     |
  // +───────────────+
  // |      x30      |
  // +───────────────+
  // |      x29      |
  // +───────────────+  <- new sp

  // alloc stack space
  as_->sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 48);

  // save x29 x30
  as_->stp(asmjit::a64::regs::x29, asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // prepare data and save it on stack
  if (a64_rs2 < 0) {
    as_->ldr(asmjit::a64::regs::x0, XRegToMem(info.rs2));
    as_->str(asmjit::a64::regs::x0,
             asmjit::arm::ptr(asmjit::a64::regs::sp, 24));
    as_->ldr(asmjit::a64::regs::x0, XRegToMem(kA64ToRv64Map[0]));
  } else {
    as_->str(asmjit::arm::GpX(a64_rs2),
             asmjit::arm::ptr(asmjit::a64::regs::sp, 24));
  }

  // prepare addr and save it on stack
  if (a64_rs1 < 0) {
    as_->ldr(asmjit::a64::regs::x0, XRegToMem(info.rs1));
    as_->mov(asmjit::a64::regs::x1, info.imm);
    as_->add(asmjit::a64::regs::x0, asmjit::a64::regs::x0,
             asmjit::a64::regs::x1);
  } else {
    as_->mov(asmjit::a64::regs::x0, info.imm);
    as_->add(asmjit::a64::regs::x0, asmjit::a64::regs::x0,
             asmjit::arm::GpX(a64_rs1));
  }
  as_->str(asmjit::a64::regs::x0, asmjit::arm::ptr(asmjit::a64::regs::sp, 16));

  // cpu*, addr, size, data, trap*
  // prepare first arg
  as_->mov(asmjit::a64::regs::x0, asmjit::a64::regs::x29);
  // prepare second arg
  as_->ldr(asmjit::a64::regs::x1, asmjit::arm::ptr(asmjit::a64::regs::sp, 16));
  // prepare third arg
  as_->mov(asmjit::a64::regs::x2, info.mem_size);
  // prepare fourth arg
  as_->ldr(asmjit::a64::regs::x3, asmjit::arm::ptr(asmjit::a64::regs::sp, 24));
  // prepare fifth arg
  as_->mov(asmjit::a64::regs::x4, &exit_trap_);

  // prepare function addr and call
  as_->mov(asmjit::a64::regs::x5, MmuStore);
  as_->blr(asmjit::a64::regs::x5);

  // tell if trap.type == kNone
  as_->mov(asmjit::a64::regs::x0, &exit_trap_);
  as_->ldr(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::x0));
  as_->sub(asmjit::a64::regs::w0, asmjit::a64::regs::w0,
           cpu::trap::TrapType::kNone);
  as_->str(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::sp, 32));

  // now x0 - x18, fp lr not changed compared with the beginning
  RestoreVolatileA64Reg();
  as_->ldp(asmjit::a64::regs::x29, asmjit::a64::regs::x30,
           asmjit::arm::ptr(asmjit::a64::regs::sp));

  // load (traptype - kNone) into w0
  as_->ldr(asmjit::a64::regs::w0, asmjit::arm::ptr(asmjit::a64::regs::sp, 32));
  as_->cbnz(asmjit::a64::regs::w0, fail);

  // now recover x0
  as_->ldr(asmjit::a64::regs::x0, XRegToMem(kA64ToRv64Map[0]));
  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 48);
  as_->b(end);

  // tag fail
  as_->bind(fail);
  // now recover x0
  as_->ldr(asmjit::a64::regs::x0, XRegToMem(kA64ToRv64Map[0]));
  // free stack space
  as_->add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, 48);
  as_->b(epilog_tag_);

  as_->bind(end);
  return true;
}

bool JitEmitter::Emit(cpu::decode::DecodeInfo& info) {
  if (info.op == cpu::decode::OpCode::kUndef ||
      info.token == InstToken::UNKNOWN) {
    return false;
  }

  bool ret = false;
  switch (info.op) {
    case cpu::decode::OpCode::kReg:
      ret = EmitReg(info);
      break;
    case cpu::decode::OpCode::kLui:
      ret = EmitLui(info);
      break;
    case cpu::decode::OpCode::kAuipc:
      ret = EmitAuipc(info);
      break;
    case cpu::decode::OpCode::kImm:
      ret = EmitImm(info);
      break;
    case cpu::decode::OpCode::kRv32:
      ret = EmitReg32(info);
      break;
    case cpu::decode::OpCode::kJal:
      ret = EmitJal(info);
      break;
    case cpu::decode::OpCode::kJalr:
      ret = EmitJalr(info);
      break;
    case cpu::decode::OpCode::kImm32:
      ret = EmitImm32(info);
      break;
    case cpu::decode::OpCode::kLoad:
      ret = EmitLoad(info);
      break;
    case cpu::decode::OpCode::kStore:
      ret = EmitStore(info);
      break;
    default:
      break;
  }
  ++delta_instret_;
  return ret;
}

}  // namespace rv64_emulator::jit
