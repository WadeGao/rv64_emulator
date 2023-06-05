#include "device/uart.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "fmt/core.h"
#include "gtest/gtest.h"

using rv64_emulator::device::uart::Uart;

class UartTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    fmt::print("start running Uart test case...\n");
  }

  static void TearDownTestSuite() {
    fmt::print("finish running Uart test case...\n");
  }

  void SetUp() override {
    auto uart = std::make_unique<Uart>();
    uart_ = std::move(uart);
  }

  void TearDown() override {}

  std::unique_ptr<Uart> uart_;
};

constexpr uint32_t kControlRstTx = 1;
constexpr uint32_t kControlRstRx = 2;

constexpr uint64_t kTxFifoAddr = offsetof(Uart::UartReg, tx_fifo);
constexpr uint64_t kControlAddr = offsetof(Uart::UartReg, control);
constexpr char kData[] = {'w', 'a', 'd', 'e'};

TEST_F(UartTest, StoreInvalid) {
  // store invalid addr
  constexpr uint64_t kInvalidAddr = sizeof(uart_->reg_);
  ASSERT_FALSE(uart_->Store(kInvalidAddr, 1, nullptr));
}

TEST_F(UartTest, StoreTxBuffer) {
  for (const char c : kData) {
    ASSERT_TRUE(uart_->Store(kTxFifoAddr, sizeof(c),
                             reinterpret_cast<const uint8_t*>(&c)));
    ASSERT_EQ(c, uart_->tx_buffer_.back());
  }

  ASSERT_EQ(sizeof(kData), uart_->tx_buffer_.size());
  for (const char c : kData) {
    ASSERT_EQ(c, uart_->Getc());
  }

  ASSERT_EQ(EOF, uart_->Getc());
}

TEST_F(UartTest, StoreControlRstTx) {
  constexpr uint32_t kRstTx[] = {~kControlRstTx, kControlRstTx};
  for (const uint32_t kCmd : kRstTx) {
    // 先向缓冲区写入数据
    for (const char c : kData) {
      ASSERT_TRUE(uart_->Store(kTxFifoAddr, sizeof(c),
                               reinterpret_cast<const uint8_t*>(&c)));
    }
    ASSERT_EQ(sizeof(kData), uart_->tx_buffer_.size());

    ASSERT_TRUE(uart_->Store(kControlAddr, sizeof(kCmd),
                             reinterpret_cast<const uint8_t*>(&kCmd)));
    ASSERT_NE(uart_->TxBufferNotEmpty(), kCmd & kControlRstTx);

    uart_->Reset();
  }
}

TEST_F(UartTest, StoreControlRstRx) {
  for (const char c : kData) {
    uart_->Putc(c);
  }
  ASSERT_EQ(sizeof(kData), uart_->rx_buffer_.size());

  constexpr uint32_t kRstRx[] = {~kControlRstRx, kControlRstRx};
  for (const uint32_t kCmd : kRstRx) {
    ASSERT_TRUE(uart_->Store(kControlAddr, sizeof(kCmd),
                             reinterpret_cast<const uint8_t*>(&kCmd)));
    ASSERT_NE(uart_->rx_buffer_.size() != 0, kCmd & kControlRstRx);
  }
}

TEST_F(UartTest, StoreControlRstRxTx) {
  for (const char c : kData) {
    uart_->Putc(c);
    ASSERT_TRUE(uart_->Store(kTxFifoAddr, sizeof(c),
                             reinterpret_cast<const uint8_t*>(&c)));
  }
  ASSERT_EQ(sizeof(kData), uart_->rx_buffer_.size());
  ASSERT_EQ(sizeof(kData), uart_->tx_buffer_.size());

  constexpr uint32_t kRstRxTxCmd = kControlRstRx | kControlRstTx;
  ASSERT_TRUE(uart_->Store(kControlAddr, sizeof(kRstRxTxCmd),
                           reinterpret_cast<const uint8_t*>(&kRstRxTxCmd)));
  ASSERT_EQ(0, uart_->rx_buffer_.size());
  ASSERT_FALSE(uart_->TxBufferNotEmpty());
}

TEST_F(UartTest, Irq) {
  ASSERT_FALSE(uart_->Irq());
  for (const char c : kData) {
    uart_->Putc(c);
    ASSERT_TRUE(uart_->Irq());
  }

  uart_->Reset();
  ASSERT_FALSE(uart_->wait_ack_);
  ASSERT_TRUE(uart_->Store(kTxFifoAddr, sizeof(kData[0]),
                           reinterpret_cast<const uint8_t*>(&kData[0])));
  ASSERT_EQ(kData[0], uart_->Getc());
  ASSERT_TRUE(uart_->wait_ack_);
}
