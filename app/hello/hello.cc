#include <utility/ostream.h>
#include <time.h>
#include <real-time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

const unsigned int iterations = 5;

const unsigned int period_a = 20; // ms
const unsigned int period_b = 40; // ms
const unsigned int period_c = 50; // ms
const unsigned int period_d = 120; // ms
const unsigned int period_e = 800; // ms
const unsigned int period_f = 1200; // ms

const unsigned int deadline_a = 15; // ms
const unsigned int deadline_b = 40; // ms
const unsigned int deadline_c = 50; // ms
const unsigned int deadline_d = 80; // ms
const unsigned int deadline_e = 400; // ms
const unsigned int deadline_f = 1000; // ms

const unsigned int capacity_a = 15; // ms
const unsigned int capacity_b = 5; // ms
const unsigned int capacity_c = 5; // ms
const unsigned int capacity_d = 10; // ms
const unsigned int capacity_e = 5; // ms
const unsigned int capacity_f = 10; // ms

OStream cout;
Chronometer chrono;
Periodic_Thread * thread_a;
Periodic_Thread * thread_b;
Periodic_Thread * thread_c;
Periodic_Thread * thread_d;
Periodic_Thread * thread_e;
Periodic_Thread * thread_f;

int work_thread(int number) {
    do {
        Microsecond elapsed = chrono.read() / 1000;
        cout << "T = " << elapsed << ". Thread " << number << " working" << endl;
    } while (Periodic_Thread::wait_next());
    cout << "Thread " << number << " ended." << endl;
    return 0;
}

int main()
{

    cout << "DM Scheduler test." << endl;

    // p,d,c,act,t
    thread_a = new Periodic_Thread(RTConf(period_a * 1000, deadline_a * 1000, capacity_a * 1000, 0, iterations), &work_thread, 1);
    thread_b = new Periodic_Thread(RTConf(period_b * 1000, deadline_b * 1000, capacity_b * 1000, 0, iterations), &work_thread, 2);
    thread_c = new Periodic_Thread(RTConf(period_c * 1000, deadline_c * 1000, capacity_c * 1000, 0, iterations), &work_thread, 3);
    thread_d = new Periodic_Thread(RTConf(period_d * 1000, deadline_d * 1000, capacity_d * 1000, 0, iterations), &work_thread, 4);
    thread_e = new Periodic_Thread(RTConf(period_e * 1000, deadline_e * 1000, capacity_e * 1000, 0, iterations), &work_thread, 5);
    thread_f = new Periodic_Thread(RTConf(period_f * 1000, deadline_f * 1000, capacity_f * 1000, 0, iterations), &work_thread, 6);

    chrono.start();

    thread_a->join();
    thread_b->join();
    thread_c->join();
    thread_d->join();
    thread_e->join();
    thread_f->join();

    chrono.stop();

    cout << "I'm also done, bye!" << endl;

    return 0;
}