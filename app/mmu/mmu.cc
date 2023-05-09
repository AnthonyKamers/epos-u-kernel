#include <utility/ostream.h>
#include <architecture/rv64/rv64_mmu.h>
#include <utility/string.h>
#include <memory.h>

using namespace EPOS;

OStream cout;
MMU mmu;

typedef struct {
    char * name;
    int age;
} test_t;

int main()
{
    cout << "Testing MMU" << endl;

    // alloc 1 frame and get its logical address
    CPU::Phy_Addr physical_address_frame = mmu.calloc(1);
    CPU::Log_Addr logical_address_frame = mmu.phy2log(physical_address_frame);
    cout << "Allocated physical address: " << physical_address_frame;
    cout << " with logical address: " << logical_address_frame << endl;

    // make example structural data
    char name[5] = "Epos";
    test_t test;
    test.name = name;
    test.age = 24;

    // write data to logical address
    memcpy(logical_address_frame, &test, sizeof(test_t));

    // verify written data
    test_t test_copy;
    memcpy(&test_copy, logical_address_frame, sizeof(test_t));

    if (test.name == test_copy.name && test.age == test_copy.age)
        cout << "Data written and read correctly!" << endl;
    else
        cout << "Test failed: data written and read incorrectly!" << endl;

    return 0;
}