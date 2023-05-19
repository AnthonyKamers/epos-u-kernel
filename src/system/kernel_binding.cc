// EPOS Kernel Binding

#include <framework/main.h>
#include <framework/agent.h>

__BEGIN_SYS
__END_SYS

__USING_SYS;
#ifdef __ia32__
extern "C" { void _exec(void *) __attribute__ ((thiscall)); }
#endif

#ifdef __sifive_u__
// The order of components in Agent::_handlers must match the respective Type<Component>::ID
Agent::Member Agent::_handlers[] = {&Agent::handle_thread,
                                    &Agent::handle_task,
                                    &Agent::handle_active,
                                    &Agent::handle_address_space,
                                    &Agent::handle_segment,
                                    &Agent::handle_mutex,
                                    &Agent::handle_semaphore,
                                    &Agent::handle_condition,
                                    &Agent::handle_clock,
                                    &Agent::handle_alarm,
                                    &Agent::handle_chronometer,
                                    &Agent::handle_utility};

extern "C"
{
    void _exec(void *m) { reinterpret_cast<Agent *>(m)->exec(); }
}
#endif
