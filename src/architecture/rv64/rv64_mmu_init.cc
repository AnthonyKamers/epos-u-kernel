#include <architecture/mmu.h>
#include <system.h>

__BEGIN_SYS

void MMU::init()
{
    db<Init, MMU>(INF) << "MMU::init() => Sv39 RV64 MMU" << endl;

    // The System info must contains the updated information (set in setup_sifive_u.cc)
    System_Info * si = System::info();
    free(si->pmm.free1_base, pages(si->pmm.free1_top - si->pmm.free1_base));
}

__END_SYS
