#include "gtest/gtest.h"

int main() {
    testing::InitGoogleTest();
    int ret = RUN_ALL_TESTS();
    return ret;
}