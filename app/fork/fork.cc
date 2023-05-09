#include <utility/ostream.h>
#include <architecture/rv64/rv64_mmu.h>
#include <memory.h>

#define BYTES_SEGMENT 10000

using namespace EPOS;

OStream cout;
MMU mmu;

int main()
{
    cout << "MMU: Fork address space test!" << endl;

    // get information from current address space
    Address_Space current_as(MMU::current());

    // allocate enough for a new address space
    CPU::Phy_Addr new_as_physical = mmu.alloc(sizeof(current_as));
    unsigned long * new_as_physical_ptr = new_as_physical;
    CPU::Log_Addr log_new_as = MMU::phy2log(new_as_physical);

    cout << "Fork physical = " << new_as_physical << endl;
    cout << "Fork logical = " << log_new_as << endl;

    // make a copy of the current address space
    memcpy(log_new_as, reinterpret_cast<void *>(MMU::current()), sizeof(current_as));
    MMU::Page_Directory * new_as = reinterpret_cast<MMU::Page_Directory *>(new_as_physical_ptr);
    Address_Space forked(new_as);

    // create segment for forked address space
    Segment * es1 = new (SYSTEM) Segment(BYTES_SEGMENT, Segment::Flags::SYS);
    cout << "new Segment created with " << BYTES_SEGMENT << "bytes" << endl;

    // attach segment for forked address space
    CPU::Log_Addr * extra1 = forked.attach(es1);
    cout << "extra segment in forked address space attached: " << extra1 << endl;

    cout << "Forked correctly!" << endl;

    return 0;
}
