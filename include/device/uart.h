#pragma once

#include <cstdint>
#include <mutex>
#include <queue>

#include "device/mmio.hpp"

namespace rv64_emulator::device {
namespace uart {

class Uart : public MmioDevice {
 private:
  struct UartReg {
    uint32_t rx_fifo = 0;
    uint32_t tx_fifo = 0;
    uint32_t status = 0;
    uint32_t control = 0;
  };

  struct UartReg reg_;
  std::mutex rx_lock_;
  std::mutex tx_lock_;

  bool wait_ack_;

  std::queue<char> rx_buffer_;
  std::queue<char> tx_buffer_;

 public:
  Uart();
  bool Load(const uint64_t addr, const uint64_t bytes,
            uint8_t* buffer) override;
  bool Store(const uint64_t addr, const uint64_t bytes,
             const uint8_t* buffer) override;
  void Reset() override;

  void Putc(const char c);
  char Getc();

  bool TxBufferNotEmpty();
  bool Irq();
};

}  // namespace uart
}  // namespace rv64_emulator::device
