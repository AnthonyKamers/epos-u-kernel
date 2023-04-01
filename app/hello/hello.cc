#include <utility/ostream.h>
#include <process.h>
using namespace EPOS;

void * p1 = new (SHARED) void *;
void * p2 = new (SHARED) void *;
void * p3 = new void *;

OStream cout;
int main()
{
	assert(p1 == p2);
        assert(p1 != p3);

        char ** c1 = reinterpret_cast<char **>(p1);
	char ** c2 = reinterpret_cast<char **>(p2);
        char ** c3 = reinterpret_cast<char **>(p3);
	char buff[32] = "We are being shared!\0";
  	*c1 = buff;
	char buff2[32] = "I am not being shared :(\0";
        *c3 = buff2; 
        cout << "p1: " << *c1 << endl; 
        cout << "p2: " << *c2 << endl;
        cout << "p3: " << *c3 << endl;
	return 0;
}
