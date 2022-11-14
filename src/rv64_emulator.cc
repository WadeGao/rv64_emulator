#include "include/bus.h"
#include "include/conf.h"
#include "include/cpu.h"
#include "include/dram.h"

#include <memory>

int main() {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(std::move(bus));

    // edd78793   addi	a5,a5,-291 (0x123)
    cpu->Execute(0xedd78793);

    return 0;
}