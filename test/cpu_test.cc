#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "dram.h"
#include "libs/elf_utils.h"

#include "elfio/elf_types.hpp"
#include "elfio/elfio.hpp"
#include "elfio/elfio_segment.hpp"
#include "fmt/core.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <tuple>

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
        auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(bus));
        m_cpu     = std::move(cpu);
    }

    virtual void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::cpu::CPU> m_cpu;
};

TEST_F(CpuTest, HandleTrap) {
    const uint64_t handler_vector = 0x10000000;
    // write ecall instruction
    m_cpu->Store(kDramBaseAddr, 32, 0x00000073);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, handler_vector);
    m_cpu->SetPC(kDramBaseAddr);
    m_cpu->Tick();

    ASSERT_EQ(handler_vector, m_cpu->GetPC());

    const uint64_t val = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_EQ(rv64_emulator::cpu::trap::kTrapToCauseTable.at(rv64_emulator::cpu::trap::TrapType::kEnvironmentCallFromMMode), val);
}

TEST_F(CpuTest, HandleInterrupt) {
    const uint64_t handler_vector = 0x10000000;
    // write "addi x0, x0, 1" instruction
    m_cpu->Store(kDramBaseAddr, 32, 0x00100013);

    const std::map<uint16_t, rv64_emulator::cpu::trap::TrapType> kInterruptTable = {
        { rv64_emulator::cpu::csr::kCsrMeipMask, rv64_emulator::cpu::trap::TrapType::kMachineExternalInterrupt },
        { rv64_emulator::cpu::csr::kCsrMtipMask, rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt },
        { rv64_emulator::cpu::csr::kCsrMsipMask, rv64_emulator::cpu::trap::TrapType::kMachineSoftwareInterrupt },
        { rv64_emulator::cpu::csr::kCsrSeipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorExternalInterrupt },
        { rv64_emulator::cpu::csr::kCsrStipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorTimerInterrupt },
        { rv64_emulator::cpu::csr::kCsrSsipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorSoftwareInterrupt },
    };

    for (const auto [mask, trap_type] : kInterruptTable) {
        m_cpu->SetPC(kDramBaseAddr);

        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, mask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, mask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, handler_vector);

        m_cpu->Tick();
        // now the interrupt can't be caught because mie is disabled
        ASSERT_EQ(kDramBaseAddr + 4, m_cpu->GetPC());

        // enable mie
        m_cpu->SetPC(kDramBaseAddr);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0b1000);
        m_cpu->Tick();
        ASSERT_EQ(handler_vector, m_cpu->GetPC());

        const auto [kIsInterrupt, kExceptedCauseBits] = rv64_emulator::cpu::trap::GetTrapCauseBits({
            .m_trap_type = trap_type,
            .m_val       = 0,
        });
        const uint64_t kRealMCauseBits                = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMcause);
        ASSERT_TRUE(kIsInterrupt);
        ASSERT_EQ(kExceptedCauseBits, kRealMCauseBits);

        m_cpu->Reset();
    }
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

    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0x8);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, 0);
    m_cpu->Tick();
    ASSERT_EQ(0, m_cpu->GetPC());
}

TEST_F(CpuTest, OfficalTests) {
    const char* filename = "/Users/wade/Desktop/rv64_emulator/third_party/riscv-tests/isa/rv64ui-p-add";

    const char* kElfDir = "test/elf";

    std::filesystem::path p(kElfDir);
    ASSERT_TRUE(std::filesystem::exists(p)) << kElfDir << " not exists\n";
    ASSERT_TRUE(std::filesystem::directory_entry(kElfDir).is_directory()) << kElfDir << " is not a directory\n";

    for (const auto& item : std::filesystem::directory_iterator(kElfDir)) {
        const std::string& filename = fmt::format("{}/{}", kElfDir, item.path().filename().filename().c_str());

        ELFIO::elfio reader;
        reader.load(filename);

        const auto [kToHostSectionExist, kToHostSectionAddr] = rv64_emulator::libs::ElfUtils::CheckSectionExist(reader, ".tohost\0");
        ASSERT_TRUE(kToHostSectionExist) << "input file is not offical test case";

        rv64_emulator::libs::ElfUtils::LoadElf(reader, m_cpu.get());

        const uint64_t kEntryAddr = reader.get_entry();
        m_cpu->SetPC(kEntryAddr);

        fmt::print("now start run {}\n", filename);

        while (true) {
            m_cpu->Tick();

            const uint64_t val = m_cpu->Load(kToHostSectionAddr, 8);
            if (val != 0) {
                m_cpu->DumpRegisters();
                EXPECT_EQ(val, 1) << fmt::format("Test {} Failed with .tohost section val = {}", filename, val);
                break;
            }
        }
    }
}
