#include "jit/jit.hh"

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

  info.op = rv64_emulator::cpu::decode::OpCode::kReg;
  info.rd = 3;
  info.rs1 = 3;
  info.rs2 = 4;
  info.token = InstToken::ADD;

  jit_->EmitProlog();
  jit_->Emit(info);  // x3 = 7

  info.rd = 10;
  info.rs1 = 11;
  info.rs2 = 24;
  jit_->Emit(info);  // x10 = 35

  info.token = InstToken::SLT;
  info.rd = 20;
  info.rs1 = 21;
  info.rs2 = 22;
  jit_->Emit(info);  // x20 = 1

  // info.token = InstToken::DIVU;
  // jit_->Emit(info);
  info.token = InstToken::REMU;
  info.rd = 1;
  info.rs1 = 13;
  info.rs2 = 5;
  jit_->Emit(info);  // x1 = 3

  info.rd = 2;
  info.rs1 = 28;
  info.rs2 = 10;     // x10 = 35
  jit_->Emit(info);  // x2 = 8

  jit_->EmitEpilog();

  rv64_emulator::jit::Func_t fn;
  auto err = jit_->rt_.add(&fn, &jit_->code_);

  fn();

  for (uint32_t i = 0; i < 32; i++) {
    fmt::print("x{}: {}\n", i, cpu_->reg_file_.xregs[i]);
  }

  ASSERT_EQ(cpu_->reg_file_.xregs[3], 3 + 4);
  ASSERT_EQ(cpu_->reg_file_.xregs[10], 11 + 24);
  ASSERT_EQ(cpu_->reg_file_.xregs[20], 1);
  ASSERT_EQ(cpu_->reg_file_.xregs[1], 3);
  ASSERT_EQ(cpu_->reg_file_.xregs[2], 28);
}
