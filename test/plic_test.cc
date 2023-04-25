#include "device/plic.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "fmt/core.h"
#include "gtest/gtest.h"

using namespace rv64_emulator::device::plic;

class PlicTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running PLIC test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running PLIC test case...\n");
  }

  void SetUp() override {
    auto plic = std::make_unique<Plic>(2, true, 2);
    plic_ = std::move(plic);
  }

  void TearDown() override {}

  std::unique_ptr<Plic> plic_;
};

constexpr uint32_t kArbitrarilyVal = 0x12345678;
constexpr uint64_t kInvalidAddrs[] = {
    0x1080, 0x1180, 0x1f2000, 0x1f4000, 0x4000000,
};

TEST_F(PlicTest, Init) {
  ASSERT_EQ(plic_->contexts_.size(), 4);
  for (uint32_t i = 0; i < plic_->contexts_.size(); i++) {
    const auto kCtx = plic_->contexts_.at(i);
    ASSERT_EQ(kCtx.id, i);
    ASSERT_EQ(kCtx.mmode, i % 2 == 0);
  }
}

TEST_F(PlicTest, StorePriority) {
  constexpr uint32_t kPriority = 100;
  for (uint32_t src_num : {0, 1}) {
    const uint64_t kAddr = kSourcePriorityBase + src_num * 4;
    ASSERT_TRUE(plic_->Store(kAddr, sizeof(kPriority),
                             reinterpret_cast<const uint8_t*>(&kPriority)));
    ASSERT_TRUE(plic_->priority_[src_num] == kPriority);
  }

  const uint64_t kAddr = kSourcePriorityBase + 2 * 4;
  ASSERT_FALSE(plic_->Store(kAddr, sizeof(kPriority),
                            reinterpret_cast<const uint8_t*>(&kPriority)));
}

TEST_F(PlicTest, StorePending) {
  constexpr uint32_t kPendingWord = 0x12345678;
  ASSERT_TRUE(plic_->Store(kPendingBase, sizeof(kPendingWord),
                           reinterpret_cast<const uint8_t*>(&kPendingWord)));
  ASSERT_TRUE(plic_->pending_[0] == kPendingWord);

  const uint64_t kAddr = kPendingBase + 1 * 4;
  ASSERT_FALSE(plic_->Store(kAddr, sizeof(kPendingWord),
                            reinterpret_cast<const uint8_t*>(&kPendingWord)));
}

TEST_F(PlicTest, StoreEnable) {
  constexpr uint32_t kEnableWord = 0x12345678;
  // context id out of bound
  constexpr uint64_t kAddr = kEnableBase + 4 * kEnableBytesPerHart;
  ASSERT_FALSE(plic_->Store(kAddr, sizeof(kEnableWord),
                            reinterpret_cast<const uint8_t*>(&kEnableWord)));

  for (uint32_t ctx_id : {0, 1, 2, 3}) {
    const uint64_t kAddr = kEnableBase + ctx_id * kEnableBytesPerHart;
    ASSERT_TRUE(plic_->Store(kAddr, sizeof(kEnableWord),
                             reinterpret_cast<const uint8_t*>(&kEnableWord)));
    ASSERT_EQ(plic_->contexts_.at(ctx_id).enable[0], kEnableWord);

    // group word index out of bound
    ASSERT_FALSE(plic_->Store(kAddr + 4, sizeof(kEnableWord),
                              reinterpret_cast<const uint8_t*>(&kEnableWord)));
  }
}

