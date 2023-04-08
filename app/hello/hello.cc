#include <utility/ostream.h>
#include <process.h>
using namespace EPOS;

void * p1 = new (SHARED) void *;
void * p2 = new (SHARED) void *;
void * p3 = new void *;

const char ** _var1 = new (SHARED) const char *;
const char ** _var2 = new (SHARED) const char *;

int touch1()
{
  * _var1 = "Thread 1 was here";
  return 0;
}

int touch2()
{
  * _var2 = "Thread 2 was here";
  return 0;
}

OStream cout;
int main()
{
    assert(p1 == p2);
    assert(p1 != p3);

    char ** c1 = reinterpret_cast<char **>(p1);
    char ** c2 = reinterpret_cast<char **>(p2);
    char ** c3 = reinterpret_cast<char **>(p3);

    char buff[48] = "Memory shared\0";
    *c1 = buff;

    char buff2[20] = "Memory not shared\0";
    *c3 = buff2;

    cout << "p1: " << *c1 << endl;
    cout << "p2: " << *c2 << endl;
    cout << "p3: " << *c3 << endl;
   
    Thread * t1 = new Thread(&touch1);
    t1->join();
    cout << *_var2 << endl; 
    Thread * t2 = new Thread(&touch2);
    t2->join();
    cout << *_var1 << endl; 
    return 0;
}
