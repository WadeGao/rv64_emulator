#pragma once

#include <chrono>
#include <random>
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

uint64_t TrapToMask(TrapType t);
TrapType InterruptBitsToTrap(uint64_t bits);
float GetMips(decltype(std::chrono::high_resolution_clock::now()) start,
              uint64_t insret_cnt);

template <typename T>
class RandomGenerator {
 public:
  RandomGenerator(const T min, const T max) : gen_(rd_()), dis_(min, max) {}
  T Get() { return dis_(gen_); }
  T Get(const T min, const T max) {
    T res = Get();
    return res % (max - min + 1) + min;
  }

 private:
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<std::mt19937::result_type> dis_;
  T min_, max_;
};

uint64_t ReadGuestTimeStamp();

template <bool CEnable = false>
bool CheckPcAlign(uint64_t pc) {
  if constexpr (CEnable) {
    return (pc & (2 - 1)) == 0;
  }
  return (pc & (4 - 1)) == 0;
}

}  // namespace libs::util
}  // namespace rv64_emulator
