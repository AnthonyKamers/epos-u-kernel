#include <architecture/mmu.h>
#include <system.h>

__BEGIN_SYS

void MMU::init()
{
    // free page tables (consider pages of 4Kb)
    free(RAM_BASE, pages(RAM_BASE + (PG_SIZE) - RAM_BASE));

    // free page tables segments (consider pages of 2Mb)
    free_segment(RAM_TOP + 1, pages(RAM_TOP + (PG_CHUNK_SIZE) - RAM_TOP));
}

__END_SYS
