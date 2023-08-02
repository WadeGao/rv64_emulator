#include "cpu/cpu.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>

#include "conf.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "device/bus.h"
#include "device/clint.h"
#include "device/dram.h"
#include "elfio/elf_types.hpp"
#include "elfio/elfio.hpp"
#include "elfio/elfio_segment.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "gtest/gtest.h"
#include "libs/utils.h"
#include "mmu.h"

class CpuTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running CPU test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running CPU test case...\n");
  }

  void SetUp() override {
    poweron_.store(true);
    auto dram = std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize);
    auto clint = std::make_unique<rv64_emulator::device::clint::Clint>(1);
    raw_clint_ = clint.get();
    // auto tmp = raw_clint_;
    oscillator_ = std::thread([this]() {
      while (poweron_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        raw_clint_->Tick();
      }
    });

    auto bus = std::make_shared<rv64_emulator::device::bus::Bus>();
    bus->MountDevice({
        .base = kDramBaseAddr,
        .size = kDramSize,
        .dev = std::move(dram),
    });
    bus->MountDevice({
        .base = kClintBase,
        .size = kClintAddrSpaceRange,
        .dev = std::move(clint),
    });

    auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(bus);
    auto mmu = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
    auto cpu = std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));
    cpu_ = std::move(cpu);
  }

  void TearDown() override {
    poweron_.store(false);
    oscillator_.join();
  }

  std::atomic<bool> poweron_ = false;
  std::unique_ptr<rv64_emulator::cpu::CPU> cpu_;
  rv64_emulator::device::clint::Clint* raw_clint_;
  std::thread oscillator_;
};

constexpr uint64_t kMaxInstructions = 100000;
constexpr uint64_t kArbitrarilyHandlerVector = 0x100000;

static float GetMips(decltype(std::chrono::high_resolution_clock::now()) start,
                     const uint64_t insret_cnt) {
  const auto kEnd = std::chrono::high_resolution_clock::now();
  const auto kDurationUs =
      std::chrono::duration_cast<std::chrono::microseconds>(kEnd - start);
  const float kMips =
      static_cast<float>(insret_cnt) / static_cast<float>(kDurationUs.count());
  return kMips;
}

TEST_F(CpuTest, HandleTrap) {
  // write ecall instruction
  const uint32_t kEcallWord = 0x00000073;
  cpu_->Store(kDramBaseAddr, sizeof(uint32_t),
              reinterpret_cast<const uint8_t*>(&kEcallWord));
  cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMtvec,
                     kArbitrarilyHandlerVector);
  cpu_->pc_ = kDramBaseAddr;
  cpu_->Tick();

  ASSERT_EQ(kArbitrarilyHandlerVector, cpu_->pc_);

  const uint64_t kVal = cpu_->state_.Read(rv64_emulator::cpu::csr::kCsrMcause);
  ASSERT_EQ(rv64_emulator::cpu::trap::kTrapToCauseTable.at(
                rv64_emulator::cpu::trap::TrapType::kEnvironmentCallFromMMode),
            kVal);
}

TEST_F(CpuTest, HandleInterrupt) {
  for (const auto t : {
           rv64_emulator::cpu::trap::TrapType::kMachineExternalInterrupt,
           rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt,
           rv64_emulator::cpu::trap::TrapType::kMachineSoftwareInterrupt,
           rv64_emulator::cpu::trap::TrapType::kSupervisorExternalInterrupt,
           rv64_emulator::cpu::trap::TrapType::kSupervisorTimerInterrupt,
           rv64_emulator::cpu::trap::TrapType::kSupervisorSoftwareInterrupt,
       }) {
    // write "addi x0, x0, 1" instruction
    constexpr uint32_t kAddiWord = 0x00100013;
    cpu_->Store(kDramBaseAddr, sizeof(uint32_t),
                reinterpret_cast<const uint8_t*>(&kAddiWord));
    cpu_->pc_ = kDramBaseAddr;

    const uint64_t kMask = rv64_emulator::libs::util::TrapToMask(t);

    cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMie, kMask);
    cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMip, kMask);
    cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMtvec,
                       kArbitrarilyHandlerVector);

    cpu_->Tick();

    // mie is disabled
    ASSERT_EQ(kDramBaseAddr + 4, cpu_->pc_)
        << fmt::format("trap type: {}", static_cast<uint64_t>(t));

    // enable mie
    cpu_->pc_ = kDramBaseAddr;
    cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0b1000);
    cpu_->Tick();

    ASSERT_EQ(kArbitrarilyHandlerVector, cpu_->pc_);

    const rv64_emulator::cpu::csr::CauseDesc kCause = {
        .cause = rv64_emulator::cpu::trap::kTrapToCauseTable.at(t),
        .interrupt = static_cast<uint64_t>(
            t >= rv64_emulator::cpu::trap::TrapType::kUserSoftwareInterrupt),
    };

    const uint64_t kRealMCauseBits =
        cpu_->state_.Read(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_TRUE(kCause.interrupt);
    ASSERT_EQ(*reinterpret_cast<const uint64_t*>(&kCause), kRealMCauseBits);

    cpu_->Reset();
  }
}

