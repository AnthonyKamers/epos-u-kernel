// EPOS-- RISC-V 64 MMU Mediator Implementation

#include <architecture/rv64/rv64_mmu.h>

__BEGIN_SYS

// Class attributes
MMU::List MMU::_free;
MMU::Page_Directory * MMU::L2;
MMU::Page_Table * MMU::L1;
MMU::Page_Table * MMU::L0;

__END_SYS