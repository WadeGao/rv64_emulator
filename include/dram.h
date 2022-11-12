#ifndef RV64_EMULATOR_INCLUDE_DRAM_H_
#define RV64_EMULATOR_INCLUDE_DRAM_H_

#include <cstdint>
#include <memory>

namespace rv64_emulator {
namespace dram {

class DRAM {
private:
    uint64_t                   m_size;
    std::unique_ptr<uint8_t[]> m_memory;

public:
    DRAM(const uint64_t mem_size);
    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);
    uint64_t GetSize() const;
    ~DRAM();
};

} // namespace dram
} // namespace rv64_emulator

#endif