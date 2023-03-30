#include "libs/LRU.hpp"

#include "fmt/core.h"
#include "gtest/gtest.h"
#include <memory>

class LRUTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        fmt::print("start running LRU test case...\n");
    }

    static void TearDownTestSuite() {
        fmt::print("finish running LRU test case...\n");
    }

    void SetUp() override {
        auto lru = std::make_unique<rv64_emulator::libs::LRUCache<std::string, int>>(3);
        m_lru    = std::move(lru);
    }

    void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::libs::LRUCache<std::string, int>> m_lru;
};

TEST_F(LRUTest, Basic) {
    m_lru->Set("1", 1);
    m_lru->Set("2", 2);
    m_lru->Set("2", 2);
    EXPECT_EQ(m_lru->m_current_size, 2);

    m_lru->Set("3", 3);
    EXPECT_EQ(m_lru->m_current_size, 3);

    int result = -1;
    EXPECT_TRUE(m_lru->Get("2", result) && (result == 2));

    result = -1;
    EXPECT_TRUE(!m_lru->Get("4", result) && (result == -1));

    result = -1;
    m_lru->Set("1", 111);
    EXPECT_TRUE(m_lru->Get("1", result) && (result == 111));

    result = -1;
    m_lru->Set("100", 100);
    EXPECT_TRUE(!m_lru->Get("3", result) && (result == -1));
}

TEST_F(LRUTest, Reset) {
    m_lru->Reset();
    ASSERT_EQ(m_lru->m_current_size, 0);
    ASSERT_EQ(m_lru->m_list.size(), 0);
    ASSERT_EQ(m_lru->m_cache.size(), 0);
}
