#include "device/dram.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "conf.h"

namespace rv64_emulator::device::dram {

DRAM::DRAM(const uint64_t mem_size) : size_(mem_size), memory_(mem_size, 0) {}

DRAM::DRAM(const uint64_t mem_size, const char* in_file) : DRAM(mem_size) {
  uint64_t file_size = std::filesystem::file_size(in_file);
  std::ifstream file(in_file, std::ios::in | std::ios::binary);

  file.read(reinterpret_cast<char*>(memory_.data()),
            std::max(file_size, mem_size));
}

uint64_t DRAM::GetSize() const { return size_; }

bool DRAM::Load(const uint64_t addr, const uint64_t bytes, uint8_t* buffer) {
  if (addr + bytes <= size_) {
    memcpy(buffer, &memory_[addr], bytes);
    return true;
  }

  return false;
}

bool DRAM::Store(const uint64_t addr, const uint64_t bytes,
                 const uint8_t* buffer) {
  if (addr + bytes <= size_) {
    memcpy(&memory_[addr], buffer, bytes);
    return true;
  }

  return false;
}

void DRAM::Reset() { std::fill(memory_.begin(), memory_.end(), 0); }

}  // namespace rv64_emulator::device::dram
