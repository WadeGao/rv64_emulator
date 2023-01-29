#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "dram.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

class CpuTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        fmt::print("start running CPU test case...\n");
    }

    static void TearDownTestSuite() {
        fmt::print("finish running CPU test case...\n");
    }

    virtual void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
        auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(
            rv64_emulator::cpu::ArchMode::kBit64, rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(bus));
        m_cpu = std::move(cpu);
    }

    virtual void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::cpu::CPU> m_cpu;
};

TEST_F(CpuTest, HandleTrap) {
    const uint64_t handler_vector = 0x10000000;
    // write ecall instruction
    m_cpu->Store(kDramBaseAddr, 32, 0x00000073);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMtvec, handler_vector);
    m_cpu->SetPC(kDramBaseAddr);
    m_cpu->Tick();

    ASSERT_EQ(handler_vector, m_cpu->GetPC());

    const auto& [val, trap] = m_cpu->ReadCsr(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_EQ(trap.m_trap_type, rv64_emulator::cpu::trap::TrapType::kNone);
    ASSERT_EQ(rv64_emulator::cpu::trap::kTrapToCauseTable.at(rv64_emulator::cpu::trap::TrapType::kEnvironmentCallFromMMode), val);
}

TEST_F(CpuTest, HandleInterrupt) {
    const uint64_t handler_vector = 0x10000000;
    // write "addi x0, x0, 1" instruction
    m_cpu->Store(kDramBaseAddr, 32, 0x00100013);
    m_cpu->SetPC(kDramBaseAddr);

    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMie, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMip, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMtvec, handler_vector);

    m_cpu->Tick();
    // now the interrupt can't be caught because mie is disabled
    ASSERT_EQ(kDramBaseAddr + 4, m_cpu->GetPC());

    // enable mie
    m_cpu->SetPC(kDramBaseAddr);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMstatus, 0b1000);
    m_cpu->Tick();
    ASSERT_EQ(handler_vector, m_cpu->GetPC());

    const auto [kIsInterrupt, kExceptedCauseBits] = m_cpu->GetTrapCause({
        .m_trap_type = rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt,
        .m_val       = 0,
    });
    const auto [kRealMCauseBits, kTrap]           = m_cpu->ReadCsr(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_TRUE(kIsInterrupt);
    ASSERT_EQ(kTrap.m_trap_type, rv64_emulator::cpu::trap::TrapType::kNone);
    ASSERT_EQ(kExceptedCauseBits, kRealMCauseBits);
}

TEST_F(CpuTest, Wfi) {
    // store wfi instruction
    m_cpu->Store(kDramBaseAddr, 32, 0x10500073);
    m_cpu->SetPC(kDramBaseAddr);
    m_cpu->Tick();
    ASSERT_EQ(kDramBaseAddr + 4, m_cpu->GetPC());

    for (int i = 0; i < 10; i++) {
        m_cpu->Tick();
        ASSERT_EQ(kDramBaseAddr + 4, m_cpu->GetPC());
    }

    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMie, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMip, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMstatus, 0x8);
    m_cpu->WriteCsr(rv64_emulator::cpu::csr::kCsrMtvec, 0);
    m_cpu->Tick();
    ASSERT_EQ(0, m_cpu->GetPC());
}