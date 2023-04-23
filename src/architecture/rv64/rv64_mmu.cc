// EPOS RV64 MMU Mediator Implementation

#include <architecture/rv64/rv64_mmu.h>

__BEGIN_SYS

MMU::ListPage MMU::_free;
MMU::Page_Directory *MMU::_master;
MMU::ListMegaPage MMU::_free_mb;

__END_SYS
