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
// Page Table Segment = 21 bits (2Mb)
// Offset = 12 bits (Sv39 RV64)
class MMU: public MMU_Common<9, 21, 12> {
    friend class CPU;
    friend class Setup;

private:
    typedef Grouping_List<Frame> ListPage;          // 4Kb page list
    typedef Grouping_List<MegaPage> ListMegaPage;   // 2Mb page list

    static const unsigned int RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned int RAM_TOP = Memory_Map::RAM_TOP;
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

        void map(unsigned long from, unsigned long to, RV64_Flags flags) {
            db<MMU>(INF) << "MMU::Page_Table::map(from=" << from << ",to=" << to << ",flags=" << flags << ")" << endl;

            Phy_Addr * addr = alloc(to - from);
            if (addr) remap(addr, from, to, flags);
            else
                for(; from < to; from++)
                    _page_table[from] = pnn2pte(alloc(1), flags);
        }

        void remap(Phy_Addr addr, unsigned long from, unsigned long to, RV64_Flags flags) {
            db<MMU>(WRN) << "MMU::Page_Table::remap(addr=" << addr << ",from=" << from << ",to=" << to << ",flags=" << flags << ")" << endl;

            addr = align_page(addr);
            db<MMU>(WRN) << "MMU::Page_Table::remap(alligned!=" << addr << endl;
            for(; from < to; from++) {
            	db<MMU>(WRN) << "MMU::Page_Table::remap(Looking for stuff" << endl;
                _page_table[from] = pnn2pte(addr, flags);
                addr += sizeof(Frame);
            }
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
    private:
        PT_Entry _page_table[PT_ENTRIES];
    };

    // Segment initializes that
    class Chunk {
    public:
        Chunk() {}

        Chunk(unsigned int bytes, Flags flags, Color color = WHITE) {
            // init variables
            _from = 0;
            _to = pages(bytes);
            _pts = MMU::pts(_to - _from);
            _pt = calloc_segment(_pts);
            _flags = RV64_Flags(flags);

            // map
            _pt->map(_from, _to, _flags);
        }

        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags) {
            // init variables
            _from = 0;
            _to = pages(bytes);
            _pts = MMU::pts(_to - _from);
            _pt = calloc_segment(_pts);
            _flags = RV64_Flags(flags);
            _phy_addr = phy_addr;

            // map
            _pt->remap(_phy_addr, _from, _to, _flags);
        }

        // TODO Check that
        ~Chunk() {
            for (; _from < _to; _from++)
                free_segment((*static_cast<Page_Table *>(phy2log(_pt)))[_from], _bytes);
        }

