#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>

#include "conf.h"
#include "cpu/cpu.h"
#include "device/bus.h"
#include "device/clint.h"
#include "device/dram.h"
#include "device/mmio.hpp"
#include "elfio/elfio.hpp"
#include "fmt/core.h"
#include "libs/utils.h"
#include "mmu.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fmt::print("{} error: no input elf file\n", argv[0]);
    exit(-1);
  }

  auto dram = std::make_unique<rv64_emulator::device::dram::DRAM>(kDramSize);
  auto clint = std::make_unique<rv64_emulator::device::clint::Clint>(1);
  auto raw_clint = clint.get();
  std::thread oscillator([raw_clint] {
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      raw_clint->Tick();
    }
  });
  oscillator.detach();

  auto bus = std::make_unique<rv64_emulator::device::bus::Bus>();
  bus->MountDevice({
      .base = kDramBaseAddr,
      .size = kDramSize,
      .dev = std::move(dram),
  });
  bus->MountDevice({
      .base = kClintBase,
      .size = kClintAddrSpaceRange,
      .dev = std::move(clint),
  });

  auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(std::move(bus));
  auto mmu = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
  auto cpu = std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));

  ELFIO::elfio reader;
  reader.load(argv[1]);

  rv64_emulator::libs::util::LoadElf(reader, cpu.get());

  cpu->SetPC(reader.get_entry());

  while (true) {
    cpu->Tick(false, false, raw_clint->MachineSoftwareIrq(0),
              raw_clint->MachineTimerIrq(0), true);
#ifdef DEBUG
    cpu->DumpRegs();
#endif
  }

  return 0;
}
