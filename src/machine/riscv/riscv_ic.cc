// EPOS RISC-V IC Mediator Implementation

#include <architecture.h>
#include <machine/machine.h>
#include <machine/ic.h>
#include <machine/timer.h>
#include <process.h>

extern "C" { void _int_entry() __attribute__ ((nothrow, alias("_ZN4EPOS1S2IC5entryEv"))); }
extern "C" { void __exit(); }
extern "C" { static void print_context(); }

__BEGIN_SYS

IC::Interrupt_Handler IC::_int_vector[IC::INTS];

void IC::entry()
{
    // Save context
    CPU::Context::push(true);
    ASM("       la     ra, 1f                   \n"     // set LR to restore context before returning
        "       j      %0                       \n" : : "i"(&dispatch));

    // Entry-point for the dummy contexts used by the first dispatching of newly created threads
    ASM("       .global _int_leave              \n"
        "_int_leave:                            \n");
if(Traits<IC>::hysterically_debugged) {
    ASM("       jalr    %0                      \n" : : "r"(print_context));
}

    // Restore context
    ASM("1:                                     \n");
    CPU::Context::pop(true);
    CPU::iret();
}

void IC::dispatch()
{
    Interrupt_Id id = int_id();

    if((id != INT_SYS_TIMER) || Traits<IC>::hysterically_debugged)
        db<IC>(TRC) << "IC::dispatch(i=" << id << ")" << endl;

    // MIP.MTI is a direct logic on (MTIME == MTIMECMP) and reseting the Timer seems to be the only way to clear it
    if(id == INT_SYS_TIMER)
        Timer::reset();

    _int_vector[id](id);

    if(id >= EXCS)
        CPU::fr(0); // tell CPU::Context::pop(true) not to increment PC since it is automatically incremented for hardware interrupts
}

void IC::int_not(Interrupt_Id id)
{
    db<IC>(WRN) << "IC::int_not(i=" << id << ")" << endl;
    if(Traits<Build>::hysterically_debugged)
        Machine::panic();
}

void IC::exception(Interrupt_Id id)
{
    CPU::Log_Addr epc = CPU::epc();
    CPU::Log_Addr sp = CPU::sp();
    CPU::Reg status = CPU::status();
    CPU::Reg cause = CPU::cause();
    CPU::Reg tval = CPU::tval();
//    Thread * thread = Thread::self();

    db<IC,System>(WRN) << "IC::Exception(" << id << ") => {" << hex << ",epc=" << epc << ",sp=" << sp << ",status=" << status << ",cause=" << cause << ",tval=" << tval << "}" << dec;

    switch(id) {
    case CPU::EXC_IALIGN: // instruction address misaligned
        db<IC, System>(WRN) << " => unaligned instruction";
        break;
    case CPU::EXC_IFAULT: // instruction access fault
        db<IC, System>(WRN) << " => instruction protection violation";
        break;
    case CPU::EXC_IILLEGAL: // illegal instruction
        db<IC, System>(WRN) << " => illegal instruction";
        break;
    case CPU::EXC_BREAK: // break point
        db<IC, System>(WRN) << " => break point";
        break;
    case CPU::EXC_DRALIGN: // load address misaligned
        db<IC, System>(WRN) << " => unaligned data read";
        break;
    case CPU::EXC_DRFAULT: // load access fault
        db<IC, System>(WRN) << " => data protection violation (read)";
        break;
    case CPU::EXC_DWALIGN: // store/AMO address misaligned
        db<IC, System>(WRN) << " => unaligned data write";
        break;
    case CPU::EXC_DWFAULT: // store/AMO access fault
        db<IC, System>(WRN) << " => data protection violation (write)";
        break;
    case CPU::EXC_ENVU: // ecall from user-mode
    case CPU::EXC_ENVS: // ecall from supervisor-mode
    case CPU::EXC_ENVH: // reserved
    case CPU::EXC_ENVM: // reserved
        db<IC, System>(WRN) << " => bad ecall";
        break;
    case CPU::EXC_IPF: // Instruction page fault
        db<IC, System>(WRN) << " => instruction page fault";
        break;
    case CPU::EXC_DRPF: // load page fault
    case CPU::EXC_RES: // reserved
    case CPU::EXC_DWPF: // store/AMO page fault
        db<IC, System>(WRN) << " => data page fault";
        break;
    default:
        int_not(id);
        break;
    }

    db<IC, System>(WRN) << endl;

    if(Traits<Build>::hysterically_debugged)
        db<IC, System>(ERR) << "Exception stopped execution due to hysterically debugging!" << endl;

//    CPU::fr(sizeof(void *)); // tell CPU::Context::pop(true) to perform PC = PC + [4|8] on return

    // Although it is 64 bits architecture, the PC is 32 bits for Risc-V 64
    CPU::fr(4);
}

__END_SYS

static void print_context() {
    __USING_SYS
    db<IC, System>(TRC) << "IC::leave:ctx=" << *static_cast<CPU::Context *>(CPU::sp() + 32) << endl;
    CPU::fr(0);
}