        unsigned int pts() const { return _pts; }
        Page_Table *pt() const { return _pt; }
        unsigned int size() const { return _bytes; }
        RV64_Flags flags() const {return _flags;}
        Phy_Addr phy_address() const { return _phy_addr ? _phy_addr : Phy_Addr(ind((*_pt)[_from])); } // always CONTIGUOUS
        int resize(unsigned int amount) { return 0; }
    private:
        unsigned int _bytes;
        unsigned int _from;
        unsigned int _to;
        Page_Table *_pt;
        unsigned int _pts;
        RV64_Flags _flags;
        Phy_Addr _phy_addr;
    };

    // Page Directory (L2)
    typedef Page_Table Page_Directory;

    // Directory (for Address Space)
    class Directory {
    public:
        Directory(): _pd(calloc(1)), _free(true) {
            for (unsigned long i = 0; i < PD_ENTRIES; i++) {
                (*_pd)[i] = (*_master)[i];
            }
        }

        Directory(Page_Directory *pd): _pd(phy2log(calloc(1))), _free(false) {}

        ~Directory() { if(_free) free(_pd); }

        Page_Directory *pd() const { return _pd; }

        // Mode = 8 (1000) = Sv39
        void activate() { CPU::satp((1UL << 63) | reinterpret_cast<CPU::Reg64>(_pd) >> PT_SHIFT); }

        Log_Addr attach(const Chunk & chunk) {
            return attach(chunk, APP_LOW);
        }

        Log_Addr attach(const Chunk & chunk, Log_Addr addr) {
            unsigned long from = pdi(addr);
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
            unsigned long from = pdi(addr);
            if(ind((*_pd)[from]) != ind(chunk.pt())) {
                db<MMU>(WRN) << "MMU::detach(chunk, addr): failed to detach" << endl;
                return;
            }

            // actually detach it
            detach(from, chunk.pt(), chunk.pts());
        }

        // TODO check if auxiliary methods are being used correctly
        Phy_Addr physical(Log_Addr addr) {
            Page_Table * pt = reinterpret_cast<Page_Table *>((void *)(*_pd)[pdi(addr)]);
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

    // Alloc Page Tables
    static Phy_Addr alloc(unsigned int frames = 1) {
        Phy_Addr phy(false);

        if (frames) {
            ListPage ::Element * element = _free.search_decrementing(frames);
            if (element) {
                phy = element->object() + element->size();
                db<MMU>(INF) << "MMU::alloc(frames): " << frames << ": " << phy << endl;
            } else {
                db<MMU>(WRN) << "MMU::alloc(frames): " << frames << " failed" << endl;
            }
        }
        return phy;
    }

    static Phy_Addr calloc(unsigned int frames = 1) {
        Phy_Addr physical = alloc(frames);
        memset(phy2log(physical), 0, sizeof(Frame) * frames);
        return physical;
    }

    static void free(Phy_Addr frame, unsigned int qtd_frames = 1) {
        db<MMU>(INF) << "MMU::free(frame=" << frame << ",qtd_frames=" << qtd_frames << ")" << endl;

        frame = ind(frame);

        if (frame && qtd_frames) {
            auto * element = new (phy2log(frame)) ListPage::Element(frame, qtd_frames);
            ListPage::Element * m1, * m2;
            _free.insert_merging(element, &m1, &m2);
        } else {
            db<MMU>(WRN) << "MMU::free(frame=" << frame << ",qtd_frames=" << qtd_frames << "): failed" << endl;
        }
    }

    // Alloc Segments
    static Phy_Addr alloc_segment(unsigned int bytes = 1) {
        Phy_Addr phy(false);

        if (bytes) {
            ListMegaPage ::Element * element = _free_mb.search_decrementing(bytes);
            if (element) {
                phy = element->object() + element->size();
                db<MMU>(INF) << "MMU::alloc_segment(bytes): " << bytes << ": " << phy << endl;
            } else {
                db<MMU>(WRN) << "MMU::alloc_segment(bytes): " << bytes << " failed" << endl;
            }
        }
        return phy;
    }

    static Phy_Addr calloc_segment(unsigned int bytes = 1) {
        Phy_Addr physical = alloc_segment(bytes);
        memset(phy2log(physical), 0, sizeof(MegaPage) * bytes);
        return physical;
    }

    static void free_segment(Phy_Addr segment, unsigned int qtd_bytes) {
        db<MMU>(INF) << "MMU::free_segment(segment=" << segment << ",qtd_bytes=" << qtd_bytes << ")" << endl;

        segment = ind(segment); // TODO Check that

        if (segment && qtd_bytes) {
            auto * element = new (phy2log(segment)) ListMegaPage::Element(segment, qtd_bytes);
            ListMegaPage::Element * m1, * m2;
            _free_mb.insert_merging(element, &m1, &m2);
        } else {
            db<MMU>(WRN) << "MMU::free_segment(segment=" << segment << ",qtd_bytes=" << qtd_bytes << "): failed" << endl;
        }
    }

    // If it is allocable for page tables
    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }

    // Current PNN on SATP
    static Page_Directory * volatile current() {
        return static_cast<Page_Directory *>(phy2log((Phy_Addr)(CPU::satp() << PT_SHIFT)));
    }

    // Get physical address from logical address
    static Phy_Addr physical(Log_Addr addr) {
        Page_Directory * page_directory = current();
        Page_Table * page_table = (*page_directory)[pdi(addr)];
        return (*page_table)[pti(addr)] | off(addr);
    }

    // Multihart flush functions
    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

    // setters
    static void set_master(Page_Directory * master) { _master = master; }

    // SATP utils
    static void set_satp() {
        CPU::satp((1UL << 63) | reinterpret_cast<unsigned long>(_master) >> 12);
    }

private:
    static void init();

    // Util functions
    static PT_Entry pnn2pte(Phy_Addr frame, RV64_Flags flags) { return (frame >> 2) | flags; }
    static PD_Entry pnn2pde(Phy_Addr frame) { return (frame >> 2) | RV64_Flags::VALID; }
    static Log_Addr phy2log(const Phy_Addr & physical) { return physical; }

    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::VALID; }
    static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }
    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }
    static RV64_Flags pde2flg(PT_Entry entry) { return (entry & RV64_Flags::MASK); }

private:
    // Page tables (4Kb)
    static ListPage _free;
    static Page_Directory *_master;

    // Page tables segments (chunks) (2Mb)
    static ListMegaPage _free_mb;
};

__END_SYS

#endif
