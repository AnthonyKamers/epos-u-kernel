#include <utility/ostream.h>
#include <process.h>
#include <time.h>

using namespace EPOS;

OStream cout;
#define DELAY_TIME 20000

const int qtd_threads = 3;
Thread * threads[qtd_threads];

int work_thread(int number) {
    cout << "Thread " << number << " working" << endl;
    Delay wait_micro_sec(DELAY_TIME);
    return 0;
}

int main()
{
    cout << "Test using FCFS scheduler (non preemptive)" << endl;

    for (int i = 0; i < qtd_threads; i++)
        threads[i] = new (SYSTEM) Thread(&work_thread, i);

    for (int i = 0; i < qtd_threads; i++) {
        threads[i]->join();
        delete threads[i];
    }

    return 0;
}
