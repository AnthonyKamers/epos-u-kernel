// EPOS RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#include <system/memory_map.h>
#include <utility/list.h>
#include <utility/debug.h>
#include <architecture/mmu.h>
#include <architecture/cpu.h>

__BEGIN_SYS

// Page directory   (L2)    = 9 bits (512 entries)
// Page table       (L0)    = 9 bits (512 entries)
// Offset                   = 12 bits (4096 bytes)
// Attacher         (L1)    = 9 bits (512 entries)
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
            VALID = 1 << 0,         // Valid
            READ = 1 << 1,          // Readable
            WRITE = 1 << 2,         // Writable
            EXECUTE = 1 << 3,       // Executable
            USER = 1 << 4,          // User mode page
            GLOBAL = 1 << 5,        // Global page (only for OS)
            ACCESSED = 1 << 6,      // Page has been accessed
            DIRTY = 1 << 7,         // Page has been written
            CONTIGUOUS = 1 << 8,    // Contiguous (0=non-contiguous, 1=contiguous)
            MASK = (1 << 10) - 1,    // Mask to remove the offset

            SYSTEM  = (VALID | READ | WRITE | EXECUTE),
            APP  = (VALID | READ | WRITE | EXECUTE | USER),
            APPC = (VALID | READ | EXECUTE | USER),
            APPD = (VALID | READ | WRITE | USER)
        };

        RV64_Flags() {};
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
        Page_Table() {};

        PT_Entry & operator[](unsigned int i) { return page_tables[i]; }
        PT_Entry get_entry(unsigned int i) { return page_tables[i]; }

        void map(unsigned int from, unsigned int to, RV64_Flags flags) {
            Phy_Addr *addr = alloc(to - from);
            if (addr) remap(addr, flags, from, to);
            else
                for(; from < to; from++)
                    page_tables[from] = phy2pte(alloc(1), flags);
        }

        void remap(Phy_Addr addr, RV64_Flags flags, unsigned int from = 0, unsigned int to = PT_ENTRIES) {
            addr = align_page(addr);
            for (; from < to; from++) {
                Log_Addr *page_table_entry = phy2log(&page_tables[from]);
                *page_table_entry = phy2pte(addr, flags);
                addr += sizeof(Frame);
            }
        }

        void map_system(unsigned int index, unsigned long addr, RV64_Flags flags = RV64_Flags::SYSTEM) {
            page_tables[index] = phy2pte(pdi(addr), flags);
        }

        // Print Page Table
        friend OStream & operator<<(OStream & os, Page_Table & pt) {
            os << "{\n";
            for (unsigned int i = 0; i < MMU::PT_ENTRIES; i++) {
                if (pt[i])
                    os << "\n";
            }
            os << "}";
            return os;
        }
    };

    // Segment initializes that
    class Chunk {
    public:
        Chunk() {};
        Chunk(unsigned int bytes, Flags flags, Color color = WHITE) {
            db<MMU>(INF) << "Called Chunk(bytes, flags, color)" << endl;

            _from = 0;
            _to = pages(bytes);
            _bytes = bytes;
            _flags = RV64_Flags(flags);

            // L0 (page table -> pts)
            _l0_size = MMU::pts(_to - _from);
            _l0 = calloc(_l0_size);
            _l0->map(0, _l0_size, _flags);

            // L1 (attacher -> ats)
            _l1_size = MMU::ats(_l0_size);
            _l1 = calloc(_l1_size);
            _l1->map(_l0_size, _l0_size + _l1_size, _flags);
        }

        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags) {
            db<MMU>(INF) << "Called Chunk(phy_addr, bytes, flags)" << endl;
            _from = 0;
            _to = pages(bytes);
            _bytes = bytes;
            _flags = RV64_Flags(flags);
            _phy_addr = phy_addr;

            // L0 (page table -> pts)
            _l0_size = MMU::pts(_to - _from);
            _l0 = calloc(_l0_size);
            _l0->remap(phy_addr, _flags, 0, _l0_size);

            // L1 (attacher -> ats)
            _l1_size = MMU::ats(_l0_size);
            _l1 = calloc(_l1_size);
            _l1->remap(phy_addr, _flags, _l0_size, _l0_size + _l1_size);
        }

        ~Chunk() {
            for (; _from < _to; _from++) {
                free((*static_cast<Page_Table *>(phy2log(_l1)))[_from]);
                free((*static_cast<Page_Table *>(phy2log(_l0)))[_from]);
            }
            // the root page table is not destructed in the previous loop
            free(_l0, _l0_size);
            free(_l1, _l1_size);
        }

        // L0
        unsigned int pts() const { return _l0_size; }
        Page_Table *pt() const { return _l0; }

        // L1
        unsigned int ats() const { return _l1_size; }
        Page_Table *at() const { return _l1; }

        bool attach_entry(unsigned int from) const {
            // run through level 1 (attacher)
            unsigned limit_l1 = from + _l1_size;
            for (unsigned i = from; i < limit_l1; i++)
                if ((*_l1)[i]) return false;

            // attach L1 (attacher) to L2 (page directory)
            Page_Table * l1_now = _l1;
            for (unsigned i = from; i < limit_l1; i++, l1_now++)
                (*_l1)[i] = phy2pde(Phy_Addr(l1_now)) | _flags;

            // run though level 0 (page table)
            Page_Table * l0_now = _l0;
            for (unsigned i = 0; i < _l1_size; i++)
                for (unsigned j = 0; j < _l0_size; j++, l0_now++) {
                    if ((*l0_now)[j]) return false;
                    (*_l0)[j] = phy2pte(Phy_Addr(l0_now), _flags);
                }

            // if arrived until here, it attached correctly
            return true;
        }

        unsigned int size() const { return _bytes; }
        RV64_Flags flags() const {return _flags;}
        Phy_Addr phy_address() const { return _phy_addr; } // always CONTIGUOUS
        int resize(unsigned int amount) { return 0; }  // if it is CONTIGUOUS, not necessary to resize
    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _bytes;
        RV64_Flags _flags;
        Phy_Addr _phy_addr;

        Page_Table * _l1;
        unsigned int _l1_size;

        Page_Table * _l0;
        unsigned int _l0_size;
    };

    // Page Directory (L2)
    typedef Page_Table Page_Directory;

    // Directory (for Address Space)
    class Directory {
    public:
        Directory(): _pd(phy2log(calloc(1))), _free(true) {}
        
        explicit Directory(Page_Directory *pd): _pd(pd), _free(false) {
            db<MMU>(INF) << "Call Directory()" << endl;
            for (unsigned int i = 0; i < MMU::PD_ENTRIES; i++) (*_pd)[i] = _pd->get_entry(i);
        }
        
        ~Directory() {
            if (_free) free(_pd);
        }

        Page_Directory *pd() const { return _pd; }

        void activate() {
            db<MMU>(INF) << "Activating SATP Directory: " << _pd << endl;
            CPU::pdp(reinterpret_cast<unsigned long>(_pd));
            MMU::flush_tlb();
        }

        Log_Addr attach(const Chunk & chunk) {
            db<MMU>(INF) << "Called Directory:attach(chunk)" << endl;
            return attach(chunk, APP_LOW);
        }

        Log_Addr attach(const Chunk & chunk, Log_Addr addr) {
            db<MMU>(INF) << "Called Directory:attach(chunk, addr)" << endl;

            // attach between different levels
            unsigned int from = directory_bits(addr);
            for (; from < MMU::PD_ENTRIES; from++)
                for (unsigned int i = 0; i < MMU::PT_ENTRIES; i++)
                    if (chunk.attach_entry(i))
                        return i << PD_SHIFT;

            // if arrived here, cannot attach to anything
            return Log_Addr(false);
        }

        void detach(const Chunk & chunk) {
            // check if attacher directory is the same as the current directory
            // dettach L1 (attacher) from L2 (page directory)
            for (unsigned i = 0; i < MMU::PD_ENTRIES; i++)
                if (ind((*_pd)[i]) == ind(phy2pde(chunk.at()))) {
                    // detach page directory, attacher and page table
                    detach_general(_pd, i, chunk.ats());
                    detach_general(chunk.at(), 0, chunk.pts());
                    detach_general(chunk.pt(), 0, chunk.pts());

                    break;
                }
        }

        void detach(const Chunk & chunk, Log_Addr addr) {
            unsigned from = directory_bits(addr);
            detach_general(_pd, from, chunk.ats());
            detach_general(chunk.at(), from, chunk.pts());
            detach_general(chunk.pt(), from, chunk.pts());
        }

        static Phy_Addr physical(Log_Addr & addr) { return addr; }

    private:
        void detach_general(Page_Table * page_table, unsigned from, unsigned size) {
            for (unsigned i = from; i < from + size; i++)
                (*page_table)[i] = 0;
        }

    private:
        Page_Directory *_pd;
        bool _free;
    };

public:
    MMU() { }

    static Phy_Addr alloc(unsigned int bytes) {
        db<MMU>(INF) << "Called MMU:alloc(bytes)" << endl;

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
        db<MMU>(INF) << "Called MMU:free(" << addr << ", " << bytes << ")" << endl;
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

    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }
    static Page_Directory * volatile current() { return static_cast<Page_Directory * volatile>(phy2log(CPU::satp() << 12)); }
    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

private:
    static void init();

    static Log_Addr phy2log(Phy_Addr const & physical)  { return physical; }
    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::VALID; }
    static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }
    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static RV64_Flags pde2flg(PT_Entry entry) { return (entry & RV64_Flags::MASK); }

private:
    static List _free;

};

__END_SYS

#endif
