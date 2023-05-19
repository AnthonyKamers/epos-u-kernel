// EPOS RISC-V 32System Call Function Implementation

#include <architecture/rv64/rv64_cpu.h>
#include <machine/ic.h>

__BEGIN_SYS

void CPU::syscall(void *message)
{
    CPU::a1(reinterpret_cast<CPU::Reg>(message));
    CPU::ecall();
}

__END_SYS
