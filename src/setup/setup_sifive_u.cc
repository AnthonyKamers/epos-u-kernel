// EPOS SiFive-U (RISC-V) SETUP

#include <architecture.h>
#include <machine.h>
#include <utility/elf.h>
#include <utility/string.h>

extern "C" {
    void _start();

    void _int_entry();
    void _panic();

    // SETUP entry point is in .init (and not in .text), so it will be linked first and will be the first function after the ELF header in the image
    void _entry() __attribute__ ((used, naked, section(".init")));
    void _setup();
    void mmu_init();

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
    static const unsigned long PAGE_TABLES      = Memory_Map::PAGE_TABLES;
    static const unsigned long PHY_MEM          = Memory_Map::PHY_MEM;
    static const unsigned long MEM_BASE         = RAM_BASE;
    static const unsigned long IO               = MIO_BASE;
    static const unsigned long SYS               = Memory_Map::SYS;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::RV64_Flags RV64_Flags;
    typedef MMU::Page Page;
    typedef MMU::MegaPage MegaPage;
    typedef MMU::PT_Entry PT_Entry;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;

public:
    Setup();

private:
    void say_hi();
    void call_next();
    void build_pmm();
    void setup_system_page_directory();
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

    // configure physical memory map
    build_pmm();

    // configure MMU
    setup_system_page_directory();
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

void Setup::build_pmm()
{
    db<Setup>(INF) << "Setup::build_pmm()" << endl;

    // Allocate (reserve) memory for all entities we have to setup.
    // We'll start at the highest address to make possible a memory model
    // on which the application's logical and physical address spaces match.
    Phy_Addr top_page = MMU::pages(si->bm.mem_top);

    // Machine to Supervisor code (1 x sizeof(Page), not listed in the PMM)
    top_page -= 1;

    // System Info (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_info = top_page * sizeof(Page);

    // System Page Table (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pt = top_page * sizeof(MegaPage);

    // System Page Directory (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pd = top_page * sizeof(Page);

    // Page tables to map the whole physical memory
    // = NP/NPTE_PT * sizeof(Page)
    //   NP = size of physical memory in pages
    //   NPTE_PT = number of page table entries per page table
    top_page -= MMU::pts(MMU::pages(si->bm.mem_top - si->bm.mem_base));
    si->pmm.phy_mem_pt = top_page * sizeof(MegaPage);

    // Page tables to map the IO address space
    // = NP/NPTE_PT * sizeof(Page)
    // NP = size of I/O address space in pages
    // NPTE_PT = number of page table entries per page table
    top_page -= MMU::pts(MMU::pages(si->bm.mio_top - si->bm.mio_base));
    si->pmm.io_pts = top_page * sizeof(MegaPage);

    // SYSTEM code segment
    top_page -= MMU::pages(si->lm.sys_code_size);
    si->pmm.sys_code = top_page * sizeof(Page);

    // SYSTEM data segment
    top_page -= MMU::pages(si->lm.sys_data_size);
    si->pmm.sys_data = top_page * sizeof(Page);

    // SYSTEM stack segment
    top_page -= MMU::pages(si->lm.sys_stack_size);
    si->pmm.sys_stack = top_page * sizeof(Page);

    // The memory allocated so far will "disappear" from the system as we set mem_top as follows:
    si->pmm.usr_mem_base = si->bm.mem_base;
    si->pmm.usr_mem_top = top_page * sizeof(Page);

    // Free chunks (passed to MMU::init)
    si->pmm.free1_base = RAM_BASE;
    si->pmm.free1_top = RAM_TOP;
}

void Setup::setup_system_page_directory() {
    db<Setup>(INF) << "Setup::setup_system_page_directory()" << endl;

    // Get the physical address for the System Page Directory
    PT_Entry * sys_pd = reinterpret_cast<PT_Entry *>(si->pmm.sys_pd);

    // Clear the System Page Directory
    memset(sys_pd, 0, sizeof(Page));

    // Calculate the number of page tables needed to map the physical memory
    unsigned int mem_size = MMU::pages(si->bm.mem_top - si->bm.mem_base);
    int n_pts = MMU::pts(mem_size);

    // Map the whole physical memory into the page tables pointed by phy_mem_pts
    PT_Entry * pts = reinterpret_cast<PT_Entry *>(si->pmm.phy_mem_pt);
    for(unsigned int i = 0; i < mem_size; i++)
        pts[i] = MMU::pnn2pte((si->bm.mem_base + i * sizeof(MegaPage)), MMU::Flags::SYS);

    // Attach all physical memory starting at PHY_MEM
    for(unsigned int i = MMU::pdi(MMU::align_page(PHY_MEM)), j = 0; i < MMU::pdi(MMU::align_page(PHY_MEM)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.phy_mem_pt + j * sizeof(Page)));

    // Attach all physical memory starting at MEM_BASE
    for(unsigned int i = MMU::pdi(MMU::align_page(MEM_BASE)), j = 0; i < MMU::pdi(MMU::align_page(MEM_BASE)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.phy_mem_pt + j * sizeof(Page)));

    // Calculate the number of page tables needed to map the IO address space
    unsigned int io_size = MMU::pages(si->bm.mio_top - si->bm.mio_base);
    n_pts = MMU::pts(io_size);

    // Map IO address space into the page tables pointed by io_pts
    pts = reinterpret_cast<PT_Entry *>(si->pmm.io_pts);
    for(unsigned int i = 0; i < io_size; i++)
        pts[i] = MMU::pnn2pte((si->bm.mio_base + i * sizeof(MegaPage)), MMU::Flags::IO);

    // Attach devices' memory at Memory_Map::IO
    for(unsigned int i = MMU::pdi(MMU::align_page(IO)), j = 0; i < MMU::pdi(MMU::align_page(IO)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.io_pts + j * sizeof(Page)));

    // Attach the OS (i.e. sys_pt)
    sys_pd[MMU::pdi(SYS)] = MMU::phy2pde(si->pmm.sys_pt);

    db<Setup>(INF) << "SYS_PD=" << *reinterpret_cast<Page_Table *>(sys_pd) << endl;
}

void Setup::mmu_init() {
    unsigned int pt_entries = MMU::PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP - RAM_BASE + 1);
    unsigned int page_tables = MMU::pts(pages);
    unsigned int attachers = MMU::ats(page_tables);
    unsigned int page_directories = MMU::pds(attachers);

    kout << "################# MMU INIT #################" << endl;
    kout << "Page table entries: " << pt_entries << endl;
    kout << "pages: " << pages << endl;
    kout << "page tables: " << page_tables << endl;
    kout << "attachers: " << attachers << endl;
    kout << "page directories: " << page_directories << endl;

    // set SATP to enable paging for the MMU + Flush TLB
    MMU::set_master(reinterpret_cast<Page_Directory *>(si->pmm.sys_pd));
    CPU::pdp(si->pmm.sys_pd);

    kout << "tlb flushed!" << endl;
    MMU::flush_tlb();

    kout << "satp set!" << endl;
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    if(CPU::mhartid() == 0)                             // SiFive-U always has 2 cores, so we disable CU1 here
        CPU::halt();

    CPU::mstatusc(CPU::MIE);                            // disable interrupts (they will be reenabled at Init_End)
    CPU::mies(CPU::MSI);                                // enable interrupts generation by CLINT
    CLINT::mtvec(CLINT::DIRECT, _int_entry);            // setup a preliminary machine mode interrupt handler pointing it to _int_entry
    CLINT::stvec(CLINT::DIRECT, _int_entry);            // setup a preliminary machine mode interrupt handler pointing it to _int_entry

    CPU::sp(Memory_Map::BOOT_STACK + Traits<Machine>::STACK_SIZE - sizeof(long)); // set the stack pointer, thus creating a stack for SETUP

    Machine::clear_bss();

    CPU::mstatus(CPU::MPP_S | CPU::MPIE);                           // stay in supervisor mode at mret (for MMU SATP) + interruptions
    CPU::mepc(CPU::Reg(&_setup));                       // entry = _setup (in supervisor mode)

    // memory protection (necessary to enable Supervisor mode, which is necessary to enable SATP (MMU))
    CPU::pmpaddr0(-1);                                  // set PMP0 to cover all memory
    CPU::pmpcfg0(0xF);                                  // set PMP0 to be read/write/executable for all modes

    // ensure SATP is zero before enabling MMU
    CPU::satp(0);

    // Delegate exceptions to supervisor mode
    CPU::mideleg(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::medeleg(0xffff);

    CPU::mret();                                            // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)
}

void _setup() // supervisor mode
{
    kerr  << endl;
    kout  << endl;

    Setup setup;
}
