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
  info.rd = 1;
  info.rs1 = 2;
  info.rs2 = 3;
  cpu_->reg_file_.xregs[2] = 0;
  cpu_->reg_file_.xregs[3] = 100;

  jit_->EmitProlog();
  jit_->Emit(info);

  info.rd = 4;
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

  jit_->EmitEpilog();

  rv64_emulator::jit::Func_t fn;
  auto err = jit_->rt_.add(&fn, &jit_->code_);

  fn();

  for (uint32_t i = 0; i < 32; i++) {
    fmt::print("x{}: {}\n", i, cpu_->reg_file_.xregs[i]);
  }

  ASSERT_EQ(cpu_->reg_file_.xregs[1], 0);
  ASSERT_EQ(cpu_->reg_file_.xregs[4], UINT64_MAX);
  ASSERT_EQ(cpu_->reg_file_.xregs[7], 4);
}
