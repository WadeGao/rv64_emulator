#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "dram.h"
#include "fmt/core.h"

#include <memory>

void LoadBin(const char* file_name, rv64_emulator::dram::DRAM* dram) {
    FILE* file = fopen(file_name, "rb");

    fseek(file, 0, SEEK_END);
    const size_t file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* buffer = new uint8_t[file_len + 1];
    fread(buffer, file_len, 1, file);
    fclose(file);

    for (size_t i = 0; i < file_len; i++) {
        dram->Store(kDramBaseAddr + i, 8, buffer[i]);
    }
}

int main(int argc, char* argv[]) {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    LoadBin(argv[1], dram.get());

    auto bus = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu = std::make_unique<rv64_emulator::cpu::CPU>(rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(bus));

    while (true) {
        cpu->Tick();
#ifdef DEBUG
        cpu->Dump();
#endif
    }

    return 0;
}
