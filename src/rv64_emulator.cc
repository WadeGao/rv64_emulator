#include "include/bus.h"
#include "include/conf.h"
#include "include/cpu.h"
#include "include/dram.h"

#include <memory>

int main() {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(std::move(bus));

    cpu->Execute(0xdcd78793);

    return 0;
}