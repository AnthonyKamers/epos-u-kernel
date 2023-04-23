// EPOS RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#define __mmu_common_only__
#include <architecture/mmu.h>
#undef __mmu_common_only__
#include <system/memory_map.h>
#include <utility/debug.h>

__BEGIN_SYS

// Page Directory = 9 bits (4kb)
// Page Table Segment = 21 bits (
class MMU: public MMU_Common<9, 21, 12> {
    friend class CPU;
    friend class Setup;

private:
    typedef unsigned char MegaPage[PG_CHUNK_SIZE];  // 2Mb page
    typedef Grouping_List<Frame> ListPage;          // 4Kb page list
    typedef Grouping_List<MegaPage> ListMegaPage;   // 2Mb page list

    static const unsigned int RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned int APP_LOW = Memory_Map::APP_LOW;
    static const unsigned int PHY_MEM = Memory_Map::PHY_MEM;

public:
    // Page Flags
    class RV64_Flags {
    public:
        enum {
            VALID = 1 << 0,         // Valid
            READ = 1 << 1,          // Readable
            WRITE = 1 << 2,         // Writable
            EXECUTE = 1 << 3,       // Executable
            USER = 1 << 4,          // User mode page
            GLOBAL = 1 << 5,        // Global page (only for OS)
            ACCESSED = 1 << 6,      // Page has been accessed
            DIRTY = 1 << 7,         // Page has been written
            MASK = (1 << 10) - 1
        };

        RV64_Flags() {}
        RV64_Flags(const RV64_Flags &f) : _flags(f) {}
        RV64_Flags(unsigned int f) : _flags(f) {}
        RV64_Flags(const Flags &f) : _flags(((f & Flags::PRE) ? VALID : 0) |
                                            ((f & Flags::RW) ? (READ | WRITE) : READ) |
                                            ((f & Flags::USR) ? USER : 0) |
                                            ((f & Flags::EX) ? EXECUTE : 0) | ACCESSED | DIRTY) {}
        operator unsigned int() const { return _flags; }

    private:
        unsigned int _flags;
    };

    // Page Flags
    typedef Flags Page_Flags;

    class Page_Table {
    public:
        Page_Table() {}

        PT_Entry & operator[](unsigned int i) { return _page_table[i]; }
        PT_Entry get_entry(unsigned int i) { return _page_table[i]; }

        void map(unsigned long from, unsigned long to, RV64_Flags flags) {
            Phy_Addr * addr = alloc(to - from);
            if (addr) remap(addr, from, to, flags);
            else
                for(; from < pnn2pte(alloc(1), flags))
                    _page_table[from] = pnn2pte(alloc(1), flags);
        }

        void remap(Phy_Addr addr, unsigned long from, unsigned long to, RV64_Flags flags) {
            addr = align_page(addr);
            for(; from < pnn2pte(alloc(1), flags)) {
                _page_table[from] = pnn2pte(alloc(1), flags);
                addr += sizeof(Frame);
            }
        }
    private:
        PT_Entry _page_table[PT_ENTRIES];
    };

    // Segment initializes that
    class Chunk {
    public:
        Chunk() {}
        Chunk(unsigned int bytes, Flags flags, Color color = WHITE) {}
        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags) {}
        Chunk(Phy_Addr pt, unsigned int from, unsigned int to, Flags flags) {}
        ~Chunk() {}

