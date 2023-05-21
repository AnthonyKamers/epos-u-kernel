// EPOS RISC-V 64 System Call Entry Implementation

#include <architecture/rv64/rv64_cpu.h>
#include <machine/ic.h>

extern "C" { void _exec(void *); }

__BEGIN_SYS

void CPU::syscalled(unsigned int int_id) {
    // We get here when an APP triggers INT_SYSCALL (i.e. ecall)
    if(Traits<Build>::MODE == Traits<Build>::KERNEL) {
        _exec(reinterpret_cast<void *>(CPU::a1())); // the message to EPOS Framework is passed on register a1
        CPU::a0(sizeof(void *));                    // tell IC::entry to perform PC = PC + 4 on return
    }
}
__END_SYS
