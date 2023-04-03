#include "dram.h"

#include <cstdint>
#include <memory>

#include "conf.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

class DRAMTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running DRAM test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running DRAM test case...\n");
  }

  void SetUp() override {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    dram_ = std::move(dram);
  }

  void TearDown() override {}

  std::unique_ptr<rv64_emulator::dram::DRAM> dram_;
};

constexpr uint64_t kArbitraryAddr = 0;
constexpr uint64_t kArbitraryVal = 0x1122334455667788;

TEST_F(DRAMTest, GetSize) { ASSERT_EQ(kDramSize, dram_->GetSize()); }

TEST_F(DRAMTest, Store) {
  ASSERT_TRUE(dram_->Store(kArbitraryAddr, sizeof(uint64_t),
                           reinterpret_cast<const uint8_t*>(&kArbitraryVal)));

  const uint8_t* kRawBytes = dram_->memory_.data();

  const auto* kDwordRawData = reinterpret_cast<const uint64_t*>(kRawBytes);
  ASSERT_EQ(*kDwordRawData, kArbitraryVal);

  ASSERT_FALSE(dram_->Store(dram_->GetSize() - 7, sizeof(uint64_t),
                            reinterpret_cast<const uint8_t*>(&kArbitraryVal)));
}

TEST_F(DRAMTest, Load) {
  dram_->Store(kArbitraryAddr, sizeof(uint64_t),
               reinterpret_cast<const uint8_t*>(&kArbitraryVal));

  uint64_t* dword_ptr = reinterpret_cast<uint64_t*>(dram_->memory_.data());
  *dword_ptr = kArbitraryVal;

  uint64_t res;
  ASSERT_TRUE(dram_->Load(kArbitraryAddr, sizeof(uint64_t),
                          reinterpret_cast<uint8_t*>(&res)));
  ASSERT_EQ(res, kArbitraryVal);
}

TEST_F(DRAMTest, InvalidLoadAddressRange) {
  uint64_t res;
  ASSERT_FALSE(dram_->Load(dram_->GetSize() - 7, sizeof(uint64_t),
                           reinterpret_cast<uint8_t*>(&res)));
}

TEST_F(DRAMTest, InvalidStoreAddressRange) {
  uint64_t res;
  ASSERT_FALSE(dram_->Store(dram_->GetSize() - 7, sizeof(uint64_t),
                            reinterpret_cast<const uint8_t*>(&kArbitraryVal)));
}
