// EPOS CPU Scheduler Component Implementation

#include <process.h>
#include <time.h>

__BEGIN_SYS

// The following Scheduling Criteria depend on Alarm, which is not available at scheduler.h
template <typename ... Tn>
FCFS::FCFS(int p, Tn & ... an): Priority((p == IDLE) ? IDLE : Alarm::elapsed()) {}

// Since the definition above is only known to this unit, forcing its instantiation
// here so it gets emitted in scheduler.o for subsequent linking with other units is necessary.
template FCFS::FCFS<>(int p);

//DM::DM(const Microsecond & d, const Microsecond & p, const Microsecond & c, unsigned int) : Real_Time_Scheduler_Common(Alarm::ticks(d), Alarm::ticks(d), p, c){}

__END_SYS
