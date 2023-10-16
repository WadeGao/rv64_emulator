
#include "libs/utils.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>

#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "elfio/elfio.hpp"
#include "error_code.h"
#include "fmt/core.h"

namespace rv64_emulator::libs::util {

using cpu::CPU;

void LoadElf(const ELFIO::elfio& reader, CPU* cpu) {
  std::string err_msg = reader.validate();
  if (!err_msg.empty()) {
    fmt::print("Error occurs while loading elf: {}\n", err_msg);
    exit(
        static_cast<int>(rv64_emulator::errorcode::ElfErrorCode::kInvalidFile));
  }

  const ELFIO::Elf_Half kSegmentNum = reader.segments.size();
  for (uint16_t i = 0; i < kSegmentNum; i++) {
    const ELFIO::segment* segment = reader.segments[i];
    const ELFIO::Elf_Word kSegmentType = segment->get_type();
    if (kSegmentType != ELFIO::PT_LOAD) {
      continue;
    }

    const ELFIO::Elf_Xword kSegFileSize = segment->get_file_size();
    const ELFIO::Elf_Xword kSegMemize = segment->get_memory_size();
    const ELFIO::Elf_Xword kSegAddr = segment->get_virtual_address();

    char const* bytes = segment->get_data();
    for (ELFIO::Elf_Xword i = kSegAddr; i < kSegAddr + kSegMemize; i++) {
      const uint8_t kVal =
          i < kSegAddr + kSegFileSize ? bytes[i - kSegAddr] : 0;
      cpu->Store(i, sizeof(uint8_t), &kVal);
    }
  }
}

bool CheckSectionExist(const ELFIO::elfio& reader, const char* name,
                       ELFIO::Elf64_Addr* addr) {
  const ELFIO::Elf_Half kShStrIndex = reader.get_section_name_str_index();
  const ELFIO::Elf_Half kSectionNum = reader.sections.size();

  if (ELFIO::SHN_UNDEF != kShStrIndex) {
    ELFIO::string_section_accessor str_reader(reader.sections[kShStrIndex]);
    for (ELFIO::Elf_Half i = 0; i < kSectionNum; ++i) {
      auto* const kPtr = reader.sections[i];

      const ELFIO::Elf_Word kSectionOffset = kPtr->get_name_string_offset();

      const char* p = str_reader.get_string(kSectionOffset);
      if (strcmp(p, name) == 0) {
        *addr = kPtr->get_address();
        return true;
      }
    }
  }

  return false;
}

uint64_t TrapToMask(TrapType t) {
  using rv64_emulator::cpu::trap::kTrapToCauseTable;

  if (t == TrapType::kNone) {
    return 0;
  }

  const uint64_t kIndex = 1 << kTrapToCauseTable[static_cast<uint64_t>(t)];
  return kIndex;
}

TrapType InterruptBitsToTrap(uint64_t bits) {
  // 中断优先级：MEI > MSI > MTI > SEI > SSI > STI
  constexpr TrapType kRankedIrqs[] = {
      TrapType::kMachineExternalInterrupt,
      TrapType::kMachineSoftwareInterrupt,
      TrapType::kMachineTimerInterrupt,
      TrapType::kSupervisorExternalInterrupt,
      TrapType::kSupervisorSoftwareInterrupt,
      TrapType::kSupervisorTimerInterrupt,
  };
  for (auto t : kRankedIrqs) {
    if (TrapToMask(t) & bits) {
      return t;
    }
  }

  return TrapType::kNone;
}

float GetMips(decltype(std::chrono::high_resolution_clock::now()) start,
              uint64_t insret_cnt) {
  const auto kEnd = std::chrono::high_resolution_clock::now();
  const auto kDurationUs =
      std::chrono::duration_cast<std::chrono::microseconds>(kEnd - start);
  const float kMips =
      static_cast<float>(insret_cnt) / static_cast<float>(kDurationUs.count());
  return kMips;
}

uint64_t __attribute__((always_inline)) ReadHostTimeStamp() {
  uint64_t counter = 0;

#ifdef __aarch64__
  asm volatile("mrs %0, cntvct_el0" : "=r"(counter));
#endif

#ifdef __x86_64__
  uint32_t low, high;
  asm volatile("rdtsc" : "=a"(low), "=d"(high));
  counter = ((uint64_t)high << 32) | low;
#endif

  return counter;
}

uint64_t __attribute__((always_inline)) ReadFreq() {
  uint64_t freq = 0;
#ifdef __aarch64__
  asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
#endif

#ifdef __x86_64__
  // TODO(Wade) add x86 get freq code. now use a fixed num
  freq = 2500000000;
#endif
  return freq;
}

double __attribute__((always_inline)) GetFreqDivCoeff() {
  uint64_t host_freq = ReadFreq();
  double rate = kMtimeFreq / (host_freq * 1.0);
  return rate;
}

uint64_t __attribute__((always_inline)) ReadGuestTimeStamp() {
  static double freq_div_coeff = GetFreqDivCoeff();
  uint64_t host_ts = ReadHostTimeStamp();
  uint64_t guest_ts = (host_ts * freq_div_coeff);
  return guest_ts;
}

}  // namespace rv64_emulator::libs::util
