#pragma once

#include <cstdint>
#include <vector>

namespace rv64_emulator::cpu::csr {

constexpr uint64_t kCsrCapacity = 4096;

constexpr uint16_t kCsrFflags = 0x001;  // Floating-Point Accrued Exceptions.
constexpr uint16_t kCsrFrm = 0x002;     // Floating-Point Dynamic Rounding Mode

// something about fcsr:
// refernce: https://zhuanlan.zhihu.com/p/339497543
// format: https://pic4.zhimg.com/v2-f247fb6ddaed4a7bfe5acf93e8227537_r.jpg
constexpr uint16_t kCsrFcsr =
    0x003;  // Floating-Point Control and Status Register (frm + fflags)

// Machine Info Registers
constexpr uint16_t kCsrMVendorId = 0xf11;
constexpr uint16_t kCsrMArchId = 0xf12;
constexpr uint16_t kCsrMImpId = 0xf13;
constexpr uint16_t kCsrMHartId = 0xf14;

// Machine Trap Setup
constexpr uint16_t kCsrMstatus = 0x300;
constexpr uint16_t kCsrSstatus = 0x100;
constexpr uint16_t kCsrUstatus = 0x000;

constexpr uint16_t kCsrMisa = 0x301;  // ISA and extensions
constexpr uint16_t kCsrMedeleg =
    0x302;  // Machine exception delegation register
constexpr uint16_t kCsrMideleg =
    0x303;                             // Machine interrupt delegation register
constexpr uint16_t kCsrMie = 0x304;    // Machine interrupt-enable register
constexpr uint16_t kCsrMtvec = 0x305;  // Machine trap-handler base address
constexpr uint16_t kCsrMcounteren = 0x306;  // Machine counter enable
constexpr uint16_t kCsrMstatush =
    0x310;  // Additional machine status register, RV32 only

// Machine Trap Handling
constexpr uint16_t kCsrMscratch =
    0x340;  // Scratch register for machine trap handlers
constexpr uint16_t kCsrMepc = 0x341;    // Machine exception program counter
constexpr uint16_t kCsrMcause = 0x342;  // Machine trap cause
constexpr uint16_t kCsrMtval = 0x343;   // Machine bad address or instruction
constexpr uint16_t kCsrMip = 0x344;     // Machine interrupt pending
constexpr uint16_t kCsrMtinst =
    0x34a;  // Machine trap instruction (transformed)
constexpr uint16_t kCsrMtval2 = 0x34b;  // Machine bad guest physical address

// Machine Timers and Counters
constexpr uint16_t kCsrMCycle = 0xb00;  // Machine cycle counter
constexpr uint16_t kCsrMinstret =
    0xb02;  // Machine instructions-retired counter

constexpr uint16_t kCsrMCycleH = 0xb80;  // Upper 32 bits of mcycle, RV32 only.
constexpr uint16_t kCsrMinstretH =
    0xb82;  // Upper 32 bits of minstret, RV32 only.

constexpr uint16_t kCsrSedeleg = 0x102;
constexpr uint16_t kCsrSideleg = 0x103;
constexpr uint16_t kCsrSie = 0x104;
constexpr uint16_t kCsrSip = 0x144;
constexpr uint16_t kCsrUie = 0x004;
constexpr uint16_t kCsrCycle = 0xc00;
constexpr uint16_t kCsrTime = 0xc01;
constexpr uint16_t kCsrSepc = 0x141;
constexpr uint16_t kCsrUepc = 0x041;
constexpr uint16_t kCsrScause = 0x142;
constexpr uint16_t kCsrUcause = 0x042;

constexpr uint16_t kCsrStval = 0x143;
constexpr uint16_t kCsrUtval = 0x043;
constexpr uint16_t kCsrStvec = 0x105;
constexpr uint16_t kCsrUtvec = 0x005;

constexpr uint16_t kCsrMeipMask = 0x800;
constexpr uint16_t kCsrSeipMask = 0x200;

constexpr uint64_t kCsrMInterruptMask = 0xaaa;
constexpr uint64_t kCsrSInterruptMask = 0x222;
constexpr uint64_t kCsrSExceptionMask = 0xf7ff;

constexpr uint64_t kCsrSatp = 0x180;

constexpr uint64_t kCsrTselect = 0x7a0;
constexpr uint64_t kCsrTdata1 = 0x7a1;

using SatpDesc = struct SatpDesc {
  uint64_t ppn : 44;
  uint64_t asid : 16;
  uint64_t mode : 4;
};

using MstatusDesc = struct MstatusDesc {
  // https://blog.csdn.net/dai_xiangjun/article/details/123373456
  uint64_t blank0 : 1;
  uint64_t sie : 1;  // supervisor interrupt enable
  uint64_t blank1 : 1;
  uint64_t mie : 1;  // machine interrupt enable
  uint64_t blank2 : 1;
  uint64_t spie : 1;  // sie prior to trapping
  uint64_t ube : 1;   // u big-endian, zero
  uint64_t mpie : 1;  // mie prior to trapping
  uint64_t spp : 1;   // supervisor previous privilege mode.
  uint64_t vs : 2;    // without vector, zero
  uint64_t mpp : 2;   // machine previous privilege mode.
  uint64_t fs : 2;    // without float, zero
  uint64_t xs : 2;    // without user ext, zero
  uint64_t mprv : 1;  // Modify PRiVilege (Turn on virtual memory and protection
                      // for load/store in M-Mode) when mpp is not M-Mode
  // mprv will be used by OpenSBI.
  uint64_t sum : 1;  // permit Supervisor User Memory access
  uint64_t mxr : 1;  // Make eXecutable Readable
  uint64_t tvm : 1;  // Trap Virtual Memory (raise trap when sfence.vma and
                     // sinval.vma executing in S-Mode)
  uint64_t tw : 1;   // Timeout Wait for WFI
  uint64_t tsr : 1;  // Trap SRET
  uint64_t blank3 : 9;
  uint64_t uxl : 2;  // user xlen
  uint64_t sxl : 2;  // supervisor xlen
  uint64_t sbe : 1;  // s big-endian
  uint64_t mbe : 1;  // m big-endian
  uint64_t blank4 : 25;
  uint64_t sd : 1;  // no vs,fs,xs, zero
};

using SstatusDesc = struct SstatusDesc {
  uint64_t blank0 : 1;
  uint64_t sie : 1;  // supervisor interrupt enable
  uint64_t blank1 : 3;
  uint64_t spie : 1;    // sie prior to trapping
  uint64_t ube : 1;     // u big-endian, zero
  uint64_t blank2 : 1;  // mie prior to trapping
  uint64_t spp : 1;     // supervisor previous privilege mode.
  uint64_t vs : 2;      // without vector, zero
  uint64_t blank3 : 2;  // machine previous privilege mode.
  uint64_t fs : 2;      // without float, zero
  uint64_t xs : 2;      // without user ext, zero
  uint64_t blank4 : 1;
  uint64_t sum : 1;  // permit Supervisor User Memory access
  uint64_t mxr : 1;  // Make eXecutable Readable
  uint64_t blank5 : 12;
  uint64_t uxl : 2;  // user xlen
  uint64_t blank6 : 29;
  uint64_t sd : 1;  // no vs,fs,xs, zero
};

using UstatusDesc = struct UstatusDesc {
  uint64_t uie : 1;
  uint64_t blank0 : 3;
  uint64_t upie : 1;
  uint64_t blank1 : 59;
};

using MieDesc = struct MieDesc {
  uint64_t usie : 1;
  uint64_t ssie : 1;
  uint64_t blank0 : 1;
  uint64_t msie : 1;

  uint64_t utie : 1;
  uint64_t stie : 1;
  uint64_t blank1 : 1;
  uint64_t mtie : 1;

  uint64_t ueie : 1;
  uint64_t seie : 1;
  uint64_t blank2 : 1;
  uint64_t meie : 1;

  uint64_t blank3 : 52;
};

using MipDesc = struct MipDesc {
  uint64_t usip : 1;
  uint64_t ssip : 1;
  uint64_t blank0 : 1;
  uint64_t msip : 1;

  uint64_t utip : 1;
  uint64_t stip : 1;
  uint64_t blank1 : 1;
  uint64_t mtip : 1;

  uint64_t ueip : 1;
  uint64_t seip : 1;
  uint64_t blank2 : 1;
  uint64_t meip : 1;

  uint64_t blank3 : 52;
};

using SieDesc = struct SieDesc {
  uint64_t usie : 1;
  uint64_t ssie : 1;
  uint64_t blank0 : 2;

  uint64_t utie : 1;
  uint64_t stie : 1;
  uint64_t blank1 : 2;

  uint64_t ueie : 1;
  uint64_t seie : 1;

  uint64_t blank2 : 54;
};

using UieDesc = struct UieDesc {
  uint64_t usie : 1;
  uint64_t blank0 : 3;

  uint64_t utie : 1;
  uint64_t blank1 : 3;

  uint64_t ueie : 1;

  uint64_t blank2 : 55;
};

using CauseDesc = struct CauseDesc {
  uint64_t cause : 63;
  uint64_t interrupt : 1;
};

using MisaDesc = struct MisaDesc {
  uint64_t A : 1;
  uint64_t B : 1;
  uint64_t C : 1;
  uint64_t D : 1;
  uint64_t E : 1;
  uint64_t F : 1;
  uint64_t G : 1;
  uint64_t H : 1;
  uint64_t I : 1;
  uint64_t J : 1;
  uint64_t K : 1;
  uint64_t L : 1;
  uint64_t M : 1;
  uint64_t N : 1;
  uint64_t O : 1;
  uint64_t P : 1;
  uint64_t Q : 1;
  uint64_t R : 1;
  uint64_t S : 1;
  uint64_t T : 1;
  uint64_t U : 1;
  uint64_t V : 1;
  uint64_t W : 1;
  uint64_t X : 1;
  uint64_t Y : 1;
  uint64_t Z : 1;
  uint64_t blank : 36;
  uint64_t mxl : 2;
};

// SXL/UXL取值范围：https://www.mail-archive.com/qemu-devel@nongnu.org/msg845227.html
enum class RiscvMXL : uint64_t {
  kRv32 = 1,
  kRv64 = 2,
  kRv128 = 3,
};

class State {
 private:
  bool wfi_;
  std::vector<uint64_t> csr_;

 public:
  State();
  uint64_t Read(const uint64_t addr) const;
  void Write(const uint64_t addr, const uint64_t val);

  bool GetWfi() const;
  void SetWfi(const bool wfi);

  void Reset();
};

}  // namespace rv64_emulator::cpu::csr
