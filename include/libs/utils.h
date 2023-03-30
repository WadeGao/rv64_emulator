#pragma once

#include "cpu/trap.h"

#include "elfio/elfio.hpp"
namespace rv64_emulator {

namespace cpu {
class CPU;
}
using cpu::CPU;

namespace libs::util {

void LoadElf(const ELFIO::elfio& reader, CPU* cpu);

bool CheckSectionExist(const ELFIO::elfio& reader, const char* section_name, ELFIO::Elf64_Addr& addr);

uint64_t TrapToMask(const rv64_emulator::cpu::trap::TrapType t);

rv64_emulator::cpu::trap::TrapType InterruptBitsToTrap(const uint64_t bits);

} // namespace libs::util
} // namespace rv64_emulator
