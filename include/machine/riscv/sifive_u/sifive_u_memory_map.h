// EPOS SiFive-U (RISC-V) Memory Map

#ifndef __riscv_sifive_u_memory_map_h
#define __riscv_sifive_u_memory_map_h

#include <system/memory_map.h>

__BEGIN_SYS

struct Memory_Map
{
private:
    static const bool multitask = Traits<System>::multitask;
    static const bool emulated = (Traits<CPU>::WORD_SIZE != 64); // specifying a SiFive-U with RV32 sets QEMU machine to Virt

public:
    enum : unsigned long {
        NOT_USED        = Traits<Machine>::NOT_USED,

        // Physical Memory
        RAM_BASE        = Traits<Machine>::RAM_BASE,
        RAM_TOP         = Traits<Machine>::RAM_TOP,
        MIO_BASE        = Traits<Machine>::MIO_BASE,
        MIO_TOP         = Traits<Machine>::MIO_TOP,
        INT_M2S         = RAM_TOP + 1 - 4096,   // the last page is used by the _int_m2s() interrupt forwarder installed by SETUP; code and stack share the same page, with code at the bottom and the stack at the top
        BOOT_STACK      = (multitask ? INT_M2S : RAM_TOP + 1) - Traits<Machine>::STACK_SIZE, // will be used as the stack's base, not the stack pointer
        FREE_BASE       = RAM_BASE,
        FREE_TOP        = BOOT_STACK,
        PAGE_TABLES     = BOOT_STACK - 64 * 1024 - ((1 + 512 + (512*512)) * 4096) + 1,
        // Memory-mapped devices
        BIOS_BASE       = 0x00001000,   // BIOS ROM
        TEST_BASE       = 0x00100000,   // SiFive test engine
        RTC_BASE        = 0x00101000,   // Goldfish RTC
        UART0_BASE      = emulated ? 0x10000000 : 0x10010000, // NS16550A or SiFive UART
        CLINT_BASE      = 0x02000000,   // SiFive CLINT
        TIMER_BASE      = 0x02004000,   // CLINT Timer
        PLIIC_CPU_BASE  = 0x0c000000,   // SiFive PLIC
        PRCI_BASE       = emulated ? NOT_USED : 0x10000000,   // SiFive-U Power, Reset, Clock, Interrupt
        GPIO_BASE       = emulated ? NOT_USED : 0x10060000,   // SiFive-U GPIO
        OTP_BASE        = emulated ? NOT_USED : 0x10070000,   // SiFive-U OTP
        ETH_BASE        = emulated ? NOT_USED : 0x10090000,   // SiFive-U Ethernet
        FLASH_BASE      = 0x20000000,   // Virt / SiFive-U Flash
        SPI0_BASE       = 0x10040000,   // SiFive-U QSPI 0
        SPI1_BASE       = 0x10041000,   // SiFive-U QSPI 1
        SPI2_BASE       = 0x10050000,   // SiFive-U QSPI 2

        // Physical Memory at Boot
        BOOT            = Traits<Machine>::BOOT,
        IMAGE           = Traits<Machine>::IMAGE,
        SETUP           = Traits<Machine>::SETUP,

        // Logical Address Space
        APP_LOW         = Traits<Machine>::APP_LOW,
        APP_HIGH        = Traits<Machine>::APP_HIGH,
        APP_CODE        = Traits<Machine>::APP_CODE,
        APP_DATA        = Traits<Machine>::APP_DATA,

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
        SYS_HIGH        = multitask ? SYS + 0x007fffff : NOT_USED
    };
};

__END_SYS

#endif
