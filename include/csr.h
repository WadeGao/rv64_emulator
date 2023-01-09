#ifndef RV64_EMULATOR_INCLUDE_CSR_H_
#define RV64_EMULATOR_INCLUDE_CSR_H_

#include <cstdint>

namespace rv64_emulator {
namespace cpu {
namespace csr {

constexpr uint16_t kCsrFflags = 0x001; // Floating-Point Accrued Exceptions.
constexpr uint16_t kCsrFrm    = 0x002; // Floating-Point Dynamic Rounding Mode

// something about fcsr:
// refernce: https://zhuanlan.zhihu.com/p/339497543
// format: https://pic4.zhimg.com/v2-f247fb6ddaed4a7bfe5acf93e8227537_r.jpg
constexpr uint16_t kCsrFcsr = 0x003; // Floating-Point Control and Status Register (frm + fflags)

// Machine Info Registers
constexpr uint16_t kCsrMVendorId = 0xf11;
constexpr uint16_t kCsrMArchId   = 0xf12;
constexpr uint16_t kCsrMImpId    = 0xf13;
constexpr uint16_t kCsrMHartId   = 0xf14;

// Machine Trap Setup
constexpr uint16_t kCsrMstatus = 0x300;
constexpr uint16_t kCsrSstatus = 0x100;
constexpr uint16_t kCsrUstatus = 0x000;

constexpr uint16_t kCsrMisa       = 0x301; // ISA and extensions
constexpr uint16_t kCsrMedeleg    = 0x302; // Machine exception delegation register
constexpr uint16_t kCsrMideleg    = 0x303; // Machine interrupt delegation register
constexpr uint16_t kCsrMie        = 0x304; // Machine interrupt-enable register
constexpr uint16_t kCsrMtvec      = 0x305; // Machine trap-handler base address
constexpr uint16_t kCsrMcounteren = 0x306; // Machine counter enable
constexpr uint16_t kCsrMstatush   = 0x310; // Additional machine status register, RV32 only

// Machine Trap Handling
constexpr uint16_t kCsrMscratch = 0x340; // Scratch register for machine trap handlers
constexpr uint16_t kCsrMepc     = 0x341; // Machine exception program counter
constexpr uint16_t kCsrMcause   = 0x342; // Machine trap cause
constexpr uint16_t kCsrMtval    = 0x343; // Machine bad address or instruction
constexpr uint16_t kCsrMip      = 0x344; // Machine interrupt pending
constexpr uint16_t kCsrMtinst   = 0x34a; // Machine trap instruction (transformed)
constexpr uint16_t kCsrMtval2   = 0x34b; // Machine bad guest physical address

// Machine Timers and Counters
constexpr uint16_t kCsrMCycle   = 0xb00; // Machine cycle counter
constexpr uint16_t kCsrMinstret = 0xb02; // Machine instructions-retired counter

constexpr uint16_t kCsrMCycleH   = 0xb80; // Upper 32 bits of mcycle, RV32 only.
constexpr uint16_t kCsrMinstretH = 0xb82; // Upper 32 bits of minstret, RV32 only.

constexpr uint16_t kCsrSedeleg = 0x102;
constexpr uint16_t kCsrSideleg = 0x103;
constexpr uint16_t kCsrSie     = 0x104;
constexpr uint16_t kCsrSip     = 0x144;
constexpr uint16_t kCsrUie     = 0x004;
constexpr uint16_t kCsrCycle   = 0xc00;
constexpr uint16_t kCsrTime    = 0xc01;
constexpr uint16_t kCsrSepc    = 0x141;
constexpr uint16_t kCsrUepc    = 0x041;
constexpr uint16_t kCsrScause  = 0x142;
constexpr uint16_t kCsrUcause  = 0x042;

constexpr uint16_t kCsrStval = 0x143; // Machine bad address or instruction
constexpr uint16_t kCsrUtval = 0x043; // Machine bad address or instruction
constexpr uint16_t kCsrStvec = 0x105;
constexpr uint16_t kCsrUtvec = 0x005;

constexpr uint16_t kCsrMeipMask = 0x800;
constexpr uint16_t kCsrMtipMask = 0x080;
constexpr uint16_t kCsrMsipMask = 0x008;
constexpr uint16_t kCsrSeipMask = 0x200;
constexpr uint16_t kCsrStipMask = 0x020;
constexpr uint16_t kCsrSsipMask = 0x002;

} // namespace csr
} // namespace cpu
} // namespace rv64_emulator
#endif