        unsigned int pts() const { return _pts; }
        Page_Table *pt() const { return _pt; }
        unsigned int size() const { return _bytes; }
        RV64_Flags flags() const {return _flags;}
        Phy_Addr phy_address() const { return _phy_addr; } // always CT
        int resize(unsigned int amount) { return 0; }
    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _pts;
        unsigned int _bytes;
        RV64_Flags _flags;
        Phy_Addr _phy_addr;
        Page_Table *_pt;
    };

    // Page Directory (L2)
    typedef Page_Table Page_Directory;

    // Directory (for Address Space)
    class Directory {
    public:
        Directory(): _pd(calloc(1)), _free(true) {}

        Directory(Page_Directory *pd): _pd(pd), _free(false) {}

        ~Directory() { if(_free) free(_pd); }

        Page_Directory *pd() const { return _pd; }

        // Mode = 8 (1000) = Sv39
        void activate() { CPU::satp((1UL << 63) | reinterpret_cast<CPU::Reg64>(_pd) >> PT_SHIFT); }

        Log_Addr attach(const Chunk & chunk) {
            for(unsigned long i = from; i < PD_ENTRIES; i++)
                if(attach(i, chunk.pt(), chunk.pts(), chunk.flags()))
                    return i << AT_SHIFT;
            return Log_Addr(false);
        }

        Log_Addr attach(const Chunk & chunk, Log_Addr addr) {
            unsigned long from = directory(addr);
            if(attach(from, chunk.pt(), chunk.pts(), chunk.flags()))
                return from << AT_SHIFT;
            return Log_Addr(false);
        }

        void detach(const Chunk & chunk) {
            for(unsigned long i = 0; i < PD_ENTRIES; i++)
                if(ind((*_pd)[i]) == ind(pnn2pde(chunk.pt()))) {
                    detach(i, chunk.pt(), chunk.pts());
                    return;
                }

            db<MMU>(WRN) << "MMU::detach(chunk): failed to detach!" << endl;
        }

        void detach(const Chunk & chunk, Log_Addr addr) {
            unsigned long from = directory(addr);
            if(ind((*_pd)[from]) != ind(chunk.pt())) {
                db<MMU>(WRN) << "MMU::detach(chunk, addr): failed to detach" << endl;
                return;
            }

            // actually detach it
            detach(from, chunk.pt(), chunk.pts());
        }

        // TODO check if auxiliary methods are being used correctly
        Phy_Addr physical(Log_Addr addr) {
            Page_Table * pt = reinterpret_cast<Page_Table *>((void *)(*_pd)[ati(addr)]);
            return (*pt)[pti(addr)] | off(addr);
        }

    private:
        bool attach(unsigned long from, const Page_Table * pt, unsigned long n, RV64_Flags flags) {
            for(unsigned long i = from; i < from + n; i++)
                if((*_pd)[i])
                    return false;
            for(unsigned long i = from; i < from + n; i++, pt++)
                (*_pd)[i] = pnn2pde(Phy_Addr(pt));
            return true;
        }

        void detach(unsigned long from, const Page_Table * pt, unsigned long n) {
            for(unsigned long i = from; i < from + n; i++)
                (*_pd)[i] = 0;
        }

    private:
        Page_Directory *_pd;
        bool _free;
    };

public:
    MMU() {}

    static Phy_Addr alloc(unsigned int bytes) { return 0; }
    static Phy_Addr calloc(unsigned int bytes) { return 0; }
    static void free(Phy_Addr addr, unsigned int bytes) {}
    static unsigned int allocable() { return 0; }
    static Page_Directory * volatile current() { return 0; }
    static Phy_Addr physical(Log_Addr addr) { return 0; }

    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

    static void master(Page_Directory * master) { _master = master; }
    static void mega_chunk(Page_Table * mega_chunk) { _mega_chunk = mega_chunk; }

private:
    static void init();

    // PNN -> PTE
    static PT_Entry pnn2pte(Phy_Addr frame, RV64_Flags flags) { return (frame >> 2) | flags; }

    // PNN -> PDE (Page Directory Entry = pte, but with X | R | W = 0)
    static PD_Entry pnn2pde(Phy_Addr frame) { return (frame >> 2) | RV64_Flags::VALID; }

private:
    // Page tables
    static ListPage _free;
    static Page_Directory *_master;

    // Page tables segments (chunks)
    static ListMegaPage _free_mb;
    static Page_Table *_mega_chunk;
};

__END_SYS

#endif
