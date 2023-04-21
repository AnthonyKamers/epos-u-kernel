// EPOS RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#include <system/memory_map.h>
#include <utility/list.h>
#include <utility/debug.h>
#include <architecture/mmu.h>
#include <architecture/cpu.h>

__BEGIN_SYS

// PPN[2] = 9 bits, PPN[1] = 9 bits, Offset - 12 bits PPN[0] = 9 bits
class MMU: public MMU_Common<9, 9, 12, 9> {
    friend class CPU;
    friend class Setup;

private:
    typedef Grouping_List<Frame> List;
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
            CONTIGUOUS = 1 << 8, // Contiguous (0=non-contiguous, 1=contiguous)
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
        void map(int from, int to, RV64_Flags flags) {}
        void remap(Phy_Addr addr, RV64_Flags flags, int from = 0, int to = PT_ENTRIES, int size = sizeof(Page)) {}
        void unmap(int from = 0, int to = PT_ENTRIES) {}

        // Print Page Table
        friend OStream & operator<<(OStream & os, Page_Table & pt) {
            os << "{\n";
//            for (int i = 0; i < PT_ENTRIES; i++) {
//                if (pt[i])
//                    os << "\n";
//            }
            os << "}";
            return os;
        }
    };

    // Segment initializes that
    class Chunk {
    public:
        // TODO Implement these methods using the new L0, L1, L2 Page Tables
        Chunk() {}
        Chunk(unsigned int bytes, Flags flags, Color color = WHITE) {
            db<MMU>(INF) << "Chamou Chunk(bytes, flags, color)" << endl;
        }
        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags) {
            db<MMU>(INF) << "Chamou Chunk(phy_addr, bytes, flags)" << endl;
        }
        Chunk(Phy_Addr pt, unsigned int from, unsigned int to, Flags flags) {
            db<MMU>(INF) << "Chamou Chunk(pt, from , to, flags)" << endl;
        }
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
        Directory(): _pd(phy2log(calloc(1))), _free(true) {}
        
        explicit Directory(Page_Directory *pd): _pd(pd), _free(false) {
            db<MMU>(INF) << "Call Directory()" << endl;
            for (unsigned int i = 0; i < MMU::PD_ENTRIES; i++) (*_pd)[i] = L2->get_entry(i);
        }
        
        ~Directory() {
            if (_free) free(_pd);
        }

        Page_Directory *pd() const { return _pd; }

        void activate() {
            db<MMU>(INF) << "Activating SATP Directory: " << _pd << endl;
            CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(_pd) >> 12));
            MMU::flush_tlb();
        }

        // TODO implement these methods using the new L0, L1, L2 Page Tables
        Log_Addr attach(const Chunk & chunk) { return 0; }
        Log_Addr attach(const Chunk & chunk, Log_Addr addr) { return 0; }
        void detach(const Chunk & chunk) {}
        void detach(const Chunk & chunk, Log_Addr addr) {}

        static Phy_Addr physical(Log_Addr & addr) { return addr; }

    private:
        Page_Directory *_pd;
        bool _free;
    };

public:
    MMU() { }

    static Phy_Addr alloc(unsigned int bytes) {
        db<MMU>(INF) << "Chamou MMU:alloc(bytes)" << endl;

        Phy_Addr phy(false);
        if (bytes) {
            List::Element * element = _free.search_decrementing(bytes);
            if (element) {
                db<MMU>(INF) << "Object allocated: " << element->object() << endl;
                phy = element->object() + element->size();

                db<MMU>(INF) << "Allocating " << bytes << " bytes at " << phy << "with size " << element->size() << endl;
            }
        }

        return phy;
    }

    static Phy_Addr calloc(unsigned int bytes) {
        Phy_Addr phy = alloc(bytes);
        memset(phy2log(phy), 0, sizeof(Frame) * bytes);
        return phy;
    }

    static void free(Phy_Addr addr, unsigned int bytes = 1) {
        db<MMU>(INF) << "Chamou MMU:free(" << addr << ", " << bytes << ")" << endl;
        addr = ind(addr);

        // make sure it is aligned
        assert(Traits<CPU>::unaligned_memory_access || !(addr % 4));

        if (addr && bytes) {
            List::Element * element = new (addr) List::Element(addr, bytes);
            List::Element *m1, *m2;

            db<MMU>(TRC) << "Creating element at list: " << &m2 << endl;
            _free.insert_merging(element, &m1, &m2);
        }
        db<MMU>(TRC) << "List size: " << _free.size() << endl;
    }

    static unsigned int allocable() {
        return _free.head() ? _free.head()->size() : 0;
    }

    // TODO check if this is correct
    static Page_Directory * volatile current() {
        return static_cast<Page_Directory * volatile>(phy2log(CPU::satp() << 12));
    }

    static void set_L2(Page_Directory * pd) { L2 = pd; }
    static void set_L1(Page_Table * page_table) { L1 = page_table; }
    static void set_L0(Page_Table * page_table) { L0 = page_table; }

    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

private:
    static void init();

    // TODO check if these util functions are correct
    static Log_Addr phy2log(Phy_Addr physical) { return physical; }
    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::VALID; }
    static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }
    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static RV64_Flags pde2flg(PT_Entry entry) { return (entry & RV64_Flags::MASK); }

private:
    static List _free;
    static Page_Directory * L2;
    static Page_Table * L1;
    static Page_Table * L0;

};

__END_SYS

#endif
