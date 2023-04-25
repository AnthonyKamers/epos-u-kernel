// EPOS SiFive-U (RISC-V) SETUP

#include <architecture.h>
#include <machine.h>
#include <utility/elf.h>
#include <utility/string.h>

extern "C" {
    void _start();

    void _int_entry();

    // SETUP entry point is in .init (and not in .text), so it will be linked first and will be the first function after the ELF header in the image
    void _entry() __attribute__ ((used, naked, section(".init")));
    void _setup();

    void _panic();

    // LD eliminates this variable while performing garbage collection, that's why the used attribute.
    char __boot_time_system_info[sizeof(EPOS::S::System_Info)] __attribute__ ((used)) = "<System_Info placeholder>"; // actual System_Info will be added by mkbi!
}

__BEGIN_SYS

extern OStream kout, kerr;

class Setup
{
private:
    // Physical memory map
    static const unsigned long RAM_BASE         = Memory_Map::RAM_BASE;
    static const unsigned long RAM_TOP          = Memory_Map::RAM_TOP;
    static const unsigned long MIO_BASE         = Memory_Map::MIO_BASE;
    static const unsigned long MIO_TOP          = Memory_Map::MIO_TOP;
    static const unsigned long FREE_BASE        = Memory_Map::FREE_BASE;
    static const unsigned long FREE_TOP         = Memory_Map::FREE_TOP;
    static const unsigned long SETUP            = Memory_Map::SETUP;
    static const unsigned long BOOT_STACK       = Memory_Map::BOOT_STACK;
    static const unsigned long SYS_INFO         = Memory_Map::SYS_INFO;
    static const unsigned long SYS_PT           = Memory_Map::SYS_PT;
    static const unsigned long SYS_PD           = Memory_Map::SYS_PD;
    static const unsigned long SYS_CODE           = Memory_Map::SYS_CODE;
    static const unsigned long SYS_DATA           = Memory_Map::SYS_DATA;
    static const unsigned long SYS_STACK           = Memory_Map::SYS_STACK;
    static const unsigned long SYS_HEAP           = Memory_Map::SYS_HEAP;
    static const unsigned long INIT           = Memory_Map::INIT;
    static const unsigned long PHY_MEM           = Memory_Map::PHY_MEM;
    static const unsigned long IMAGE           = Memory_Map::IMAGE;
    static const unsigned long PAGE_TABLES           = Memory_Map::PAGE_TABLES;

    static const unsigned int PT_ENTRIES    = MMU::PT_ENTRIES;
    static const unsigned int AT_ENTRIES    = MMU::AT_ENTRIES;
    static const unsigned int PD_ENTRIES    = MMU::PD_ENTRIES;
    static const unsigned long PG_SIZE      = MMU::PG_SIZE;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::Page Page;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;
    typedef MMU::PT_Entry PT_Entry;
    typedef MMU::Flags Flags;
    typedef MMU::RV64_Flags RV64_Flags;

public:
    Setup();

private:
    void say_hi();
    void call_next();

    // mmu
    void mmu_init();

private:
    System_Info * si;
};


Setup::Setup()
{
//    CPU::int_disable();
    Display::init();
    kout << endl;
    kerr << endl;

    si = reinterpret_cast<System_Info *>(&__boot_time_system_info);
    if(si->bm.n_cpus > Traits<Machine>::CPUS)
        si->bm.n_cpus = Traits<Machine>::CPUS;

    db<Setup>(TRC) << "Setup(si=" << reinterpret_cast<void *>(si) << ",sp=" << CPU::sp() << ")" << endl;
    db<Setup>(INF) << "Setup:si=" << *si << endl;

    // Print basic facts about this EPOS instance
     say_hi();

    // Make MMU page tables
    mmu_init();

    // SETUP ends here, so let's transfer control to the next stage (INIT or APP)
    call_next();
}


