// #include "third_party/gmock/gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>

int main() {
    testing::InitGoogleTest();
    int ret = RUN_ALL_TESTS();
    return ret;
}