#include "device/bus.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "conf.h"
#include "device/dram.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

class BusTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running Bus test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running Bus test case...\n");
  }

  void SetUp() override {
    bus_ = std::make_unique<rv64_emulator::device::bus::Bus>();
    dram_ = std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize);
  }

  void TearDown() override {}

  std::unique_ptr<rv64_emulator::device::bus::Bus> bus_;
  std::unique_ptr<rv64_emulator::device::dram::DRAM> dram_;
};

constexpr uint64_t kInvalidAddr[] = {
    kDramBaseAddr - 4,
    kDramBaseAddr + kDramSize,
    kDramBaseAddr + kDramSize + 4,
};

constexpr uint64_t kArbitrarilyVal = 0x1122334455667788;

TEST_F(BusTest, Load) {
  // bypass the bus and store dram directly
  uint64_t* raw_data_ptr = reinterpret_cast<uint64_t*>(dram_->memory_.data());
  *raw_data_ptr = 0x1122334455667788;

  // no device mounted
  uint64_t res = 0;
  ASSERT_FALSE(bus_->Load(kDramBaseAddr, sizeof(uint64_t),
                          reinterpret_cast<uint8_t*>(&res)));

  // now load via bus
  bus_->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram_),
  });
  ASSERT_TRUE(bus_->Load(kDramBaseAddr, sizeof(uint64_t),
                         reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(0x1122334455667788, res);
}

TEST_F(BusTest, Store) {
  auto raw_dram_ptr = dram_.get();

  // no device mounted
  ASSERT_FALSE(bus_->Store(kDramBaseAddr, sizeof(uint64_t),
                           reinterpret_cast<const uint8_t*>(&kArbitrarilyVal)));

  bus_->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram_),
  });
  ASSERT_TRUE(bus_->Store(kDramBaseAddr, sizeof(uint64_t),
                          reinterpret_cast<const uint8_t*>(&kArbitrarilyVal)));

  // read from dram directly
  const uint64_t* kRawData =
      reinterpret_cast<uint64_t*>(raw_dram_ptr->memory_.data());
  uint64_t res = 0;
  ASSERT_TRUE(bus_->Load(kDramBaseAddr, sizeof(uint64_t),
                         reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(*kRawData, res);
}

TEST_F(BusTest, NoMountedDevice) {
  uint64_t res = 0;
  for (const auto kAddr : kInvalidAddr) {
    ASSERT_FALSE(
        bus_->Load(kAddr, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
    ASSERT_FALSE(
        bus_->Store(kAddr, sizeof(uint64_t),
                    reinterpret_cast<const uint8_t*>(&kArbitrarilyVal)));
  }
}

TEST_F(BusTest, InvalidRange) {
  uint64_t res = 0;

  bus_->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram_),
  });

  for (const auto kAddr : kInvalidAddr) {
    ASSERT_FALSE(
        bus_->Load(kAddr, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&res)));
    ASSERT_FALSE(
        bus_->Store(kAddr, sizeof(uint64_t),
                    reinterpret_cast<const uint8_t*>(&kArbitrarilyVal)));
  }
}
