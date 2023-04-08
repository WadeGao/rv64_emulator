#include <cstdlib>
#include <memory>

#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "dram.h"
#include "elfio/elfio.hpp"
#include "fmt/core.h"
#include "libs/utils.h"
#include "mmu.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fmt::print("{} error: no input elf file\n", argv[0]);
    exit(-1);
  }

  auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
  auto bus = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
  auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(std::move(bus));
  auto mmu = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
  auto cpu = std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));

  ELFIO::elfio reader;
  reader.load(argv[1]);

  rv64_emulator::libs::util::LoadElf(reader, cpu.get());

  cpu->SetPC(reader.get_entry());

  while (true) {
    cpu->Tick();
#ifdef DEBUG
    cpu->DumpRegs();
#endif
  }

  return 0;
}
