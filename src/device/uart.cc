#include "device/uart.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>

namespace rv64_emulator::device::uart {

constexpr uint32_t kTxFifoFull = 0b1000;
constexpr uint32_t kTxFifoEmpty = 0b0100;
constexpr uint32_t kRxFifoFull = 0b0010;
constexpr uint32_t kRxFifoValidData = 0b0001;

constexpr uint32_t kControlRstTx = 1;
constexpr uint32_t kControlRstRx = 2;

Uart::Uart() : wait_ack_(false) { reg_.status = kTxFifoEmpty; }

bool Uart::Load(uint64_t addr, uint64_t bytes, uint8_t* buffer) {
  std::unique_lock<std::mutex> lock(rx_lock_);
  if (addr + bytes > sizeof(reg_)) {
    return false;
  }

  if (!rx_buffer_.empty()) {
    reg_.status |= kRxFifoValidData;
    reg_.rx_fifo = rx_buffer_.front();
  } else {
    reg_.status &= ~kRxFifoValidData;
  }

  memcpy(buffer, reinterpret_cast<const char*>(&reg_) + addr, bytes);
  wait_ack_ = false;
  constexpr auto kRxFifoBias = offsetof(struct UartReg, rx_fifo);
  if (addr <= kRxFifoBias && kRxFifoBias <= addr + bytes) {
    if (!rx_buffer_.empty()) {
      rx_buffer_.pop();
    }
  }

  return true;
}

bool Uart::Store(uint64_t addr, uint64_t bytes, const uint8_t* buffer) {
  std::unique_lock<std::mutex> lock_rx(rx_lock_);
  std::unique_lock<std::mutex> lock_tx(tx_lock_);

  if (addr + bytes > sizeof(reg_)) {
    return false;
  }

  memcpy(reinterpret_cast<char*>(&reg_) + addr, buffer, bytes);
  constexpr auto kTxFifoBias = offsetof(struct UartReg, tx_fifo);
  constexpr auto kControlBias = offsetof(struct UartReg, control);

  if (addr <= kTxFifoBias && kTxFifoBias <= addr + bytes) {
    tx_buffer_.push(reg_.tx_fifo & 0xff);
  }

  if (addr <= kControlBias && kControlBias <= addr + bytes) {
    if (reg_.control & kControlRstTx) {
      while (!tx_buffer_.empty()) {
        tx_buffer_.pop();
      }
    }

    if (reg_.control & kControlRstRx) {
      while (!rx_buffer_.empty()) {
        rx_buffer_.pop();
      }
    }
  }

  return true;
}

void Uart::Reset() {
  reg_.status = kTxFifoEmpty;
  wait_ack_ = false;
  while (!rx_buffer_.empty()) {
    rx_buffer_.pop();
  }
  while (!tx_buffer_.empty()) {
    tx_buffer_.pop();
  }
}

void Uart::Putc(char ch) {
  std::unique_lock<std::mutex> lock(rx_lock_);
  rx_buffer_.push(ch);
}

char Uart::Getc() {
  std::unique_lock<std::mutex> lock(tx_lock_);
  if (!tx_buffer_.empty()) {
    char res = tx_buffer_.front();
    tx_buffer_.pop();
    if (tx_buffer_.empty()) {
      wait_ack_ = true;
    }
    return res;
  }
  return EOF;
}

bool Uart::TxBufferNotEmpty() {
  std::unique_lock<std::mutex> lock(tx_lock_);
  return !tx_buffer_.empty();
}

bool Uart::Irq() {
  std::unique_lock<std::mutex> lock(rx_lock_);
  return !rx_buffer_.empty() || wait_ack_;
}

}  // namespace rv64_emulator::device::uart
