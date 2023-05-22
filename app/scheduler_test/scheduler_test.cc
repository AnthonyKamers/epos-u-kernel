#include <utility/ostream.h>
#include <process.h>

using namespace EPOS;

OStream cout;

const int qtd_threads = 3;
Thread * threads[qtd_threads];

int work_thread(int number) {
    cout << "Thread " << number << " working" << endl;
    return 0;
}

int main()
{
    cout << "Test using RR scheduler (preemptive)" << endl;

    for (int i = 0; i < qtd_threads; i++)
        threads[i] = new Thread(&work_thread, i);

    for (int i = 0; i < qtd_threads; i++)
        threads[i]->join();

    return 0;
}