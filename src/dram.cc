#include "dram.h"

#include <cstdint>
#include <cstdlib>

#include "conf.h"
#include "fmt/core.h"

namespace rv64_emulator::dram {

DRAM::DRAM(const uint64_t mem_size) : size_(mem_size), memory_(mem_size, 0) {
#ifdef DEBUG
  fmt::print("dram init to all zeros, size {} bytes\n", mem_size);
#endif
}

uint64_t DRAM::GetSize() const { return size_; }

bool DRAM::Load(const uint64_t addr, const uint64_t bytes,
                uint8_t* buffer) const {
  if (addr + bytes <= size_) {
    memcpy(buffer, &memory_[addr], bytes);
    return true;
  }

#ifdef DEBUG
  fmt::print(
      "dram load error, addr[{:#018x}], bytes[{:#018x}], m_size[{:#018x}]\n",
      addr, bytes, size_);
#endif

  return false;
}

bool DRAM::Store(const uint64_t addr, const uint64_t bytes,
                 const uint8_t* buffer) {
  if (addr + bytes <= size_) {
    memcpy(&memory_[addr], buffer, bytes);
    return true;
  }

#ifdef DEBUG
  fmt::print(
      "dram store error, addr[{:#018x}], bytes[{:#018x}], m_size[{:#018x}]\n",
      addr, bytes, size_);
#endif

  return false;
}

void DRAM::Reset() { std::fill(memory_.begin(), memory_.end(), 0); }

DRAM::~DRAM() {
#ifdef DEBUG
  fmt::print("destroy a dram, size {} bytes\n", size_);
#endif
}

}  // namespace rv64_emulator::dram
