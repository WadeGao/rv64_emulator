#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "dram.h"
#include "libs/utils.h"
#include "mmu.h"

#include "elfio/elf_types.hpp"
#include "elfio/elfio.hpp"
#include "elfio/elfio_segment.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
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
        auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(std::move(mmu));
        m_cpu     = std::move(cpu);
    }

    void TearDown() override {
    }

    std::unique_ptr<rv64_emulator::cpu::CPU> m_cpu;
};

constexpr uint64_t kProgramEntry             = 0;
constexpr uint64_t kMaxInstructions          = 100000;
constexpr uint64_t kArbitrarilyHandlerVector = 0x100000;

TEST_F(CpuTest, HandleTrap) {
    // write ecall instruction
    const uint32_t kEcallWord = 0x00000073;
    m_cpu->Store(kProgramEntry, sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&kEcallWord));
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, kArbitrarilyHandlerVector);
    m_cpu->SetPC(kProgramEntry);
    m_cpu->Tick();

    ASSERT_EQ(kArbitrarilyHandlerVector, m_cpu->GetPC());

    const uint64_t kVal = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMcause);
    ASSERT_EQ(rv64_emulator::cpu::trap::kTrapToCauseTable.at(rv64_emulator::cpu::trap::TrapType::kEnvironmentCallFromMMode), kVal);
}

TEST_F(CpuTest, HandleInterrupt) {
    for (const auto t : {
             rv64_emulator::cpu::trap::TrapType::kMachineExternalInterrupt,
             rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt,
             rv64_emulator::cpu::trap::TrapType::kMachineSoftwareInterrupt,
             rv64_emulator::cpu::trap::TrapType::kSupervisorExternalInterrupt,
             rv64_emulator::cpu::trap::TrapType::kSupervisorTimerInterrupt,
             rv64_emulator::cpu::trap::TrapType::kSupervisorSoftwareInterrupt,
         }) {
        // write "addi x0, x0, 1" instruction
        constexpr uint32_t kAddiWord = 0x00100013;
        m_cpu->Store(kProgramEntry, sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&kAddiWord));
        m_cpu->SetPC(kProgramEntry);

        const uint64_t kMask = rv64_emulator::libs::util::TrapToMask(t);

        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, kMask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, kMask);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMtvec, kArbitrarilyHandlerVector);

        m_cpu->Tick();

        // mie is disabled
        ASSERT_EQ(kProgramEntry + 4, m_cpu->GetPC()) << fmt::format("trap type: {}", static_cast<uint64_t>(t));

        // enable mie
        m_cpu->SetPC(kProgramEntry);
        m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMstatus, 0b1000);
        m_cpu->Tick();

        ASSERT_EQ(kArbitrarilyHandlerVector, m_cpu->GetPC());

        const rv64_emulator::cpu::csr::CauseDesc kCause = {
            .cause     = rv64_emulator::cpu::trap::kTrapToCauseTable.at(t),
            .interrupt = static_cast<uint64_t>(t >= rv64_emulator::cpu::trap::TrapType::kUserSoftwareInterrupt),
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

    constexpr auto kTrap     = rv64_emulator::cpu::trap::TrapType::kMachineTimerInterrupt;
    const uint64_t kMtipMask = rv64_emulator::libs::util::TrapToMask(kTrap);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMie, kMtipMask);
    m_cpu->m_state.Write(rv64_emulator::cpu::csr::kCsrMip, kMtipMask);
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
        const auto& kFileName = fmt::format("{}/{}", kElfDir, item.path().filename().filename().c_str());

        ELFIO::elfio reader;
        reader.load(kFileName);

        ELFIO::Elf64_Addr tohost_addr = 0;

        const bool kToHostSectionExist = rv64_emulator::libs::util::CheckSectionExist(reader, ".tohost\0", tohost_addr);

        ASSERT_TRUE(kToHostSectionExist) << "input file is not offical test case";

        m_cpu->Reset();
        rv64_emulator::libs::util::LoadElf(reader, m_cpu.get());

        const uint64_t kEntryAddr = reader.get_entry();
        m_cpu->SetPC(kEntryAddr);

        fmt::print("now start run {}\n", kFileName);

        while (true) {
            m_cpu->Tick();
#ifdef DEBUG
            m_cpu->DumpRegisters();
#endif
            uint8_t val = UINT8_MAX;

            const bool kSucc = m_cpu->m_mmu->m_sv39->m_bus->Load(tohost_addr, sizeof(uint8_t), &val);
            ASSERT_TRUE(kSucc);

            if (val != 0) {
                EXPECT_EQ(val, 1) << fmt::format(fmt::fg(fmt::color::red), "{} Failed with .tohost section val = {}", kFileName, val);
                break;
            }

            const uint64_t kCycles = m_cpu->m_state.Read(rv64_emulator::cpu::csr::kCsrMCycle);
            if (kCycles > kMaxInstructions) {
                EXPECT_TRUE(false) << fmt::format(fmt::fg(fmt::color::red), "{} timeout!", kFileName);
                break;
            }
        }
    }
}