TEST_F(CpuTest, Wfi) {
  // store wfi instruction
  const uint32_t kWfiWord = 0x10500073;
  cpu_->Store(kDramBaseAddr, sizeof(uint32_t),
              reinterpret_cast<const uint8_t*>(&kWfiWord));
  cpu_->pc_ = kDramBaseAddr;
  cpu_->Tick();
  ASSERT_EQ(kDramBaseAddr + 4, cpu_->pc_);

  for (int i = 0; i < 10; i++) {
    cpu_->Tick();
    ASSERT_EQ(kDramBaseAddr + 4, cpu_->pc_);
  }

  constexpr auto kTrap =
      rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt;
  const uint64_t kMtipMask = rv64_emulator::libs::util::TrapToMask(kTrap);
  cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMie, kMtipMask);
  cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMip, kMtipMask);
  cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0x8);
  cpu_->state_.Write(rv64_emulator::cpu::csr::kCsrMtvec, 0);
  cpu_->Tick();
  ASSERT_EQ(0, cpu_->pc_);
}

TEST_F(CpuTest, OfficalTests) {
  const char* kElfDir = "test/elf";

  std::filesystem::path p(kElfDir);
  ASSERT_TRUE(std::filesystem::exists(p)) << kElfDir << " not exists\n";
  ASSERT_TRUE(std::filesystem::directory_entry(kElfDir).is_directory())
      << kElfDir << " is not a directory\n";

  for (const auto& item : std::filesystem::directory_iterator(kElfDir)) {
    const auto& kFileName = fmt::format(
        "{}/{}", kElfDir, item.path().filename().filename().c_str());

    ELFIO::elfio reader;
    reader.load(kFileName);

    ELFIO::Elf64_Addr tohost_addr = 0;

    const bool kToHostSectionExist =
        rv64_emulator::libs::util::CheckSectionExist(reader, ".tohost\0",
                                                     &tohost_addr);

    ASSERT_TRUE(kToHostSectionExist) << "input file is not offical test case";

    cpu_->Reset();
    rv64_emulator::libs::util::LoadElf(reader, cpu_.get());

    const uint64_t kEntryAddr = reader.get_entry();
    cpu_->pc_ = kEntryAddr;

    const uint64_t kOriginInsrCnt = cpu_->instret_;
    auto start = std::chrono::high_resolution_clock::now();

    while (true) {
      cpu_->Tick(false, false, raw_clint_->MachineSoftwareIrq(0),
                 raw_clint_->MachineTimerIrq(0), true);
#ifdef DEBUG
      cpu_->DumpRegs();
#endif
      uint8_t val = UINT8_MAX;

      const bool kSucc =
          cpu_->mmu_->sv39_->bus_->Load(tohost_addr, sizeof(uint8_t), &val);
      ASSERT_TRUE(kSucc);

      if (val != 0) {
        const float kMips = GetMips(start, cpu_->instret_ - kOriginInsrCnt);
        EXPECT_EQ(val, 1) << fmt::format(
            fmt::fg(fmt::color::red),
            "Failed {} with .tohost section val = {}, MIPS = {:.2f}\n",
            kFileName, val, kMips);
        if (val == 1) {
          fmt::print("[ MIPS = {:#05.2f} ] Pass {}\n", kMips, kFileName);
        }
        break;
      }

      const uint64_t kCycles =
          cpu_->state_.Read(rv64_emulator::cpu::csr::kCsrMCycle);
      if (kCycles > kMaxInstructions) {
        EXPECT_TRUE(false) << fmt::format(fmt::fg(fmt::color::red),
                                          "{} timeout!", kFileName);
        break;
      }
    }
  }
}