TEST_F(PlicTest, StoreContext) {
  constexpr uint32_t kCtxThreshold = 0x12345678;
  // context id out of bound
  constexpr uint64_t kAddr = kContextBase + 4 * kContextBytesPerHart;
  ASSERT_FALSE(plic_->Store(kAddr, sizeof(kCtxThreshold),
                            reinterpret_cast<const uint8_t*>(&kCtxThreshold)));

  for (uint32_t ctx_id : {0, 1, 2, 3}) {
    const uint64_t kAddr = kContextBase + ctx_id * kContextBytesPerHart;
    // bias is 0, store context threshold
    ASSERT_TRUE(plic_->Store(kAddr + 0, sizeof(kCtxThreshold),
                             reinterpret_cast<const uint8_t*>(&kCtxThreshold)));
    ASSERT_EQ(plic_->contexts_.at(ctx_id).threshold, kCtxThreshold);

    // bias is 4, store context claimed bitset
    for (uint32_t claimed_bit : {0, 1, 2}) {
      // set all bits of the context claimed bitset
      plic_->contexts_.at(ctx_id).claimed_[0] = 0xffffffff;

      if (claimed_bit < 2) {
        // store operation
        ASSERT_TRUE(
            plic_->Store(kAddr + 4, sizeof(claimed_bit),
                         reinterpret_cast<const uint8_t*>(&claimed_bit)));
        // store operation make the corresponding bit reset
        ASSERT_EQ(plic_->contexts_.at(ctx_id).claimed_[0],
                  ~(1u << claimed_bit));
        continue;
      }

      // claimed bitset out of bound
      ASSERT_FALSE(
          plic_->Store(kAddr + 4, sizeof(claimed_bit),
                       reinterpret_cast<const uint8_t*>(&claimed_bit)));
      // context claimed bitset not changed if bitset index invalid
      ASSERT_EQ(plic_->contexts_.at(ctx_id).claimed_[0], 0xffffffff);
    }

    // offset == 8 is reserved
    ASSERT_FALSE(
        plic_->Store(kAddr + 8, sizeof(kCtxThreshold),
                     reinterpret_cast<const uint8_t*>(&kCtxThreshold)));
  }
}

TEST_F(PlicTest, StoreInvalid) {
  for (const auto kAddr : kInvalidAddrs) {
    ASSERT_FALSE(
        plic_->Store(kAddr, sizeof(kArbitrarilyVal),
                     reinterpret_cast<const uint8_t*>(&kArbitrarilyVal)));
  }
}

