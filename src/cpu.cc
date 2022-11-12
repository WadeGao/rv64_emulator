#include "include/cpu.h"
#include "include/bus.h"
#include "include/conf.h"
#include "include/decoder.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace rv64_emulator {
namespace cpu {

CPU::CPU(std::unique_ptr<rv64_emulator::bus::Bus> bus)
    : m_pc(DRAM_BASE)
    , m_bus(std::move(bus)) {
    memset(m_reg, 0, sizeof(uint64_t) * RV64_REGISTER_NUM);
    m_reg[0] = 0;
    m_reg[2] = DRAM_BASE + DRAM_SIZE;
#ifdef DEBUG
    printf("cpu init, bus addr is %p\n", m_bus.get());
#endif
}

uint32_t CPU::Fetch() const {
    uint32_t instruction = m_bus->Load(m_pc, RV64_INSTRUCTION_BIT_SIZE);
    return instruction;
}

uint64_t CPU::Load(const uint64_t addr, const uint64_t bit_size) const {
    return m_bus->Load(addr, bit_size);
}

void CPU::Store(const uint64_t addr, const uint64_t bit_size, const uint64_t val) {
    m_bus->Store(addr, bit_size, val);
}

uint64_t CPU::Execute(const uint32_t instruction) {
    const uint8_t opcode = rv64_emulator::decoder::GetOpCode(instruction);
    const uint8_t funct7 = rv64_emulator::decoder::GetFunct7(instruction);
    const uint8_t funct3 = rv64_emulator::decoder::GetFunct3(instruction);

    Exec_ADDI(instruction);

    // in a emulator, there is not a GND hardwiring x0 to zero
    m_reg[0] = 0;

    return 0;
}

void CPU::Exec_ADDI(const uint32_t instruction) {
    const uint8_t  rd  = rv64_emulator::decoder::GetRd(instruction);
    const uint8_t  rs1 = rv64_emulator::decoder::GetRs1(instruction);
    const uint32_t imm = rv64_emulator::decoder::GetImm(instruction, rv64_emulator::decoder::RV64InstructionFormatType::I_Type);

    m_reg[rd] = m_reg[rs1] + static_cast<uint64_t>(imm);
#ifdef DEBUG
    printf("addi x%u, x%u, 0x%x\n", rd, rs1, imm);
#endif
}

void CPU::Exec_SLLI(const uint32_t instruction) {
}

void CPU::Exec_SLTI(const uint32_t instruction) {
}

void CPU::Exec_SLTIU(const uint32_t instruction) {
}

void CPU::Exec_XORI(const uint32_t instruction) {
}

void CPU::Exec_SRLI(const uint32_t instruction) {
}

void CPU::Exec_SRAI(const uint32_t instruction) {
}

void CPU::Exec_ORI(const uint32_t instruction) {
}

void CPU::Exec_ANDI(const uint32_t instruction) {
}

CPU::~CPU() {
#ifdef DEBUG
    printf("destroy a cpu\n");
#endif
}

} // namespace cpu
} // namespace rv64_emulator