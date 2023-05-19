// EPOS Task Implementation

#include <process.h>
#include <framework/main.h>

__BEGIN_SYS

Task * volatile Task::_current;

Task::~Task()
{
    db<Task>(INF) << "~Task" << endl;
    db<Task>(TRC) << "~Task(this=" << this << ")" << endl;

    while(!_threads.empty())
        delete _threads.remove()->object();

    delete _as;
}

__END_SYS
