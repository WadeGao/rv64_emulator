#ifndef RV64_EMULATOR_SRC_CPU_H_
#define RV64_EMULATOR_SRC_CPU_H_

#include "include/bus.h"
#include "include/conf.h"

#include <cstdint>
#include <memory>

namespace rv64_emulator {
namespace cpu {

class CPU {
private:
    uint64_t m_pc;
    uint64_t m_reg[RV64_REGISTER_NUM];

    /*

    +───────────+───────────+────────────────────────────────────+────────+
    | Register  | ABI Name  | Description                        | Saver  |
    +───────────+───────────+────────────────────────────────────+────────+
    | x0        | zero      | Hard-wired zero                    |  ----  |
    | x1        | ra        | Return address                     | Caller |
    | x2        | sp        | Stack pointer                      | Callee |
    | x3        | gp        | Global pointer                     |  ----  |
    | x4        | tp        | Thread pointer                     |  ----  |
    | x5        | t0        | Temporary/alternate link register  | Caller |
    | x6 - 7    | t1 - 2    | Temporaries                        | Caller |
    | x8        | s0/fp     | Saved register/frame pointer       | Callee |
    | x9        | s1        | Saved registers                    | Callee |
    | x10 - 11  | a0 - 1    | Function args/return values        | Caller |
    | x12 - 17  | a2 - 7    | Function args                      | Caller |
    | x18 - 27  | s2 - 11   | Saved registers                    | Callee |
    | x28 - 31  | t3 - 6    | Temporaries                        | Caller |
    +───────────+───────────+────────────────────────────────────+────────+

    */
    std::unique_ptr<rv64_emulator::bus::Bus> m_bus;

    uint64_t Load(const uint64_t addr, const uint64_t bit_size) const;
    void     Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val);

public:
    CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus);
    uint32_t Fetch() const;
    ~CPU();
};

} // namespace cpu
} // namespace rv64_emulator
#endif