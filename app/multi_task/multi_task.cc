#include <utility/ostream.h>
#include <process.h>

using namespace EPOS;

OStream cout;

int working_task(int number_arguments, char * arguments[]) {
    cout << "Task " << number_arguments << " running!" << endl;
    return 0;
}


int main()
{
    cout << "Multi Task Test - Main task running!" << endl;
    Task *self = Task::self();

    // by the limitation of the current implementation of handle_task in agent.h,
    // we decided to make a simple argc, argv test using the argc as the number of
    // the task
    Task * task1 = new Task(self, &working_task, 1, nullptr);
    working_task(0, nullptr);

    task1->join();
    delete task1;

    cout << "Main Task leaving!" << endl;

    return 0;
}
