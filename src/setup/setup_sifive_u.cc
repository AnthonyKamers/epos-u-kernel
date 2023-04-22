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

    static const unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    static const unsigned int PD_ENTRIES = PT_ENTRIES;
    static const unsigned int PAGE_SIZE = 4096;
    static const unsigned long PAGE_TABLES = Memory_Map::PAGE_TABLES;
    static const unsigned long INIT = Memory_Map::INIT;
    static const unsigned long SYS_HEAP = Memory_Map::SYS_HEAP;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::RV64_Flags RV64_Flags;
    typedef MMU::Page Page;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;
public:
    Setup();

private:
    void say_hi();
    void call_next();
    void mmu_init();

private:
    System_Info * si;
};


Setup::Setup()
{
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
//    unsigned int pt_entries = MMU::PT_ENTRIES;
//    unsigned long pages = MMU::pages(RAM_TOP + 1);
//    unsigned int page_tables = MMU::pts(pages);
//    unsigned int attachers = MMU::ats(page_tables);
//    unsigned int page_directories = MMU::pds(attachers);

//    kout << "Page table entries: " << pt_entries << endl;
//    kout << "pages: " << pages << endl;
//    kout << "page tables: " << page_tables << endl;
//    kout << "attachers: " << attachers << endl;
//    kout << "page directories: " << page_directories << endl;
  //
     unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP + 1);


    kout << "pages = " << pages << endl;

    unsigned total_pts = MMU::page_tables(pages);
    kout << "pts = " << total_pts << endl;

    unsigned int PD_ENTRIES_LVL_2 = total_pts / PT_ENTRIES;
    unsigned int PD_ENTRIES_LVL_1 = PT_ENTRIES;
    unsigned int PT_ENTRIES_LVL_0 = PT_ENTRIES;

    // kout << "LVL 2: " << PD_ENTRIES_LVL_2 << endl;

    Phy_Addr PD2_ADDR = PAGE_TABLES;
    Phy_Addr pts = PAGE_TABLES;
    kout << "Page Tables: " << pts << endl;
    Page_Directory *master = new ((void *)PD2_ADDR) Page_Directory();
    kout << "SATP: " << PD2_ADDR << endl;
    PD2_ADDR += PAGE_SIZE;

    master->remap(PD2_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_2);

    Phy_Addr PD1_ADDR = PD2_ADDR + PT_ENTRIES * PAGE_SIZE;
    Phy_Addr PT0_ADDR = PD1_ADDR;

    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        Page_Directory *pd_lv1 = new ((void *)PD2_ADDR) Page_Directory();
        PD2_ADDR += PAGE_SIZE;

        pd_lv1->remap(PD1_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_1);
        PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
    }

    PD1_ADDR = 0;
    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        for (unsigned long j = 0; j < PD_ENTRIES_LVL_1; j++)
        {
            Page_Table *pt_lv0 = new ((void *)PT0_ADDR) Page_Table();
            PT0_ADDR += PAGE_SIZE;
            pt_lv0->remap(PD1_ADDR, RV64_Flags::SYS, 0, PT_ENTRIES_LVL_0);
            PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
        }
    }
    kout << "Last Directory" << PD2_ADDR << endl;
    kout << "Last Page" << PD1_ADDR - 1 << endl;

    // SYSTEM MAPPING
    kout << "System Mapping" << endl;

    kout << "SIZE: " << (Phy_Addr) (SYS_HEAP - INIT) << endl;
    kout << "INDEX: " << MMU::directory_lvl_2(INIT) << endl;

    kout << "PAGE_TABLES: "  << (Phy_Addr) PAGE_TABLES << endl;


    unsigned sys_pages = MMU::pages(SYS_HEAP - INIT);
    unsigned sys_pts = MMU::page_tables(sys_pages);
    int page = MMU::directory_lvl_2(INIT);
    Phy_Addr addr = PAGE_TABLES + (1 + page) * PAGE_SIZE;

    master->remap(addr, RV64_Flags::V, page, page + 1);
    Page_Directory * sys_pd = new ((void *)addr) Page_Directory();
    addr = PAGE_TABLES + (1 + PD_ENTRIES + PD_ENTRIES_LVL_2 * PD_ENTRIES) * PAGE_SIZE;
    sys_pd->remap(addr, RV64_Flags::V, 0, sys_pts);

    unsigned long sys_addr = PAGE_TABLES + (1 + PD_ENTRIES + PD_ENTRIES * PD_ENTRIES - sys_pages) * PAGE_SIZE;
    for (unsigned long i = 0; i < sys_pts; i++) {
      Page_Table * sys_pt = new ((void *)addr) Page_Table();
      addr += PAGE_SIZE;
      sys_pt->remap(sys_addr, RV64_Flags::SYS);
      sys_addr += PD_ENTRIES * PAGE_SIZE;
    }

    db<Setup>(WRN) << "addr = " << hex << addr << endl;
    db<Setup>(WRN) << "sys_addr = " << sys_addr << endl;

    si->pmm.free1_base = RAM_BASE;
    si->pmm.free1_top = addr;

    // Set SATP and enable paging
    // db<Setup>(WRN) << "Set SATP" << endl;
    CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(master) >> 12));
    // db<Setup>(WRN) << "Flush TLB" << endl;
    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();

    asm("setup:");

}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    if(CPU::mhartid() != 0)                             // SiFive-U always has 2 cores, so we disable CU1 here
        CPU::halt();

    CPU::mstatusc(CPU::MIE);                            // disable interrupts (they will be reenabled at Init_End)
    CPU::mies(CPU::MSI);                                // enable interrupts generation by CLINT
    CLINT::mtvec(CLINT::DIRECT, _int_entry);            // setup a preliminary machine mode interrupt handler pointing it to _int_entry

    CPU::sp(Memory_Map::BOOT_STACK + Traits<Machine>::STACK_SIZE - sizeof(long)); // set the stack pointer, thus creating a stack for SETUP

    Machine::clear_bss();

    CPU::mstatus(CPU::MPP_M);                           // stay in machine mode at mret

    CPU::mepc(CPU::Reg(&_setup));                       // entry = _setup
    CPU::mret();                                        // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)
}

void _setup() // supervisor mode
{
    kerr  << endl;
    kout  << endl;

    Setup setup;
}