TEST_F(PlicTest, LoadPriority) {
  constexpr uint32_t kPriority = 100;
  for (uint32_t src_num : {0, 1}) {
    const uint64_t kAddr = kSourcePriorityBase + src_num * 4;
    ASSERT_TRUE(plic_->Store(kAddr, sizeof(kPriority),
                             reinterpret_cast<const uint8_t*>(&kPriority)));

    uint32_t val = UINT32_MAX;
    ASSERT_TRUE(
        plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
    ASSERT_EQ(val, kPriority);
  }

  const uint64_t kAddr = kSourcePriorityBase + 2 * 4;
  uint32_t val = UINT32_MAX;
  ASSERT_FALSE(
      plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
}

TEST_F(PlicTest, LoadPending) {
  constexpr uint32_t kPendingWord = 0x12345678;
  ASSERT_TRUE(plic_->Store(kPendingBase, sizeof(kPendingWord),
                           reinterpret_cast<const uint8_t*>(&kPendingWord)));

  uint32_t val = UINT32_MAX;
  ASSERT_TRUE(
      plic_->Load(kPendingBase, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
  ASSERT_EQ(val, kPendingWord);

  constexpr uint64_t kAddr = kPendingBase + 1 * 4;
  ASSERT_FALSE(
      plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
}

TEST_F(PlicTest, LoadEnable) {
  uint32_t val = UINT32_MAX;

  // context id out of bound
  constexpr uint64_t kAddr = kEnableBase + 4 * kEnableBytesPerHart;
  ASSERT_FALSE(
      plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));

  for (uint32_t ctx_id : {0, 1, 2, 3}) {
    const uint64_t kAddr = kEnableBase + ctx_id * kEnableBytesPerHart;
    const uint32_t kStoredVal = kArbitrarilyVal + ctx_id;

    ASSERT_TRUE(plic_->Store(kAddr, sizeof(kStoredVal),
                             reinterpret_cast<const uint8_t*>(&kStoredVal)));
    ASSERT_TRUE(
        plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
    ASSERT_EQ(val, kStoredVal);

    // group word index out of bound
    ASSERT_FALSE(
        plic_->Load(kAddr + 4, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
  }
}

TEST_F(PlicTest, LoadContext) {
  // context id out of bound
  uint32_t val = UINT32_MAX;
  constexpr uint64_t kAddr = kContextBase + 4 * kContextBytesPerHart;
  ASSERT_FALSE(
      plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));

  constexpr uint32_t kCtxThreshold = 0x12345678;
  for (uint32_t ctx_id : {0, 1, 2, 3}) {
    const uint64_t kAddr = kContextBase + ctx_id * kContextBytesPerHart;
    const uint32_t kVal = kCtxThreshold + ctx_id;
    // bias is 0, load context threshold
    ASSERT_TRUE(plic_->Store(kAddr + 0, sizeof(kVal),
                             reinterpret_cast<const uint8_t*>(&kVal)));

    val = UINT32_MAX;
    ASSERT_TRUE(
        plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
    ASSERT_EQ(val, kVal);

    // bias is 4, load context claimed bitset
    for (uint32_t claimed_bit : {0, 1}) {
      // reset all bits of the context claimed bitset
      plic_->contexts_.at(ctx_id).claimed_[0] = 0;
      val = UINT32_MAX;
      ASSERT_TRUE(plic_->Load(kAddr + 4, sizeof(val),
                              reinterpret_cast<uint8_t*>(&val)));
      ASSERT_EQ(plic_->contexts_.at(ctx_id).claim, val);
      ASSERT_EQ(plic_->contexts_.at(ctx_id).claimed_[0], (1u << val));
    }

    // offset == 8 is reserved
    ASSERT_FALSE(
        plic_->Load(kAddr + 8, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
  }
}

TEST_F(PlicTest, LoadInvalid) {
  uint32_t val = UINT32_MAX;
  for (const auto kAddr : kInvalidAddrs) {
    ASSERT_FALSE(
        plic_->Load(kAddr, sizeof(val), reinterpret_cast<uint8_t*>(&val)));
  }
}
TEST_F(PlicTest, GetInterrupt) {
  // 4 devices and 2 contexts
  auto plic = std::make_unique<Plic>(2, true, 4);
  plic_ = std::move(plic);

  constexpr uint32_t kCtxThresholdBase = 0x12345678;
  for (uint32_t id : {0, 1, 2, 3}) {
    // store priority of each device
    plic_->priority_[id] = kCtxThresholdBase + id + 1;
    // store threshold of each context
    plic_->contexts_[id].threshold = kCtxThresholdBase + id;
  }

  plic_->pending_[0] = 0b1011;

  // case 1: normal select
  plic_->contexts_[3].enable[0] = 0b1110;
  plic_->contexts_[3].claimed_[0] = 0b0010;
  ASSERT_TRUE(plic_->GetInterrupt(3));
  ASSERT_EQ(plic_->contexts_.at(3).claim, 3);

  // case 2: context 3 has two valid irq, select device 1
  plic_->contexts_[3].claimed_[0] = 0b0100;
  plic_->priority_[1] = plic_->priority_[3] + 1;
  ASSERT_TRUE(plic_->GetInterrupt(3));
  ASSERT_EQ(plic_->contexts_.at(3).claim, 1);

  // the same as case 2 but device 1 not enable, select device 3
  plic_->contexts_[3].enable[0] = 0b1100;
  ASSERT_TRUE(plic_->GetInterrupt(3));
  ASSERT_EQ(plic_->contexts_.at(3).claim, 3);

  // case 3: all device irq valid, but
  // the priority of device 2 is biggest one
  plic_->pending_[0] = 0b1111;
  plic_->contexts_[3].enable[0] = 0b1110;
  plic_->contexts_[3].claimed_[0] = 0b0001;
  plic_->priority_[1] = plic_->contexts_[3].threshold - 1;
  plic_->priority_[2] = plic_->contexts_[3].threshold + 1;
  plic_->priority_[3] = plic_->contexts_[3].threshold;
  ASSERT_TRUE(plic_->GetInterrupt(3));
  ASSERT_EQ(plic_->contexts_.at(3).claim, 2);
}

TEST_F(PlicTest, UpdateExt) {
  uint32_t pending_word = 0;
  constexpr uint32_t kSourceId = 1;
  ASSERT_TRUE(plic_->Store(kPendingBase, sizeof(pending_word),
                           reinterpret_cast<const uint8_t*>(&pending_word)));
  plic_->UpdateExt(1, true);
  ASSERT_EQ(plic_->pending_[0], (1u << kSourceId));

  pending_word = 0xffffffff;
  ASSERT_TRUE(plic_->Store(kPendingBase, sizeof(pending_word),
                           reinterpret_cast<const uint8_t*>(&pending_word)));
  plic_->UpdateExt(1, false);
  ASSERT_EQ(plic_->pending_[0], ~(1u << kSourceId));
}

TEST_F(PlicTest, Reset) { plic_->Reset(); }
