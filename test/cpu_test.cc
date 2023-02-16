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
#include "mmu.h"
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

    void SetUp() override {
        auto dram = std::make_unique<rv64_emulator::dram::DRAM>(kDramSize);
        auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
        auto sv39 = std::make_unique<rv64_emulator::mmu::Sv39>(std::move(bus));
        auto mmu  = std::make_unique<rv64_emulator::mmu::Mmu>(std::move(sv39));
        auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(rv64_emulator::cpu::PrivilegeMode::kMachine, std::move(mmu));
        m_cpu     = std::move(cpu);
    }

    void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::cpu::CPU> m_cpu;
};

constexpr uint64_t kProgramEntry             = 0;
constexpr uint64_t kArbitrarilyHandlerVector = 0x100000;

TEST_F(CpuTest, HandleTrap) {
    // write ecall instruction
    const uint32_t kEcallWord = 0x00000073;
    m_cpu->Store(kProgramEntry, sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&kEcallWord));
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, kArbitrarilyHandlerVector);
    m_cpu->SetPC(kProgramEntry);
    m_cpu->Tick();

    ASSERT_EQ(kArbitrarilyHandlerVector, m_cpu->GetPC());

    const uint64_t val = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_EQ(rv64_emulator::cpu::trap::kTrapToCauseTable.at(rv64_emulator::cpu::trap::TrapType::kEnvironmentCallFromMMode), val);
}

TEST_F(CpuTest, HandleInterrupt) {
    // write "addi x0, x0, 1" instruction
    const uint32_t kAddiWord = 0x00100013;
    m_cpu->Store(kProgramEntry, sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&kAddiWord));

    const std::map<uint16_t, rv64_emulator::cpu::trap::TrapType> kInterruptTable = {
        { rv64_emulator::cpu::csr::kCsrMeipMask, rv64_emulator::cpu::trap::TrapType::kMachineExternalInterrupt },
        { rv64_emulator::cpu::csr::kCsrMtipMask, rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt },
        { rv64_emulator::cpu::csr::kCsrMsipMask, rv64_emulator::cpu::trap::TrapType::kMachineSoftwareInterrupt },
        { rv64_emulator::cpu::csr::kCsrSeipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorExternalInterrupt },
        { rv64_emulator::cpu::csr::kCsrStipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorTimerInterrupt },
        { rv64_emulator::cpu::csr::kCsrSsipMask, rv64_emulator::cpu::trap::TrapType::kSupervisorSoftwareInterrupt },
    };

    for (const auto [mask, trap_type] : kInterruptTable) {
        m_cpu->SetPC(kProgramEntry);

        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, mask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, mask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, kArbitrarilyHandlerVector);

        m_cpu->Tick();
        // now the interrupt can't be caught because mie is disabled
        ASSERT_EQ(kProgramEntry + 4, m_cpu->GetPC());

        // enable mie
        m_cpu->SetPC(kProgramEntry);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0b1000);
        m_cpu->Tick();
        ASSERT_EQ(kArbitrarilyHandlerVector, m_cpu->GetPC());

        const rv64_emulator::cpu::csr::CauseDesc kCause = {
            .cause     = rv64_emulator::cpu::trap::kTrapToCauseTable.at(trap_type),
            .interrupt = trap_type >= rv64_emulator::cpu::trap::TrapType::kUserSoftwareInterrupt,
        };

        const uint64_t kRealMCauseBits = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMcause);
        ASSERT_TRUE(kCause.interrupt);
        ASSERT_EQ(*reinterpret_cast<const uint64_t*>(&kCause), kRealMCauseBits);

        m_cpu->Reset();
    }
}

TEST_F(CpuTest, Wfi) {
    // store wfi instruction
    const uint32_t kWfiWord = 0x10500073;
    m_cpu->Store(kProgramEntry, sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&kWfiWord));
    m_cpu->SetPC(kProgramEntry);
    m_cpu->Tick();
    ASSERT_EQ(kProgramEntry + 4, m_cpu->GetPC());

    for (int i = 0; i < 10; i++) {
        m_cpu->Tick();
        ASSERT_EQ(kProgramEntry + 4, m_cpu->GetPC());
    }

    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, rv64_emulator::cpu::csr::kCsrMtipMask);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0x8);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, 0);
    m_cpu->Tick();
    ASSERT_EQ(0, m_cpu->GetPC());
}

TEST_F(CpuTest, OfficalTests) {
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
#ifdef DEBUG
            m_cpu->DumpRegisters();
#endif
            uint8_t val = UINT8_MAX;

            const rv64_emulator::cpu::trap::Trap kLoadTrap = m_cpu->Load(kToHostSectionAddr, sizeof(uint8_t), &val);

            if (kLoadTrap.m_trap_type != rv64_emulator::cpu::trap::TrapType::kNone) {
                ASSERT_TRUE(false) << fmt::format("load virt memory failed, addr[{:#018x}]\n", kToHostSectionAddr);
            }

            if (val != 0) {
                EXPECT_EQ(val, 1) << fmt::format("Test {} Failed with .tohost section val = {}", filename, val);
                break;
            }
        }
    }
}
