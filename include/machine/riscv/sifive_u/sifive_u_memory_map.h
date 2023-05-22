// EPOS SiFive-U (RISC-V) Memory Map

#ifndef __riscv_sifive_u_memory_map_h
#define __riscv_sifive_u_memory_map_h

#include <system/memory_map.h>

__BEGIN_SYS

struct Memory_Map
{
private:
    static const bool multitask = Traits<System>::multitask;
    static const bool library = (Traits<Build>::MODE == Traits<Build>::LIBRARY);
public:
    enum : unsigned long {
        NOT_USED        = Traits<Machine>::NOT_USED,

        // Physical Memory
        RAM_BASE        = Traits<Machine>::RAM_BASE,
        RAM_TOP         = Traits<Machine>::RAM_TOP,
        MIO_BASE        = Traits<Machine>::MIO_BASE,
        MIO_TOP         = Traits<Machine>::MIO_TOP,
        LAST_PAGE       = RAM_TOP + 1 - 4096,
        INT_M2S         = LAST_PAGE,   		// with multitasking, the last page is used by the _int_m2s() machine mode interrupt forwarder installed by SETUP before going into supervisor mode; code and stack share the same page, with code at the bottom and the stack at the top
        FLAT_MEM_MAP    = LAST_PAGE,       	// in LIBRARY mode, the last page is used for a single-level mapping of the whole memory space
        BOOT_STACK      = LAST_PAGE - Traits<Machine>::STACK_SIZE, // will be used as the stack's base, not the stack pointer

        FREE_BASE       = RAM_BASE,
        FREE_TOP        = BOOT_STACK,

        // Memory-mapped devices
        BIOS_BASE       = 0x0000000000001000,   // BIOS ROM
        TEST_BASE       = 0x0000000000100000,   // SiFive test engine
        RTC_BASE        = 0x0000000000101000,   // Goldfish RTC
        UART0_BASE      = 0x0000000010010000,   // SiFive UART
        CLINT_BASE      = 0x0000000002000000,   // SiFive CLINT
        TIMER_BASE      = 0x0000000002004000,   // CLINT Timer
        PLIIC_CPU_BASE  = 0x000000000c000000,   // SiFive PLIC
        PRCI_BASE       = 0x0000000010000000,   // SiFive-U Power, Reset, Clock, Interrupt
        GPIO_BASE       = 0x0000000010060000,   // SiFive-U GPIO
        OTP_BASE        = 0x0000000010070000,   // SiFive-U OTP
        ETH_BASE        = 0x0000000010090000,   // SiFive-U Ethernet
        FLASH_BASE      = 0x0000000020000000,   // Virt / SiFive-U Flash
        SPI0_BASE       = 0x0000000010040000,   // SiFive-U QSPI 0
        SPI1_BASE       = 0x0000000010041000,   // SiFive-U QSPI 1
        SPI2_BASE       = 0x0000000010050000,   // SiFive-U QSPI 2

        // Physical Memory at Boot
        BOOT            = Traits<Machine>::BOOT,
        IMAGE           = Traits<Machine>::IMAGE,
        SETUP           = Traits<Machine>::SETUP,

        // Logical Address Space
        INIT            = Traits<Machine>::INIT,

        PHY_MEM         = Traits<Machine>::PHY_MEM,

        IO              = Traits<Machine>::IO,

        SYS             = Traits<Machine>::SYS,
        SYS_CODE        = multitask ? SYS + 0x00000000 : NOT_USED,
        SYS_INFO        = multitask ? SYS + 0x00100000 : NOT_USED,
        SYS_PT          = multitask ? SYS + 0x00101000 : NOT_USED,
        SYS_PD          = multitask ? SYS + 0x00102000 : NOT_USED,
        SYS_DATA        = multitask ? SYS + 0x00103000 : NOT_USED,
        SYS_STACK       = multitask ? SYS + 0x00200000 : NOT_USED,
        SYS_HEAP        = multitask ? SYS + 0x00400000 : NOT_USED,
        SYS_HIGH        = multitask ? SYS_HEAP + Traits<Machine>::HEAP_SIZE : NOT_USED,

        APP_LOW         = library ? PHY_MEM : SYS_HIGH,
        APP_HIGH        = Traits<Machine>::APP_HIGH,
        APP_CODE        = APP_LOW,
        APP_DATA        = library ? APP_CODE : APP_CODE + 0x400000
    };
};

__END_SYS

#endif
