#include <utility/ostream.h>
#include <process.h>
#include <system.h>

using namespace EPOS;

OStream cout;

int teste() {
    cout << "teste" << endl;
    return 0;
}

int main()
{
    cout << "Hello world!" << endl;

    Thread * thread = new Thread(&teste);
    thread->join();

    cout << "finished thread; bye!" << endl;
    return 0;
}
