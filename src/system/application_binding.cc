// EPOS Application Binding

#include <utility/spin.h>
#include <utility/ostream.h>
#include <architecture/cpu.h>
#include <framework/main.h>

// Global objects
__BEGIN_SYS
OStream kerr;
__END_SYS


// Bindings
extern "C" {
    void _panic() {
//        _API::Thread::exit(-1);
        for(;;);
    }
    void _exit(int s) {
//        _API::Thread::exit(s);
        for(;;);
    }
}

__USING_SYS;
extern "C" {
    void _syscall(void * m) {
        CPU::syscall(m);
    }

    int putchar(int character) {
        static volatile int *uart = (int *)(void *)0x10010000;
        while (uart[0] < 0);
        return uart[0] = character & 0xff;
    }

    void _print(const char *s)
    {
        while (*s) putchar(*s++);
    }

    void _print_preamble() {}
    void _print_trailler(bool error) {
//        if(error) _exit(-1);
    }
}
