#include <termios.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>

#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/mmu.h"
#include "device/bus.h"
#include "device/clint.h"
#include "device/dram.h"
#include "device/mmio.hpp"
#include "device/plic.h"
#include "device/uart.h"
#include "fmt/core.h"
#include "libs/utils.h"

bool delay_cr = false;
bool send_ctrl_c = false;

void UartInput(rv64_emulator::device::uart::Uart* uart) {
  termios tmp;
  tcgetattr(STDIN_FILENO, &tmp);
  tmp.c_lflag &= (~ICANON & ~ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &tmp);
  while (true) {
    char c = getchar();
    if (c == 10) {
      c = 13;
    }
    uart->Putc(c);
  }
}

void SigintHangler(int x) {
  static time_t last_time;
  if (time(nullptr) - last_time < 1) {
    exit(0);
  }
  last_time = time(nullptr);
  send_ctrl_c = true;
}

std::unique_ptr<rv64_emulator::cpu::CPU> MakeCPU(
    std::shared_ptr<rv64_emulator::device::bus::Bus>& bus) {
  auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(bus);
  auto mmu = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
  return std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fmt::print("{} error: no input elf file\n", argv[0]);
    exit(-1);
  }

  signal(SIGINT, SigintHangler);

  auto dram =
      std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize, argv[1]);
  auto uart = std::make_unique<rv64_emulator::device::uart::Uart>();
  auto clint = std::make_unique<rv64_emulator::device::clint::Clint>(1);
  auto plic = std::make_unique<rv64_emulator::device::plic::Plic>(1, true, 2);

  auto raw_clint = clint.get();
  auto raw_uart = uart.get();
  auto raw_plic = plic.get();

  auto bus = std::make_shared<rv64_emulator::device::bus::Bus>();
  bus->MountDevice({
      .base = kClintBase,
      .size = kClintAddrSpaceRange,
      .dev = std::move(clint),
  });
  bus->MountDevice({
      .base = kPlicBase,
      .size = kPlicAddrSpaceRange,
      .dev = std::move(plic),
  });
  bus->MountDevice({
      .base = kUartBase,
      .size = kUartAddrSpaceRange,
      .dev = std::move(uart),
  });
  bus->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram),
  });

  std::thread uart_input_thread(UartInput, raw_uart);

  auto cpu1 = MakeCPU(bus);

  cpu1->pc_ = kDramBaseAddr;

  while (true) {
    raw_clint->Tick();
    raw_plic->UpdateExt(1, raw_uart->Irq());
    cpu1->Tick(raw_plic->GetInterrupt(0), raw_plic->GetInterrupt(1),
               raw_clint->MachineSoftwareIrq(0), raw_clint->MachineTimerIrq(0),
               true);

    while (raw_uart->TxBufferNotEmpty()) {
      char ch = raw_uart->Getc();
      if (ch == '\r') {
        delay_cr = true;
      } else {
        if (delay_cr && ch != '\n') {
          fmt::print("\r");
        }
        fmt::print("{}", ch);
        fflush(stdout);
        delay_cr = false;
      }
    }

    if (send_ctrl_c) {
      raw_uart->Putc(3);
      send_ctrl_c = false;
    }
  }

  return 0;
}
