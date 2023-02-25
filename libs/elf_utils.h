#pragma once

#include "elfio/elfio.hpp"

#include <tuple>

namespace rv64_emulator {

namespace cpu {
class CPU;
}
using cpu::CPU;

namespace libs::ElfUtils {

void LoadElf(const ELFIO::elfio& reader, CPU* cpu);

std::tuple<bool, ELFIO::Elf64_Addr> CheckSectionExist(const ELFIO::elfio& reader, const char* section_name);

} // namespace libs::ElfUtils
} // namespace rv64_emulator
