// EPOS Thread Component Declarations

#ifndef __process_h
#define __process_h

#include <architecture.h>
#include <machine.h>
#include <utility/queue.h>
#include <utility/handler.h>
#include <memory.h>
#include <scheduler.h>

extern "C" { void __exit(); }

__BEGIN_SYS

class Thread
{
    friend class Init_End;              // context->load()
    friend class Init_System;           // for init() on CPU != 0
    friend class Scheduler<Thread>;     // for link()
    friend class Synchronizer_Common;   // for lock() and sleep()
    friend class Alarm;                 // for lock()
    friend class System;                // for init()
    friend class IC;                    // for link() for priority ceiling

protected:
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const bool multitask = Traits<System>::multitask;
    static const bool reboot = Traits<System>::reboot;

    static const unsigned int QUANTUM = Traits<Thread>::QUANTUM;
    static const unsigned int STACK_SIZE = multitask ? Traits<System>::STACK_SIZE : Traits<Application>::STACK_SIZE;
    static const unsigned int USER_STACK_SIZE = Traits<Application>::STACK_SIZE;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Context Context;

public:
    // Thread State
    enum State {
        RUNNING,
        READY,
        SUSPENDED,
        WAITING,
        FINISHING
    };

    // Thread Scheduling Criterion
    typedef Traits<Thread>::Criterion Criterion;
    enum {
        HIGH    = Criterion::HIGH,
        NORMAL  = Criterion::NORMAL,
        LOW     = Criterion::LOW,
        MAIN    = Criterion::MAIN,
        IDLE    = Criterion::IDLE
    };

    // Thread Queue
    typedef Ordered_Queue<Thread, Criterion, Scheduler<Thread>::Element> Queue;

    // Thread Configuration
    // t = 0 => Task::self()
    // ss = 0 => user-level stack on an auto expand segment
    struct Configuration {
        Configuration(const State & s = READY, const Criterion & c = NORMAL, Task * t = 0, unsigned int ss = STACK_SIZE)
        : state(s), criterion(c), task(t), stack_size(ss) {}

        State state;
        Criterion criterion;
        Task * task;
        unsigned int stack_size;
    };


public:
    template<typename ... Tn>
    Thread(int (* entry)(Tn ...), Tn ... an);
    template<typename ... Tn>
    Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an);
    ~Thread();

    const volatile State & state() const { return _state; }
    const volatile Criterion::Statistics & statistics() { return criterion().statistics(); }

    const volatile Criterion & priority() const { return _link.rank(); }
    void priority(const Criterion & p);

    Task * task() const { return _task; }

    int join();
    void pass();
    void suspend();
    void resume();

    static Thread * volatile self() { return running(); }
    static void yield();
    static void exit(int status = 0);

protected:
    void constructor_prologue(unsigned int stack_size);
    void constructor_epilogue(Log_Addr entry, unsigned int stack_size);

    Criterion & criterion() { return const_cast<Criterion &>(_link.rank()); }
    Queue::Element * link() { return &_link; }

    static Thread * volatile running() { return _scheduler.chosen(); }

    static void lock() { CPU::int_disable(); }
    static void unlock() { CPU::int_enable(); }
    static bool locked() { return CPU::int_disabled(); }

    static void sleep(Queue * q);
    static void wakeup(Queue * q);
    static void wakeup_all(Queue * q);

    static void reschedule();
    static void time_slicer(IC::Interrupt_Id interrupt);

    static void dispatch(Thread * prev, Thread * next, bool charge = true);

    static int idle();

private:
    static void init();

protected:
    Task * _task;
    Segment * _user_stack;

    char * _stack;
    Context * volatile _context;
    volatile State _state;
    Queue * _waiting;
    Thread * volatile _joining;
    Queue::Element _link;

    static volatile unsigned int _thread_count;
    static Scheduler_Timer * _timer;
    static Scheduler<Thread> _scheduler;
};


// Task (only used in multitasking configurations)
class Task
{
    friend class Thread;        // for insert()

private:
    static const bool multitask = Traits<System>::multitask;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Context Context;
    typedef Thread::Queue Queue;

protected:
    // This constructor is only used by Thread::init()
    template<typename ... Tn>
    Task(Address_Space * as, Segment * cs, Segment * ds, Log_Addr code, Log_Addr data, int (* entry)(Tn ...), Tn ... an)
    : _as(as), _cs(cs), _ds(ds), _code(code), _data(data), _entry(entry) {
        db<Task, Init>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",code=" << _code << ",data=" << _data << ",entry=" << _entry << ") => " << this << endl;
        // Start first task as thread
        _current = this;
        activate();
        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN, this, 0), entry, an...);
    }