void Setup::say_hi()
{
    db<Setup>(TRC) << "Setup::say_hi()" << endl;
    db<Setup>(INF) << "System_Info=" << *si << endl;

    if(si->bm.application_offset == -1U)
        db<Setup>(ERR) << "No APPLICATION in boot image, you don't need EPOS!" << endl;

    kout << "This is EPOS!\n" << endl;
    kout << "Setting up this machine as follows: " << endl;
    kout << "  Mode:         " << ((Traits<Build>::MODE == Traits<Build>::LIBRARY) ? "library" : (Traits<Build>::MODE == Traits<Build>::BUILTIN) ? "built-in" : "kernel") << endl;
    kout << "  Processor:    " << Traits<Machine>::CPUS << " x RV" << Traits<CPU>::WORD_SIZE << " at " << Traits<CPU>::CLOCK / 1000000 << " MHz (BUS clock = " << Traits<Machine>::HFCLK / 1000000 << " MHz)" << endl;
    kout << "  Machine:      SiFive-U" << endl;
    kout << "  Memory:       " << (RAM_TOP + 1 - RAM_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(RAM_BASE) << ":" << reinterpret_cast<void *>(RAM_TOP) << "]" << endl;
    kout << "  User memory:  " << (FREE_TOP - FREE_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(FREE_BASE) << ":" << reinterpret_cast<void *>(FREE_TOP) << "]" << endl;
    kout << "  I/O space:    " << (MIO_TOP + 1 - MIO_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(MIO_BASE) << ":" << reinterpret_cast<void *>(MIO_TOP) << "]" << endl;
    kout << "  Node Id:      ";
    if(si->bm.node_id != -1)
        kout << si->bm.node_id << " (" << Traits<Build>::NODES << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    kout << "  Position:     ";
    if(si->bm.space_x != -1)
        kout << "(" << si->bm.space_x << "," << si->bm.space_y << "," << si->bm.space_z << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    if(si->bm.extras_offset != -1UL)
        kout << "  Extras:       " << si->lm.app_extra_size << " bytes" << endl;

    kout << endl;
}


void Setup::call_next()
{
    db<Setup>(INF) << "SETUP ends here!" << endl;

    // Call the next stage
    static_cast<void (*)()>(_start)();

    // SETUP is now part of the free memory and this point should never be reached, but, just in case ... :-)
    db<Setup>(ERR) << "OS failed to init!" << endl;
}

void Setup::mmu_init() {
    unsigned int pt_entries = PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP + 1);
    unsigned int page_tables = MMU::pts(pages);
    unsigned int attachers = MMU::ats(page_tables);
    unsigned int page_directories = MMU::pds(attachers);

    kout << "################# MMU INIT #################" << endl;
    kout << "Page table entries: " << pt_entries << endl;
    kout << "pages: " << pages << endl;
    kout << "page tables: " << page_tables << endl;
    kout << "attachers: " << attachers << endl;
    kout << "page directories: " << page_directories << endl;
    kout << "PD_ENTRIES: " << PD_ENTRIES << endl;
    kout << "AT_ENTRIES: " << AT_ENTRIES << endl;
    kout << "PT_ENTRIES: " << PT_ENTRIES << endl;

    // Map L2 Page Directory
    Phy_Addr L2_Addr = RAM_BASE;
    auto * L2 = new ((void *) (L2_Addr)) Page_Directory();
    L2_Addr += PG_SIZE;
    L2->remap(L2_Addr, MMU::RV64_Flags::VALID, 0, PD_ENTRIES);

    // Map L1 Page Table (Attacher -> Chunk)
    Phy_Addr L1_Addr = L2_Addr + AT_ENTRIES * PG_SIZE;
    for (unsigned long i = 0; i < PD_ENTRIES; i++) {
        Page_Table * L1 = new ((void *)L2_Addr) Page_Table();
        L2_Addr += PG_SIZE;

        L1->remap(L1_Addr, MMU::RV64_Flags::VALID, 0, AT_ENTRIES);
        L1_Addr += AT_ENTRIES * PG_SIZE;
    }

    // Map L0 Page Table (Final Page Table -> Chunk)
    Phy_Addr L0_Addr = L1_Addr;
    L1_Addr = 0;
    for (unsigned long i = 0; i < PD_ENTRIES; i++)
        for (unsigned long j = 0; j < AT_ENTRIES; j++) {
            Page_Table * L0 = new ((void *)L0_Addr) Page_Table();
            L0_Addr += PG_SIZE;

            L0->remap(L1_Addr, MMU::RV64_Flags::VALID, 0, PT_ENTRIES);
            L1_Addr += PT_ENTRIES * PG_SIZE;
        }

    kout << "Last Directory: " << L2_Addr << endl;
    kout << "Last Page: " << L1_Addr << endl << endl;

    kout << "MMU -> System Mapping (System + IO)" << endl;

    // System Page Directory (System + IO Address Space)
    unsigned sys_pages = MMU::pages(SYS_HEAP - INIT);
    unsigned sys_pts = MMU::pts(sys_pages);
    unsigned sys_attachers = MMU::ats(sys_pts);
    unsigned sys_pds = MMU::pds(sys_attachers);
    Phy_Addr addr = PAGE_TABLES + (1 + sys_attachers) * PG_SIZE;

    kout << "sys_attachers = " << sys_attachers << endl;
    kout << "sys_pds = " << sys_pds << endl;

    L2->remap(addr, MMU::RV64_Flags::VALID, sys_attachers, sys_attachers + 1);
    Page_Directory * sys_pd = new ((void *)addr) Page_Directory();
    addr = PAGE_TABLES + (1 + PD_ENTRIES + AT_ENTRIES * PD_ENTRIES) * PG_SIZE;
    sys_pd->remap(addr, MMU::RV64_Flags::VALID, 0, sys_pts);

    unsigned long sys_addr = PAGE_TABLES + (1 + PD_ENTRIES + PD_ENTRIES * PD_ENTRIES - sys_pages) * PG_SIZE;
    for (unsigned long i = 0; i < sys_pts; i++) {
        Page_Table * sys_pt = new ((void *)addr) Page_Table();
        addr += PG_SIZE;
        sys_pt->remap(sys_addr, Flags::SYS);
        sys_addr += PD_ENTRIES * PG_SIZE;
    }

    db<Setup>(INF) << "addr = " << hex << addr << endl;
    db<Setup>(INF) << "sys_addr = " << sys_addr << endl;

    // update free memory (to initialize the free space list in rv64_mmu_init.cc
    si->pmm.free1_base = RAM_BASE;
    si->pmm.free1_top = addr;

    // Set SATP (to change page allocation) + Flush old TLB
    CPU::pdp(reinterpret_cast<unsigned long>(L2));
    CPU::flush_tlb();
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    // SiFive-U core 0 doesn't have MMU
    if (CPU::mhartid() == 0)
        CPU::halt();

    CPU::mint_disable();
    CPU::mstatusc(CPU::MIE);
    CPU::satp(0);
    Machine::clear_bss();

    // set the stack pointer, thus creating a stack for SETUP
    CPU::sp(Memory_Map::BOOT_STACK - Traits<Machine>::STACK_SIZE - sizeof(long));

    // Set up the Physical Memory Protection registers correctly A = NAPOT, X, R, W, all memory
    CPU::pmpcfg0(0x1f);
    CPU::pmpaddr0((1UL << 55) - 1);

    // Delegate all traps to supervisor (Timer will not be delegated due to architecture reasons).
    CPU::mideleg(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::medeleg(0xffff);

    CPU::mies(CPU::MSI | CPU::MTI | CPU::MEI); // enable interrupts generation by CLINT // (mstatus) disable interrupts (they will be reenabled at Init_End)
    CLINT::mtvec(CLINT::DIRECT, _int_entry); // setup a preliminary machine mode interrupt handler pointing it to _mmode_forward

    // MPP_S = change to supervirsor
    // MPIE = otherwise we won't ever receive interrupts
    CPU::mstatus(CPU::MPP_S | CPU::MPIE);
    CPU::mepc(CPU::Reg(&_setup)); // entry = _setup
    CPU::mret(); // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)
}

void _setup() // supervisor mode
{
    kerr  << endl;
    kout  << endl;

    Setup setup;
}
