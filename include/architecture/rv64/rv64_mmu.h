// EPOS RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#define __mmu_common_only__
#include <architecture/mmu.h>
#undef __mmu_common_only__
#include <system/memory_map.h>
#include <utility/debug.h>

__BEGIN_SYS

// PPN[2] = 9 bits, PPN[1] = 9 bits, Offset - 12 bits PPN[0] = 9 bits
class MMU: public MMU_Common<9, 9, 12, 9> {
    friend class CPU;
    friend class Setup;

private:
    typedef Grouping_List<unsigned int> List;
    static const unsigned int RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned int APP_LOW = Memory_Map::APP_LOW;
    static const unsigned int PHY_MEM = Memory_Map::PHY_MEM;

public:
    // Page Flags
    class RV64_Flags {
    public:
        enum {
            VALID = 1 << 0, // Valid
            READ = 1 << 1, // Readable
            WRITE = 1 << 2, // Writable
            EXECUTE = 1 << 3, // Executable
            USER = 1 << 4, // User mode page
            GLOBAL = 1 << 5, // Global page (only for OS)
            ACCESSED = 1 << 6, // Page has been accessed
            DIRTY = 1 << 7, // Page has been written
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
    private:
        PT_Entry page_tables[PT_ENTRIES];
    public:
        Page_Table() {}

        PT_Entry & operator[](unsigned int i) { return page_tables[i]; }
        PT_Entry get_entry(unsigned int i) { return page_tables[i]; }
        void map(RV64_Flags flags, int from, int to) {}
        void remap(Phy_Addr addr, RV64_Flags flags, int from = 0, int to = PT_ENTRIES, int size = sizeof(Page)) {}
        void unmap(int from = 0, int to = PT_ENTRIES) {}

        // Print Page Table
//        friend OStream & operator<<(OStream & os, Page_Table & pt) {
//            os << "{\n";
//            for (int i = 0; i < MMU::PT_ENTRIES; i++) {
//                if (pt[i])
//                    os << "[" << i << "] \t" << pde2phy(pt[i]) << " " << hex << pde2flg(pt[i]) << dec << "\n";
//            }
//            os << "}";
//            return os;
//        }
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
        Directory() {}
        Directory(Page_Directory *pd) {}
        ~Directory() {}

        Page_Directory *pd() const { return _pd; }
        void activate() {}
        Log_Addr attach(const Chunk & chunk) { return 0; }
        Log_Addr attach(const Chunk & chunk, Log_Addr addr) { return 0; }
        void detach(const Chunk & chunk) {}
        void detach(const Chunk & chunk, Log_Addr addr) {}
        void detach(Log_Addr addr, unsigned int bytes) {}
        Phy_Addr physical(Log_Addr addr) { return 0; }

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

private:
    static void init();
    static Log_Addr phy2log(Phy_Addr physical) { return 0; }
    static PD_Entry phy2pde(Phy_Addr bytes) { return 0; }
    static Phy_Addr pde2phy(PD_Entry entry) { return 0; }
    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags) { return 0; }
    static Phy_Addr pte2phy(PT_Entry entry) { return 0; }
    static RV64_Flags pte2flg(PT_Entry entry) { return 0; }
};

__END_SYS

#endif