public:
    template<typename ... Tn>
    Task(Segment * cs, Segment * ds, Log_Addr code, Log_Addr data, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _code(_as->attach(_cs, code)), _data(_as->attach(_ds, data)), _entry(entry) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;
        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::MAIN, this, 0), entry, an...);
    }
    
    template<typename ... Tn>
    Task(const Thread::Configuration & conf, Segment * cs, Segment * ds, Log_Addr code, Log_Addr data, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _code(_as->attach(_cs, code)), _data(_as->attach(_ds, data)), _entry(entry) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;
        _main = new (SYSTEM) Thread(Thread::Configuration(conf.state, conf.criterion, this, 0), entry, an...);
    }
    
    template<typename ... Tn>
    Task(Task * task = _current, int (* entry)(Tn ...) = 0, Tn ... an) { // fork-like constructor
        // Aloca espaços de memoria para a nova task
        this->_as = new (SYSTEM) Address_Space;
        this->_cs = new (SYSTEM) Segment(current()->code_segment()->size(), Segment::Flags::APP);
        this->_ds = new (SYSTEM) Segment(current()->data_segment()->size(), Segment::Flags::APP);

        // Caso não passe uma função ele executa o PAI novamente
        this->_entry = entry ? entry : static_cast<int (*)(Tn...)>(current()->entry());

        // Salva o enderaçamento da área de codigo e dados da task atual
        Log_Addr src_code, src_data;
        src_code = current()->code();
        src_data = current()->data();

        // Task atual cria o endereçamento para a área de codigos e dados que foi criada por ela
        Log_Addr dst_code = current()->address_space()->attach(_cs);
        Log_Addr dst_data = current()->address_space()->attach(_ds);

        // Copia o segmento de codigo e dados da task atual para a nova task
        memcpy(dst_code, src_code, this->code_segment()->size());
        memcpy(dst_data, src_data, this->data_segment()->size());

        // Desaloca do processo atual os segmentos da task que vai ser criada
        current()->address_space()->detach(this->code_segment());
        current()->address_space()->detach(this->data_segment());

        // Realiza um attach dos segmentos de codigo e dados da nova task em seu address space
        this->_code = this->_as->attach(this->_cs);
        this->_data = this->_as->attach(this->_ds);

        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        // Create the task's main thread
        this->_main = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::MAIN, this), static_cast<int (*)(Tn...)>(_entry), an...);
    }

    ~Task();

    Address_Space * address_space() const { return _as; }

    Segment * code_segment() const { return _cs; }
    Segment * data_segment() const { return _ds; }

    Log_Addr code() const { return _code; }
    Log_Addr data() const { return _data; }
    Log_Addr entry() const { return _entry; }

    Thread * main() const { return _main; }

    int join() { return _main->join(); }

    static Task * volatile self() { return current(); }

private:
    void activate() const { _current = const_cast<Task *>(this); _as->activate(); }

    void insert(Thread * t) { _threads.insert(new (SYSTEM) Queue::Element(t)); }
    void remove(Thread * t) { Queue::Element * el = _threads.remove(t); if(el) delete el; }

    static Task * volatile current() { return _current; }
    static void current(Task * t) { _current = t; }

    static void init();

private:
    Address_Space * _as;
    Segment * _cs;
    Segment * _ds;
    Log_Addr _code;
    Log_Addr _data;
    Log_Addr _entry;
    Thread * _main;
    Queue _threads;

    static Task * volatile _current;
};


// Thread inline methods that depend on Task
// Threads with the default configuration are only used in single-task scenarios, since the framework's agent always creates a configuration
template <typename... Tn>
inline Thread::Thread(int (*entry)(Tn...), Tn... an)
    : _task(Task::self()), _user_stack(0), _state(READY), _waiting(0), _joining(0), _link(this, NORMAL)
{
    constructor_prologue(STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an...);
    constructor_epilogue(entry, STACK_SIZE);
}

template<typename ... Tn>
inline Thread::Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an)
: _task(conf.task ? conf.task : Task::self()), _state(conf.state), _waiting(0), _joining(0), _link(this, conf.criterion)
{
    if (multitask && !conf.stack_size)
    { 
        constructor_prologue(STACK_SIZE);
        _user_stack = new (SYSTEM) Segment(USER_STACK_SIZE, Segment::Flags::APP);   

        // Processo pai realiza um attach para user stack do filho ter um endereçamento
        Log_Addr ustack = Task::self()->address_space()->attach(_user_stack);

        // TODO: ENTENDER melhor isso;
        // Initialize the thread's user-level stack and determine a relative stack pointer (usp) from the top of the stack
        Log_Addr usp = ustack + USER_STACK_SIZE;
        if (conf.criterion == MAIN)
            usp -= CPU::init_user_stack(usp, 0, an...); // the main thread of each task must return to crt0 to call _fini (global destructors) before calling __exit
        else
            usp -= CPU::init_user_stack(usp, &__exit, an...); // __exit will cause a Page Fault that must be properly handled

        // Removendo o o endereçamento que foi criado no processo pai para criar a stack do filho
        Task::self()->address_space()->detach(_user_stack, ustack);

        // Criando no espaço de enderaçmento do processo filho o attach para a usr_stack
        ustack = _task->address_space()->attach(_user_stack);

        // TODO: Entneder init_user_stack e posicionamento do ponteiro
        usp = ustack + USER_STACK_SIZE - usp;

        // Initialize the thread's system-level stack
        constructor_epilogue(entry, STACK_SIZE);
        _context = CPU::init_stack(usp, _stack + STACK_SIZE, &__exit, entry, an...);
    }
    else
    { // single-task scenarios and idle thread, which is a kernel thread, don't have a user-level stack
        constructor_prologue(conf.stack_size);
        _user_stack = 0;
        _context = CPU::init_stack(0, _stack + conf.stack_size, &__exit, entry, an...);
    }

    constructor_epilogue(entry, STACK_SIZE);
}

__END_SYS

#endif
