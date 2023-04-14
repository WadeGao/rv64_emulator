#pragma once

#include <type_traits>
#include <utility>

#include "cpu/cpu.h"
#include "cpu/trap.h"
#include "elfio/elfio.hpp"

namespace rv64_emulator {

using cpu::CPU;
using cpu::trap::TrapType;

namespace libs::util {
void LoadElf(const ELFIO::elfio& reader, CPU* cpu);

bool CheckSectionExist(const ELFIO::elfio& reader, const char* name,
                       ELFIO::Elf64_Addr* addr);

uint64_t TrapToMask(const TrapType t);

TrapType InterruptBitsToTrap(const uint64_t bits);

bool CheckPcAlign(const uint64_t pc, const uint64_t isa);

}  // namespace libs::util
}  // namespace rv64_emulator
