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
  }

  void TearDown() override {}

  std::unique_ptr<rv64_emulator::device::bus::Bus> bus_;
};

TEST_F(BusTest, Load) {
  // bypass the bus and store dram directly
  auto dram = std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize);
  uint64_t* raw_data_ptr = reinterpret_cast<uint64_t*>(dram->memory_.data());
  *raw_data_ptr = 0x1122334455667788;

  // now load via bus

  bus_->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram),
  });
  uint64_t res = 0;
  ASSERT_TRUE(bus_->Load(kDramBaseAddr, sizeof(uint64_t),
                         reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(0x1122334455667788, res);
}

TEST_F(BusTest, Store) {
  auto dram = std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize);
  auto raw_dram_ptr = dram.get();

  bus_->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram),
  });

  constexpr uint64_t kVal = 0x1122334455667788;
  bus_->Store(kDramBaseAddr, sizeof(uint64_t),
              reinterpret_cast<const uint8_t*>(&kVal));

  // read from dram directly
  const uint64_t* kRawData =
      reinterpret_cast<uint64_t*>(raw_dram_ptr->memory_.data());
  uint64_t res = 0;
  ASSERT_TRUE(bus_->Load(kDramBaseAddr, sizeof(uint64_t),
                         reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(*kRawData, res);
}
