#include "include/bus.h"
#include "include/conf.h"
#include "include/cpu.h"
#include "include/csr.h"
#include "include/dram.h"
#include "gtest/gtest.h"
#include <memory>

TEST(CpuTest, Trap) {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(
        rv64_emulator::cpu::ArchMode::kBit64, rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(bus));

    const uint64_t handler_vector = 0x10000000;
    // write ecall instruction
    cpu->Store(kDramBaseAddr, 32, 0x00000073);
    cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMtvec, handler_vector);
    cpu->SetPC(kDramBaseAddr);
    cpu->Tick();

    ASSERT_EQ(handler_vector, cpu->GetPC());

    const auto& [val, trap] = cpu->ReadCsr(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_EQ(trap.m_trap_type, rv64_emulator::cpu::TrapType::kNone);
    ASSERT_EQ(rv64_emulator::cpu::TrapToCauseTable.at(rv64_emulator::cpu::TrapType::kEnvironmentCallFromMMode), val);
}
