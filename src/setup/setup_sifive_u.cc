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
    static const unsigned long PHY_MEM           = Memory_Map::PHY_MEM;
    static const unsigned long IMAGE           = Memory_Map::IMAGE;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::Page Page;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;
    typedef MMU::PT_Entry PT_Entry;
    typedef MMU::Flags Flags;

public:
    Setup();

private:
    void say_hi();
    void call_next();

    // mmu
    void build_lm();
    void build_pmm();
    void setup_sys_pt();
    void setup_sys_pd();
    void mmu_init();

private:
    char * bi;
    System_Info * si;
};


Setup::Setup()
{
//    CPU::int_disable();
    Display::init();
    kout << endl;
    kerr << endl;

    bi = reinterpret_cast<char *>(IMAGE);
    si = reinterpret_cast<System_Info *>(&__boot_time_system_info);
    if(si->bm.n_cpus > Traits<Machine>::CPUS)
        si->bm.n_cpus = Traits<Machine>::CPUS;

    db<Setup>(TRC) << "Setup(si=" << reinterpret_cast<void *>(si) << ",sp=" << CPU::sp() << ")" << endl;
    db<Setup>(INF) << "Setup:si=" << *si << endl;

    // Print basic facts about this EPOS instance
    // say_hi();

    // Initialize Logical and physical memory map
    build_lm();
    build_pmm();

    // Make MMU page tables
    setup_sys_pt();
    setup_sys_pd();
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

void Setup::build_lm()
{
    db<Setup>(TRC) << "Setup::build_lm()" << endl;

    // Get boot image structure
    si->lm.has_stp = (si->bm.setup_offset != -1u);
    si->lm.has_ini = (si->bm.init_offset != -1u);
    si->lm.has_sys = (si->bm.system_offset != -1u);
    si->lm.has_app = (si->bm.application_offset != -1u);
    si->lm.has_ext = (si->bm.extras_offset != -1u);

    // Check SETUP integrity and get the size of its segments
    si->lm.stp_entry = 0;
    si->lm.stp_segments = 0;
    si->lm.stp_code = ~0U;
    si->lm.stp_code_size = 0;
    si->lm.stp_data = ~0U;
    si->lm.stp_data_size = 0;

    // Check INIT integrity and get the size of its segments
    si->lm.ini_entry = 0;
    si->lm.ini_segments = 0;
    si->lm.ini_code = ~0U;
    si->lm.ini_code_size = 0;
    si->lm.ini_data = ~0U;
    si->lm.ini_data_size = 0;

    // Check SYSTEM integrity and get the size of its segments
    si->lm.sys_entry = 0;
    si->lm.sys_segments = 0;
    si->lm.sys_code = ~0U;
    si->lm.sys_code_size = 0;
    si->lm.sys_data = ~0U;
    si->lm.sys_data_size = 0;
    si->lm.sys_stack = Memory_Map::SYS_STACK;
    si->lm.sys_stack_size = Traits<System>::STACK_SIZE * si->bm.n_cpus;

    // Check APPLICATION integrity and get the size of its segments
    si->lm.app_entry = 0;
    si->lm.app_segments = 0;
    si->lm.app_code = ~0U;
    si->lm.app_code_size = 0;
    si->lm.app_data = ~0U;
    si->lm.app_data_size = 0;
}

void Setup::build_pmm()
{
    db<Setup>(TRC) << "Setup::build_pmm()" << endl;

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
    si->pmm.sys_pt = top_page * sizeof(Page);

    // System Page Directory (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pd = top_page * sizeof(Page);

    // Page tables to map the whole physical memory
    // = NP/NPTE_PT * sizeof(Page)
    //   NP = size of physical memory in pages
    //   NPTE_PT = number of page table entries per page table
    top_page -= MMU::pts(MMU::pages(si->bm.mem_top - si->bm.mem_base));
    si->pmm.phy_mem_pts = top_page * sizeof(Page);

    // Page tables to map the IO address space = NP/NPTE_PT * sizeof(Page)
    // NP = size of I/O address space in pages
    // NPTE_PT = number of page table entries per page table
    top_page -= MMU::pts(MMU::pages(si->bm.mio_top - si->bm.mio_base));
    si->pmm.io_pts = top_page * sizeof(Page);

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
    si->pmm.free1_base = si->lm.has_ext ? si->lm.app_extra + si->lm.app_extra_size : si->lm.app_data + si->lm.app_data_size;
    si->pmm.free1_top = top_page * sizeof(Page);

    // Test if we didn't overlap SETUP and the boot image
//    if(si->pmm.usr_mem_top <= si->lm.stp_code + si->lm.stp_code_size + si->lm.stp_data_size) {
//        db<Setup>(ERR) << "SETUP would have been overwritten!" << endl;
//        _panic();
//    }
}

void Setup::setup_sys_pt()
{
    db<Setup>(TRC) << "Setup::setup_sys_pt(pmm="
                   << "{si="      << (void *)si->pmm.sys_info
                   << ",pt="      << (void *)si->pmm.sys_pt
                   << ",pd="      << (void *)si->pmm.sys_pd
                   << ",sysc={b=" << (void *)si->pmm.sys_code << ",s=" << MMU::pages(si->lm.sys_code_size) << "}"
                   << ",sysd={b=" << (void *)si->pmm.sys_data << ",s=" << MMU::pages(si->lm.sys_data_size) << "}"
                   << ",syss={b=" << (void *)si->pmm.sys_stack << ",s=" << MMU::pages(si->lm.sys_stack_size) << "}"
                   << "})" << endl;

    // Get the physical address for the System Page Table
    PT_Entry * sys_pt = reinterpret_cast<PT_Entry *>(si->pmm.sys_pt);

    // Clear the System Page Table
    memset(sys_pt, 0, sizeof(Page));

    // System Info
    sys_pt[MMU::pti(SYS_INFO)] = MMU::phy2pte(si->pmm.sys_info, Flags::SYS);

    // Set an entry to this page table, so the system can access it later
    sys_pt[MMU::pti(SYS_PT)] = MMU::phy2pte(si->pmm.sys_pt, Flags::SYS);

    // System Page Directory
    sys_pt[MMU::pti(SYS_PD)] = MMU::phy2pte(si->pmm.sys_pd, Flags::SYS);

    unsigned int i;
    PT_Entry aux;

    // SYSTEM code
    for(i = 0, aux = si->pmm.sys_code; i < MMU::pages(si->lm.sys_code_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::pti(SYS_CODE) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM data
    for(i = 0, aux = si->pmm.sys_data; i < MMU::pages(si->lm.sys_data_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::pti(SYS_DATA) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM stack (used only during init and for the ukernel model)
    for(i = 0, aux = si->pmm.sys_stack; i < MMU::pages(si->lm.sys_stack_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::pti(SYS_STACK) + i] = MMU::phy2pte(aux, Flags::SYS);

    db<Setup>(INF) << "SYS_PT=" << *reinterpret_cast<Page_Table *>(sys_pt) << endl;
}

void Setup::setup_sys_pd()
{
    db<Setup>(TRC) << "setup_sys_pd(bm="
                   << "{memb="  << (void *)si->bm.mem_base
                   << ",memt="  << (void *)si->bm.mem_top
                   << ",miob="  << (void *)si->bm.mio_base
                   << ",miot="  << (void *)si->bm.mio_top
                   << "{si="    << (void *)si->pmm.sys_info
                   << ",spt="   << (void *)si->pmm.sys_pt
                   << ",spd="   << (void *)si->pmm.sys_pd
                   << ",mem="   << (void *)si->pmm.phy_mem_pts
                   << ",io="    << (void *)si->pmm.io_pts
                   << ",umemb=" << (void *)si->pmm.usr_mem_base
                   << ",umemt=" << (void *)si->pmm.usr_mem_top
                   << ",sysc="  << (void *)si->pmm.sys_code
                   << ",sysd="  << (void *)si->pmm.sys_data
                   << ",syss="  << (void *)si->pmm.sys_stack
                   << ",apct="  << (void *)si->pmm.app_code_pt
                   << ",apdt="  << (void *)si->pmm.app_data_pt
                   << ",fr1b="  << (void *)si->pmm.free1_base
                   << ",fr1t="  << (void *)si->pmm.free1_top
                   << ",fr2b="  << (void *)si->pmm.free2_base
                   << ",fr2t="  << (void *)si->pmm.free2_top
                   << "})" << endl;

    // Get the physical address for the System Page Directory
    PT_Entry * sys_pd = reinterpret_cast<PT_Entry *>(si->pmm.sys_pd);

    // Clear the System Page Directory
    memset(sys_pd, 0, sizeof(Page));

    // Calculate the number of page tables needed to map the physical memory
    unsigned int mem_size = MMU::pages(si->bm.mem_top - si->bm.mem_base);
    unsigned int n_pts = MMU::pts(mem_size);

    // Map the whole physical memory into the page tables pointed by phy_mem_pts
    PT_Entry * pts = reinterpret_cast<PT_Entry *>(si->pmm.phy_mem_pts);
    for(unsigned int i = 0; i < mem_size; i++)
        pts[i] = MMU::phy2pte((si->bm.mem_base + i * sizeof(Page)), Flags::SYS);

    // Attach all physical memory starting at PHY_MEM
    assert((MMU::pdi(MMU::align_segment(PHY_MEM)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS
    for(unsigned int i = MMU::pdi(MMU::align_segment(PHY_MEM)), j = 0; i < MMU::pdi(MMU::align_segment(PHY_MEM)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.phy_mem_pts + j * sizeof(Page)));

    // Attach all physical memory starting at MEM_BASE
    assert((MMU::pdi(MMU::align_segment(RAM_BASE)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS
    for(unsigned int i = MMU::pdi(MMU::align_segment(RAM_BASE)), j = 0; i < MMU::pdi(MMU::align_segment(RAM_BASE)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.phy_mem_pts + j * sizeof(Page)));

    // Calculate the number of page tables needed to map the IO address space
    unsigned int io_size = MMU::pages(si->bm.mio_top - si->bm.mio_base);
    n_pts = MMU::pts(io_size);

    // Map IO address space into the page tables pointed by io_pts
    pts = reinterpret_cast<PT_Entry *>(si->pmm.io_pts);
    for(unsigned int i = 0; i < io_size; i++)
        pts[i] = MMU::phy2pte((si->bm.mio_base + i * sizeof(Page)), Flags::IO);

    // Attach devices' memory at Memory_Map::IO
    assert((MMU::pdi(MMU::align_segment(MIO_BASE)) + n_pts) < (MMU::PD_ENTRIES - 3)); // check if it would overwrite the OS
    for(unsigned int i = MMU::pdi(MMU::align_segment(MIO_BASE)), j = 0; i < MMU::pdi(MMU::align_segment(MIO_BASE)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde((si->pmm.io_pts + j * sizeof(Page)));

    // Attach the OS (i.e. sys_pt)
    sys_pd[MMU::pdi(SETUP)] = MMU::phy2pde(si->pmm.sys_pt);

     db<Setup>(INF) << "SYS_PD=" << *reinterpret_cast<Page_Table *>(sys_pd) << endl;
}

void Setup::mmu_init() {
    unsigned int pt_entries = MMU::PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP + 1);
    unsigned int page_tables = MMU::pts(pages);
    unsigned int attachers = MMU::ats(page_tables);
    unsigned int page_directories = MMU::pds(attachers);

    kout << "Page table entries: " << pt_entries << endl;
    kout << "pages: " << pages << endl;
    kout << "page tables: " << page_tables << endl;
    kout << "attachers: " << attachers << endl;
    kout << "page directories: " << page_directories << endl << endl;

    // Map L2 Page Directory
//    unsigned long base = RAM_BASE;
//    auto * master = new ((void *) (base)) Page_Directory();
//    master->remap(base, MMU::RV64_Flags::VALID);

    // Set SATP (to change page allocation) + Flush old TLB
    CPU::pdp(si->pmm.sys_pd);
    CPU::flush_tlb();
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
//    // SiFive-U always has 2 cores, so we disable core 0 here (only core 1 has MMU implemented)
//    if(CPU::mhartid() != 0)
//        CPU::halt();
//
//    CPU::mstatusc(CPU::MIE);                            // disable interrupts (they will be reenabled at Init_End)
//    CPU::mies(CPU::MSI);                                // enable interrupts generation by CLINT
//    CLINT::mtvec(CLINT::DIRECT, _int_entry);            // setup a preliminary machine mode interrupt handler pointing it to _int_entry
//
//    CPU::sp(Memory_Map::BOOT_STACK + Traits<Machine>::STACK_SIZE - sizeof(long)); // set the stack pointer, thus creating a stack for SETUP
//
//    Machine::clear_bss();
//
//    CPU::mstatus(CPU::MPP_M);                           // stay in machine mode at mret
//
//    CPU::mepc(CPU::Reg(&_setup));                       // entry = _setup
//    CPU::mret();                                        // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)


    // SiFive-U core 0 doesn't have MMU
    if (CPU::mhartid() == 0)
        CPU::halt();

//    CPU::mint_disable();
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
