#include "libs/lru.hpp"

#include <memory>

#include "fmt/core.h"
#include "gtest/gtest.h"

class LRUTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running LRU test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running LRU test case...\n");
  }

  void SetUp() override {
    auto lru =
        std::make_unique<rv64_emulator::libs::LRUCache<std::string, int>>(3);
    lru_ = std::move(lru);
  }

  void TearDown() override {}

  std::unique_ptr<rv64_emulator::libs::LRUCache<std::string, int>> lru_;
};

TEST_F(LRUTest, Basic) {
  lru_->Set("1", 1);
  lru_->Set("2", 2);
  lru_->Set("2", 2);
  EXPECT_EQ(lru_->cur_size_, 2);

  lru_->Set("3", 3);
  EXPECT_EQ(lru_->cur_size_, 3);

  int result = -1;
  EXPECT_TRUE(lru_->Get("2", &result) && (result == 2));

  result = -1;
  EXPECT_TRUE(!lru_->Get("4", &result) && (result == -1));

  result = -1;
  lru_->Set("1", 111);
  EXPECT_TRUE(lru_->Get("1", &result) && (result == 111));

  result = -1;
  lru_->Set("100", 100);
  EXPECT_TRUE(!lru_->Get("3", &result) && (result == -1));
}

TEST_F(LRUTest, Reset) {
  lru_->Reset();
  ASSERT_EQ(lru_->cur_size_, 0);
  ASSERT_EQ(lru_->kv_node_.size(), 0);
  ASSERT_EQ(lru_->cache_.size(), 0);
}
