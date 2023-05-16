// EPOS Application Binding

#include <utility/spin.h>
#include <utility/ostream.h>
#include <architecture/cpu.h>
#include <system.h>
#include <framework/main.h>

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
    // void _syscall(void * m) { CPU::syscall(m); }
    void _print(const char * s);
    void _print_preamble() {}
    void _print_trailler(bool error) { if(error) _exit(-1); }
}
