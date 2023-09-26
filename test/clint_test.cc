#include "device/clint.h"

#include <cstdint>
#include <memory>
#include <random>
#include <utility>

#include "fmt/core.h"
#include "gtest/gtest.h"
#include "libs/utils.h"

using rv64_emulator::device::clint::Clint;
using rv64_emulator::libs::util::RandomGenerator;

class ClintTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running PLIC test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running PLIC test case...\n");
  }

  void SetUp() override {
    auto clint = std::make_unique<Clint>(2);
    clint_ = std::move(clint);
  }
  void TearDown() override {}

  std::unique_ptr<Clint> clint_;
};

RandomGenerator<uint64_t> rg(0, UINT64_MAX);

TEST_F(ClintTest, LoadMsip) {
  clint_->msip_[0] = rg.Get();
  clint_->msip_[1] = rg.Get();

  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMsipBase;
  uint16_t res = UINT16_MAX;
  ASSERT_TRUE(
      clint_->Load(kAddrBase, sizeof(res), reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(res, clint_->msip_[0] & 0xffff);

  ASSERT_TRUE(clint_->Load(kAddrBase + 2, sizeof(res),
                           reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(res, clint_->msip_[0] >> (sizeof(res) * 8));

  ASSERT_TRUE(clint_->Load(kAddrBase + 3, sizeof(res),
                           reinterpret_cast<uint8_t*>(&res)));

  // trans msip of different harts load
  // | 0 - 1 - 2 - 3 | 0 - 1 - 2 - 3 |
  // |             ^   ^             |
  // |     msip0     |     msip1     |
  ASSERT_EQ(res, ((clint_->msip_[0] >> 24) & 0xff) |
                     ((clint_->msip_[1] & 0xff) << 8));

  // out of range
  ASSERT_FALSE(clint_->Load(kAddrBase + 7, sizeof(res),
                            reinterpret_cast<uint8_t*>(&res)));
}

TEST_F(ClintTest, LoadMtimeCmp) {
  clint_->mtimecmp_[0] = rg.Get();
  clint_->mtimecmp_[1] = rg.Get();

  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMtimeCmpBase;
  uint64_t res = UINT64_MAX;
  ASSERT_TRUE(
      clint_->Load(kAddrBase, sizeof(res), reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(res, clint_->mtimecmp_[0]);

  // trans mtimecmp of different harts load
  ASSERT_TRUE(clint_->Load(kAddrBase + 4, sizeof(res),
                           reinterpret_cast<uint8_t*>(&res)));

  ASSERT_EQ(res, (clint_->mtimecmp_[0] >> 32) |
                     ((clint_->mtimecmp_[1] & 0xffffffff) << 32));

  // out of range
  ASSERT_FALSE(clint_->Load(kAddrBase + 8 * clint_->harts_ - 1, sizeof(res),
                            reinterpret_cast<uint8_t*>(&res)));
}

TEST_F(ClintTest, LoadMtime) {
  const uint64_t kRandomNumber = rg.Get();
  clint_->mtime_ = kRandomNumber;

  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMtimeBase;

  uint64_t res = UINT64_MAX;
  ASSERT_TRUE(
      clint_->Load(kAddrBase, sizeof(res), reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(res, kRandomNumber);

  // out of range
  ASSERT_FALSE(clint_->Load(kAddrBase + sizeof(res) - 1, sizeof(res),
                            reinterpret_cast<uint8_t*>(&res)));
}

TEST_F(ClintTest, StoreMsip) {
  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMsipBase;

  for (uint64_t i = 0; i < 20; i++) {
    const uint32_t kMsipVal = rg.Get(0, UINT32_MAX);
    ASSERT_TRUE(clint_->Store(kAddrBase, sizeof(kMsipVal),
                              reinterpret_cast<const uint8_t*>(&kMsipVal)));
    ASSERT_EQ(kMsipVal & 1, clint_->msip_[0]);
  }

  constexpr uint64_t kData = 0x00112231ffee00fd;
  ASSERT_TRUE(clint_->Store(kAddrBase, sizeof(kData),
                            reinterpret_cast<const uint8_t*>(&kData)));
  ASSERT_EQ(1, clint_->msip_[0]);
  ASSERT_EQ(1, clint_->msip_[1]);

  ASSERT_FALSE(clint_->Store(kAddrBase + 4 * clint_->harts_ - 1, sizeof(kData),
                             reinterpret_cast<const uint8_t*>(&kData)));
}

TEST_F(ClintTest, StoreMtimeCmp) {
  const uint64_t kMtimeCmpVal[2] = {rg.Get(), rg.Get()};
  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMtimeCmpBase;

  for (uint64_t i = 0; i < clint_->harts_; i++) {
    const uint64_t kVal = kMtimeCmpVal[i];
    ASSERT_TRUE(clint_->Store(kAddrBase + i * 8, sizeof(kVal),
                              reinterpret_cast<const uint8_t*>(&kVal)));
    ASSERT_EQ(kVal, clint_->mtimecmp_[i]);
  }

  // trans mtime of different harts store

  const uint64_t kRandomVal = rg.Get();
  ASSERT_TRUE(clint_->Store(kAddrBase + 4, sizeof(kRandomVal),
                            reinterpret_cast<const uint8_t*>(&kRandomVal)));

  ASSERT_EQ(clint_->mtimecmp_[0] >> 32, kRandomVal & 0xffffffff);
  ASSERT_EQ(clint_->mtimecmp_[1] & 0xffffffff, kRandomVal >> 32);

  // out of range
  ASSERT_FALSE(
      clint_->Store(kAddrBase + 8 * clint_->harts_ - 1, sizeof(kMtimeCmpVal[1]),
                    reinterpret_cast<const uint8_t*>(&kMtimeCmpVal[1])));
}

TEST_F(ClintTest, StoreMtime) {
  const uint64_t kRandomNumber = rg.Get();
  constexpr uint64_t kAddrBase = rv64_emulator::device::clint::kMtimeBase;

  ASSERT_TRUE(clint_->Store(kAddrBase, sizeof(kRandomNumber),
                            reinterpret_cast<const uint8_t*>(&kRandomNumber)));
  ASSERT_EQ(clint_->mtime_, kRandomNumber);

  // out of range
  ASSERT_FALSE(clint_->Store(kAddrBase + sizeof(kRandomNumber) - 1,
                             sizeof(kRandomNumber),
                             reinterpret_cast<const uint8_t*>(&kRandomNumber)));
}

TEST_F(ClintTest, Reset) {
  // generate random value
  clint_->mtime_ = rg.Get();
  for (uint64_t i = 0; i < clint_->harts_; i++) {
    clint_->msip_[i] = rg.Get();
    clint_->mtimecmp_[i] = rg.Get();
  }

  // clear
  clint_->Reset();

  for (uint64_t i = 0; i < clint_->harts_; i++) {
    ASSERT_EQ(clint_->msip_[i], 0);
    ASSERT_EQ(clint_->mtimecmp_[i], 0);
  }
}

TEST_F(ClintTest, Tick) {
  const uint16_t kMax = rg.Get(1, UINT16_MAX);
  const uint64_t kOrigin = clint_->mtime_;
  for (uint16_t i = 0; i < kMax; i++) {
    clint_->Tick();
  }
  ASSERT_EQ(clint_->mtime_ - kOrigin, kMax);
}

TEST_F(ClintTest, TimerIrq) {
  const uint64_t kRandomNumber = rg.Get(UINT8_MAX, UINT16_MAX);
  clint_->mtimecmp_.at(0) = kRandomNumber - 1;
  clint_->mtimecmp_.at(1) = kRandomNumber + 1;
  clint_->mtime_ = kRandomNumber;

  ASSERT_TRUE(clint_->MachineTimerIrq(0));
  ASSERT_FALSE(clint_->MachineTimerIrq(1));
}

TEST_F(ClintTest, SoftwareIrq) {
  // generate a random even
  const auto kRandomNumber = rg.Get();
  clint_->msip_.at(0) = kRandomNumber;
  clint_->msip_.at(1) = kRandomNumber + 1;

  bool msip0 = clint_->MachineSoftwareIrq(0);
  bool msip1 = clint_->MachineSoftwareIrq(1);

  ASSERT_TRUE(msip0 || msip1);
  ASSERT_FALSE(msip0 && msip1);
}
