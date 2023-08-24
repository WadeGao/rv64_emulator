#include "jit/jit.hh"

#include <cstdint>
#include <memory>

#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/decode.h"
#include "cpu/mmu.h"
#include "device/bus.h"
#include "device/dram.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

using rv64_emulator::cpu::decode::InstToken;
using rv64_emulator::cpu::decode::OpCode;

class JitTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running JIT test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running JIT test case...\n");
  }

  void SetUp() override {
    auto dram = std::make_unique<rv64_emulator::device::dram::DRAM>(128);

    auto bus = std::make_shared<rv64_emulator::device::bus::Bus>();
    bus->MountDevice({
        .base = kDramBaseAddr,
        .size = kDramSize,
        .dev = std::move(dram),
    });

    auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(bus);
    auto mmu = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
    auto cpu = std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));
    cpu_ = std::move(cpu);

    for (uint32_t i = 0; i < 32; i++) {
      cpu_->reg_file_.xregs[i] = i;
    }

    jit_ = std::make_unique<rv64_emulator::jit::JitEmitter>(cpu_.get());
  }

  void TearDown() override {}

  std::unique_ptr<rv64_emulator::cpu::CPU> cpu_;
  std::unique_ptr<rv64_emulator::jit::JitEmitter> jit_;
};

TEST_F(JitTest, Emit) {
  rv64_emulator::cpu::decode::DecodeInfo info(0, 0);

  info.op = OpCode::kReg;
  info.token = InstToken::MULHSU;

  jit_->EmitProlog();

  info.rd = 1;
  info.rs1 = 2;
  info.rs2 = 3;
  cpu_->reg_file_.xregs[2] = 0;
  cpu_->reg_file_.xregs[3] = 100;
  jit_->Emit(info);

  info.rd = 2;
  info.rs1 = 5;
  info.rs2 = 6;
  cpu_->reg_file_.xregs[5] = -5;
  cpu_->reg_file_.xregs[6] = 10;
  jit_->Emit(info);

  info.rd = 7;
  info.rs1 = 8;
  info.rs2 = 9;
  cpu_->reg_file_.xregs[8] = 5;
  cpu_->reg_file_.xregs[9] = -10;
  jit_->Emit(info);

  info.op = OpCode::kImm;
  info.token = InstToken::ADDI;
  info.rd = 10;
  info.rs1 = 11;
  info.imm = -111;
  jit_->Emit(info);

  info.op = OpCode::kReg;
  info.token = InstToken::SUB;
  info.rd = 12;
  info.rs1 = 13;
  info.rs2 = 14;
  jit_->Emit(info);

  info.op = OpCode::kRv32;
  info.token = InstToken::REMUW;
  info.rd = 15;
  info.rs1 = 16;
  info.rs2 = 17;
  cpu_->reg_file_.xregs[16] = 0xf1234567;
  cpu_->reg_file_.xregs[17] = 0;
  jit_->Emit(info);

  info.token = InstToken::ADDW;
  info.rd = 18;
  info.rs1 = 19;
  info.rs2 = 20;
  cpu_->reg_file_.xregs[19] = 0xefffffff;
  cpu_->reg_file_.xregs[20] = 0x1;
  jit_->Emit(info);

  info.op = OpCode::kJal;
  info.token = InstToken::JAL;
  info.rd = 19;
  info.pc = 0x1234;
  info.imm = 0x45678;
  info.size = 4;
  jit_->Emit(info);

  info.op = OpCode::kJalr;
  info.token = InstToken::JALR;
  info.rd = 20;
  info.rs1 = 21;
  info.pc = 0x1234;
  info.imm = 0x45678;
  info.size = 4;
  cpu_->reg_file_.xregs[info.rs1] = 0x1234567890abcdef;
  jit_->Emit(info);

  info.op = OpCode::kJalr;
  info.token = InstToken::JALR;
  info.rd = 3;
  info.rs1 = 4;
  info.pc = 0x1234;
  info.imm = 0xabcde;
  info.size = 4;
  cpu_->reg_file_.xregs[info.rs1] = 0x1234567890abcdef;
  jit_->Emit(info);

  jit_->EmitEpilog();

  rv64_emulator::jit::Func_t fn;
  auto err = jit_->rt_.add(&fn, &jit_->code_);

  fn();

  for (uint32_t i = 0; i < 32; i++) {
    fmt::print("x{}: {:#018x}\n", i, cpu_->reg_file_.xregs[i]);
  }
  fmt::print("pc: {:#018x}\n", cpu_->pc_);

  ASSERT_EQ(cpu_->reg_file_.xregs[1], 0);
  ASSERT_EQ(cpu_->reg_file_.xregs[2], UINT64_MAX);
  ASSERT_EQ(cpu_->reg_file_.xregs[7], 4);
  ASSERT_EQ(cpu_->reg_file_.xregs[10], 11 - 111);
  ASSERT_EQ(cpu_->reg_file_.xregs[12], -1);
  ASSERT_EQ(cpu_->reg_file_.xregs[15], 0xfffffffff1234567);
  ASSERT_EQ(cpu_->reg_file_.xregs[18], 0xfffffffff0000000);
  ASSERT_EQ(cpu_->reg_file_.xregs[19], 0x1234 + 4);
  ASSERT_EQ(cpu_->reg_file_.xregs[20], 0x1234 + 4);
  ASSERT_EQ(cpu_->pc_, 0x1234567890b68acc);
  ASSERT_EQ(cpu_->reg_file_.xregs[3], 0x1234 + 4);
}
