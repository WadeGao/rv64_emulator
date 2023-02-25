#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "dram.h"
#include "libs/elf_utils.h"

#include "elfio/elfio.hpp"
#include "fmt/core.h"

#include <cstdlib>
#include <memory>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fmt::print("{} error: no input elf file\n", argv[0]);
        exit(-1);
    }

    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(bus));

    ELFIO::elfio reader;
    reader.load(argv[1]);

    rv64_emulator::libs::ElfUtils::LoadElf(reader, cpu.get());
    cpu->SetPC(kDramBaseAddr);

    while (true) {
        cpu->Tick();
#ifdef DEBUG
        cpu->DumpRegisters();
#endif
    }

    return 0;
}
