// EPOS Task Implementation

#include <process.h>

__BEGIN_SYS

Task * volatile Task::_current;

Task::~Task()
{
    db<Task>(INF) << "~Task" << endl;
    db<Task>(TRC) << "~Task(this=" << this << ")" << endl;

    while(!_threads.empty()){
        Thread * thread = _threads.remove()->object();
        if (thread != _current->main()) { //Checar pra não deletar idle?
            db<Task>(INF) << "Deletou thread da Task" << endl;
            delete _threads.remove()->object();
        }
        else
            db<Task>(INF) << "Achou a thread atual, nã deleta" << endl;
    }

    //Vai dar failed pra task main, já que não atacha os segments
    _as->detach(_ds);
    _as->detach(_cs);

    delete _as;
}

__END_SYS
