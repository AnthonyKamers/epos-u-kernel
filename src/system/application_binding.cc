// EPOS Application Binding

#include <utility/spin.h>
#include <utility/ostream.h>
#include <architecture/cpu.h>
#include <system.h>
#include <framework/main.h>

// Framework class attributes
__BEGIN_SYS
Framework::Cache Framework::_cache;
__END_SYS

// Global objects
__BEGIN_SYS
OStream kerr;
__END_SYS


// Bindings
extern "C" {
    void _panic() { _API::Thread::exit(-1); }
    void _exit(int s) { _API::Thread::exit(s); for(;;); }
}

__USING_SYS;
extern "C" {
    void _syscall(void * m) { CPU::syscall(m); }

//    void _print(const char * s) {
//        Message msg(Id(UTILITY_ID, 0), Message::PRINT, reinterpret_cast<unsigned long>(s));
//        msg.act();
//    }

    // backup method of printting out stuff (it goes directly, so it does not need system calls)
    int putchar(int character) {
        static volatile int *uart = (int *)(void *)0x10010000;
        while (uart[0] < 0);
        return uart[0] = character & 0xff;
    }

    void _print(const char *s) {
        while (*s) putchar(*s++);
    }

    void _print_preamble() {}
    void _print_trailler(bool error) {
        if(error) _exit(-1);
    }
}
