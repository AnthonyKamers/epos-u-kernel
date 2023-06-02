#include <utility/ostream.h>
#include <time.h>
#include <real-time.h>
#include <synchronizer.h>
#include <process.h>
#include <utility/math.h>

using namespace EPOS;

const unsigned int iterations = 5;

const unsigned int period_a = 4; // ms
const unsigned int period_b = 5; // ms
const unsigned int period_c = 6; // ms
const unsigned int period_d = 11; // ms

const unsigned int deadline_a = 3; // ms
const unsigned int deadline_b = 4; // ms
const unsigned int deadline_c = 5; // ms
const unsigned int deadline_d = 10; // ms

const unsigned int capacity_a = 1; // ms
const unsigned int capacity_b = 1; // ms
const unsigned int capacity_c = 2; // ms
const unsigned int capacity_d = 1; // ms

const unsigned int wcet_a = 1; // ms
const unsigned int wcet_b = 1; // ms
const unsigned int wcet_c = 1; // ms
const unsigned int wcet_d = 1; // ms

OStream cout;
Chronometer chrono;
Periodic_Thread * thread_a;
Periodic_Thread * thread_b;
Periodic_Thread * thread_c;
Periodic_Thread * thread_d;

int work_thread(int number) {
    cout << "Thread " << number << " started." << endl;
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

    chrono.start();

    thread_a->join();
    thread_b->join();
    thread_c->join();
    thread_d->join();

    chrono.stop();

    cout << "I'm also done, bye!" << endl;

    return 0;
}