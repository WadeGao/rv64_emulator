#include <cstdint>
#include <map>
#include <vector>

#include "cpu/decode.h"
#include "elfio/elf_types.hpp"
#include "elfio/elfio.hpp"
#include "elfio/elfio_section.hpp"

constexpr size_t kMaxChars = 40;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("usage: %s <input riscv exe>\n", argv[0]);
    return 1;
  }
  // instruction frequency counter. key is table index, value is frequency
  std::map<int32_t, uint32_t> counter;

  ELFIO::elfio reader;
  reader.load(argv[1]);
  const auto kSectionNum = reader.sections.size();
  for (uint32_t i = 0; i < kSectionNum; i++) {
    const ELFIO::section* sec = reader.sections[i];
    if (sec->get_type() == ELFIO::SHT_PROGBITS &&
        (sec->get_flags() & ELFIO::SHF_EXECINSTR)) {
      const auto kSize = sec->get_size();
      const auto kDataStart = reinterpret_cast<const uint8_t*>(sec->get_data());
      const auto kDataEnd = kDataStart + kSize;
      const uint8_t* ptr = kDataStart;
      while (ptr < kDataEnd) {
        const uint32_t kInstructionLen = (!(*ptr & 0b11) ? 2 : 4);
        const uint32_t kWord = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += kInstructionLen;
        bool not_fount = true;
        for (int32_t i = 0;
             i < sizeof(rv64_emulator::cpu::decode::kInstTable) /
                     sizeof(rv64_emulator::cpu::decode::InstDesc);
             i++) {
          auto inst = rv64_emulator::cpu::decode::kInstTable[i];
          if ((kWord & inst.mask) == inst.signature) {
            ++counter[i];
            not_fount = false;
            break;
          }
        }
        if (not_fount) {
          ++counter[-1];
        }
      }
    }
  }

  uint32_t inst_cnt = 0;
  for (const auto& [_, v] : counter) {
    inst_cnt += v;
  }

  std::vector<std::pair<int32_t, uint32_t>> counter_vec(counter.begin(),
                                                        counter.end());
  std::sort(counter_vec.begin(), counter_vec.end(),
            [](const std::pair<int32_t, uint32_t> p1,
               std::pair<int32_t, uint32_t> p2) -> bool {
              return p1.second > p2.second;
            });

  printf("instruction usage histogram\n");
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

  uint32_t rank = 0;
  for (const auto [k, v] : counter_vec) {
    const char* kInstName =
        k < 0 ? "UNDEF" : rv64_emulator::cpu::decode::kInstTable[k].name;
    const auto kInstDesc = rv64_emulator::cpu::decode::kInstTable[k];

    printf(
        "%5u. %-10s %5.2f%% [%-7u] %s\n", ++rank, kInstName,
        static_cast<float>(v) / static_cast<float>(inst_cnt) * 100.0f, v,
        std::string(v * (kMaxChars - 1) / counter_vec[0].second, '#').c_str());
  }

  return 0;
}
