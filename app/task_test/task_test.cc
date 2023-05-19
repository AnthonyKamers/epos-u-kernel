#include <utility/ostream.h>
#include <architecture/rv64/rv64_cpu.h>
#include <memory.h>
#include <process.h>

#define BYTES_SEGMENT TASK::BYTES_SEGMENT;

using namespace EPOS;

OStream cout;
typedef CPU::Reg64 Reg64;

int test_task()
{
    Reg64 satp = CPU::satp();
    cout << "I am a completely new task - satp: " << satp << endl;

    cout << "Leaving test task" << endl;
    return 0;
}


int main()
{
    Reg64 satp = CPU::satp();
    cout << "First task - satp: " << satp << endl;

    // make new task and wait for it to finish
    Task * new_task = new Task(&test_task);
    new_task->join();

    cout << "Leaving main Task" << endl;
    return 0;
}
