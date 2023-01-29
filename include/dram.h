#ifndef RV64_EMULATOR_INCLUDE_DRAM_H_
#define RV64_EMULATOR_INCLUDE_DRAM_H_

#include <cstdint>
#include <vector>

namespace rv64_emulator::dram {

class DRAM {
private:
    const uint64_t       m_size;
    std::vector<uint8_t> m_memory;

public:
    DRAM(const uint64_t mem_size);
    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);
    uint64_t GetSize() const;
    ~DRAM();
};

} // namespace rv64_emulator::dram

#endif
