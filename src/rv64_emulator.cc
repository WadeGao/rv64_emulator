#include "include/bus.h"
#include "include/conf.h"
#include "include/cpu.h"
#include "include/dram.h"

#include <memory>

int main() {
    auto dram = std::make_unique<rv64_emulator::dram::DRAM>(DRAM_SIZE);
    auto bus  = std::make_unique<rv64_emulator::bus::Bus>(std::move(dram));
    auto cpu  = std::make_unique<rv64_emulator::cpu::CPU>(std::move(bus));

    /*
    test.o:     file format elf64-littleriscv


    Disassembly of section .text:

    0000000000000000 <f>:
    0:	fd010113          	addi	sp,sp,-48
    4:	02813423          	sd	s0,40(sp)
    8:	03010413          	addi	s0,sp,48
    c:	fca43c23          	sd	a0,-40(s0)
    10:	fd843703          	ld	a4,-40(s0)
    14:	1ed00793          	li	a5,493
    18:	00e7ea63          	bltu	a5,a4,2c <.L2>
    1c:	fd843783          	ld	a5,-40(s0)
    20:	01f7d793          	srli	a5,a5,0x1f
    24:	fef42623          	sw	a5,-20(s0)
    28:	0100006f          	j	38 <.L4>

    000000000000002c <.L2>:
    2c:	fd843783          	ld	a5,-40(s0)
    30:	00d79793          	slli	a5,a5,0xd
    34:	fef42623          	sw	a5,-20(s0)

    0000000000000038 <.L4>:
    38:	00000013          	nop
    3c:	02813403          	ld	s0,40(sp)
    40:	03010113          	addi	sp,sp,48
    44:	00008067          	ret

    0000000000000048 <main>:
    48:	ff010113          	addi	sp,sp,-16
    4c:	00113423          	sd	ra,8(sp)
    50:	00813023          	sd	s0,0(sp)
    54:	01010413          	addi	s0,sp,16
    58:	00000797          	auipc	a5,0x0
    5c:	00078793          	mv	a5,a5
    60:	0007b783          	ld	a5,0(a5) # 58 <main+0x10>
    64:	00078513          	mv	a0,a5
    68:	00000097          	auipc	ra,0x0
    6c:	000080e7          	jalr	ra # 68 <main+0x20>
    70:	00000793          	li	a5,0
    74:	00078513          	mv	a0,a5
    78:	00813083          	ld	ra,8(sp)
    7c:	00013403          	ld	s0,0(sp)
    80:	01010113          	addi	sp,sp,16
    84:	00008067          	ret
    */
    cpu->Execute(0x1235b513);

    return 0;
}