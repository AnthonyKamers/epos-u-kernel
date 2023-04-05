#include <utility/ostream.h>
#include <time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

OStream cout;

int teste(int number) {

    cout << "teste: " << number << endl;
    return 0;
}

int main()
{
    int number = 1;
    new Thread(&teste, number);

    return 0;
}
