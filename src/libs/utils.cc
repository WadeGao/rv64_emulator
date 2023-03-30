#include "libs/utils.h"
#include "cpu/cpu.h"
#include "cpu/trap.h"
#include "error_code.h"

#include "elfio/elfio.hpp"
#include "fmt/core.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <tuple>

namespace rv64_emulator {

namespace cpu {
class CPU;
}
using cpu::CPU;

namespace libs::util {

void LoadElf(const ELFIO::elfio& reader, CPU* cpu) {
    const std::string& kErrMsg = reader.validate();
    if (!kErrMsg.empty()) {
        fmt::print("Error occurs while loading elf: {}\n", kErrMsg);
        exit(static_cast<int>(rv64_emulator::errorcode::ElfErrorCode::kInvalidFile));
    }

    const ELFIO::Elf_Half kSegmentNum = reader.segments.size();
    for (uint16_t i = 0; i < kSegmentNum; i++) {
        const ELFIO::segment* segment      = reader.segments[i];
        const ELFIO::Elf_Word kSegmentType = segment->get_type();
        if (kSegmentType != ELFIO::PT_LOAD) {
            continue;
        }

        const ELFIO::Elf_Xword kSegFileSize = segment->get_file_size();
        const ELFIO::Elf_Xword kSegMemize   = segment->get_memory_size();
        const ELFIO::Elf_Xword kSegAddr     = segment->get_virtual_address();

        char const* bytes = segment->get_data();
        for (ELFIO::Elf_Xword i = kSegAddr; i < kSegAddr + kSegMemize; i++) {
            const uint8_t kVal = i < kSegAddr + kSegFileSize ? bytes[i - kSegAddr] : 0;
            cpu->Store(i, sizeof(uint8_t), &kVal);
        }
    }
}

bool CheckSectionExist(const ELFIO::elfio& reader, const char* section_name, ELFIO::Elf64_Addr& addr) {
    const ELFIO::Elf_Half kShStrIndex = reader.get_section_name_str_index();
    const ELFIO::Elf_Half kSectionNum = reader.sections.size();

    if (ELFIO::SHN_UNDEF != kShStrIndex) {
        ELFIO::string_section_accessor str_reader(reader.sections[kShStrIndex]);
        for (ELFIO::Elf_Half i = 0; i < kSectionNum; ++i) {
            const auto kPtr = reader.sections[i];

            const ELFIO::Elf_Word kSectionOffset = kPtr->get_name_string_offset();

            const char* p = str_reader.get_string(kSectionOffset);
            if (strcmp(p, section_name) == 0) {
                addr = kPtr->get_address();
                return true;
            }
        }
    }

    return false;
}

uint64_t TrapToMask(const rv64_emulator::cpu::trap::TrapType t) {
    using rv64_emulator::cpu::trap::kTrapToCauseTable;
    using rv64_emulator::cpu::trap::TrapType;

    if (t == TrapType::kNone) {
        return 0;
    }

    const uint64_t kIndex = 1 << kTrapToCauseTable.at(t);
    return kIndex;
}

rv64_emulator::cpu::trap::TrapType InterruptBitsToTrap(const uint64_t bits) {
    using rv64_emulator::cpu::trap::TrapType;

    // 中断优先级：MEI > MSI > MTI > SEI > SSI > STI
    for (const auto t : {
             TrapType::kMachineExternalInterrupt,
             TrapType::kMachineSoftwareInterrupt,
             TrapType::kMachineTimerInterrupt,
             TrapType::kSupervisorExternalInterrupt,
             TrapType::kSupervisorSoftwareInterrupt,
             TrapType::kSupervisorTimerInterrupt,
         }) {
        if (TrapToMask(t) & bits) {
            return t;
        }
    }

    return TrapType::kNone;
}

} // namespace libs::util
} // namespace rv64_emulator
