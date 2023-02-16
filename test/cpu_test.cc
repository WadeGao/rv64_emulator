#include "bus.h"
#include "conf.h"
#include "cpu/cpu.h"
#include "cpu/csr.h"
#include "cpu/trap.h"
#include "dram.h"

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

static std::tuple<bool, ELFIO::Elf64_Addr> CheckSectionExist(const ELFIO::elfio& reader, const char* section_name) {
    const ELFIO::Elf_Half kShStrIndex = reader.get_section_name_str_index();
    const ELFIO::Elf_Half kSectionNum = reader.sections.size();

    if (ELFIO::SHN_UNDEF != kShStrIndex) {
        ELFIO::string_section_accessor str_reader(reader.sections[kShStrIndex]);
        for (ELFIO::Elf_Half i = 0; i < kSectionNum; ++i) {
            const ELFIO::Elf_Word kSectionOffset = reader.sections[i]->get_name_string_offset();

            const char* p = str_reader.get_string(kSectionOffset);
            if (strcmp(p, section_name) == 0) {
                const ELFIO::Elf64_Addr kSectionAddr = reader.sections[i]->get_address();
                return {
                    true,
                    kSectionAddr,
                };
            }
        }
    }

    return { false, 0 };
}

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
    ELFIO::elfio reader;
    reader.load("/Users/wade/Desktop/rv64_emulator/third_party/riscv-tests/isa/rv64ui-p-add");

    const auto [kToHostSectionExist, kToHostSectionAddr] = CheckSectionExist(reader, ".tohost\0");

    ASSERT_TRUE(kToHostSectionExist) << "input file is not offical test case";

    const ELFIO::Elf_Half kSegmentNum = reader.segments.size();
    for (uint16_t i = 0; i < kSegmentNum; i++) {
        const ELFIO::segment* segment      = reader.segments[i];
        const ELFIO::Elf_Word kSegmentType = segment->get_type();
        if (kSegmentType != ELFIO::PT_LOAD) {
            continue;
        }

        const ELFIO::Elf_Xword kSegFileSize = segment->get_file_size();
        const ELFIO::Elf_Xword kSegMemize   = segment->get_memory_size();
        const ELFIO::Elf_Xword kSegAddr     = segment->get_virtual_address();

        char const* bytes = segment->get_data();
        for (ELFIO::Elf_Xword i = kSegAddr; i < kSegAddr + kSegMemize; i++) {
            const uint8_t byte = i < kSegAddr + kSegFileSize ? bytes[i - kSegAddr] : 0;
            m_cpu->Store(i, 8, byte);
        }
    }

    const uint64_t kEntryAddr = reader.get_entry();
    m_cpu->SetPC(kEntryAddr);

    while (true) {
        m_cpu->Tick();

        const uint64_t val = m_cpu->Load(kToHostSectionAddr, 8);
        if (val != 0) {
            ASSERT_EQ(val, 1) << "Test Failed with .tohost section val = " << val;
            break;
        }
    }
}
