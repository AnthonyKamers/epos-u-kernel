// EPOS RISC-V QSPI Mediator Declarations

#ifndef __riscv_qspi_h
#define __riscv_qspi_h

#include <architecture/cpu.h>
#include <machine/spi.h>
#include <system/memory_map.h>

__BEGIN_SYS

class QSPI: private SPI_Common
{
private:
    typedef CPU::Reg8 Reg8;
    typedef CPU::Reg32 Reg32;

private:
    static const unsigned int CLOCK = Traits<SPI>::CLOCK;
    static const unsigned DEF_PROTOCOL = Traits<SPI>::DEF_PROTOCOL;
    static const unsigned DEF_MODE = Traits<SPI>::DEF_MODE;
    static const unsigned DEF_BIT_RATE = Traits<SPI>::DEF_BIT_RATE;
    static const unsigned DEF_DATA_BITS = Traits<SPI>::DEF_DATA_BITS;

public:
      // SPI registers offsets from SPI_BASE
    enum {
        SCKDIV  = 0x00, // Serial Clock divisor register    (Serial clock divisor)
        SCKMODE = 0x04, // SPI mode and control register    (Serial clock mode)
        CSID    = 0x10, // Chip select ID register
        CSDEF   = 0x14, // Chip select default register
        CSMODE  = 0x18, // Chip select mode register        (Chip select mode)
        DELAY0  = 0x28, // Delay control register 0
        DELAY1  = 0x2c, // Delay control register 1
        FMT     = 0x40, // Frame format
        TXDATA  = 0x48, // Transmit data register           (Transfer FIFO data)
        RXDATA  = 0x4c, // Receive data register            (Received FIFO data)
        TXMARK  = 0x50, // Transmit watermark register
        RXMARK  = 0x54, // Receive watermark register
        FCTRL   = 0x60, // Flash interface control register
        FFMT    = 0x64, // Flash interface timing register
        IE      = 0x70, // Interrupt enable register        (Interrupt enable)
        IP      = 0x74, // Interrupt pending register       (Interrupt pending)
    };

    // Useful bits from multiple registers
    enum {
        SCK_DIV = 0x3,
        FMT_FLASH = 0x00080008,
        FMT_NON_FLASH = 0x00080000,
        RXD_EMPTY = 1 << 31,
        RXD_DATA = 0xFF << 0,
        TXD_FULL = 1 << 31,

        TXWM = 1 <<  0,   // IE/IP, TX water mark

        SCK_PHA = 0b0,
        SCK_POL = 0b1,
        MODE_QUAD = 0b10,
        MODE_DUAL = 0b01,
        MODE_SINGLE = 0b00
    };
public:
    Reg32 rxd_now;

private:
  static volatile CPU::Reg32 & reg(unsigned int o) {
      return reinterpret_cast<volatile CPU::Reg32 *>(Memory_Map::SPI2_BASE)[o / sizeof(CPU::Reg32)];
  }

public:
  QSPI(unsigned int clock = CLOCK, unsigned int protocol = DEF_PROTOCOL, unsigned int mode = DEF_MODE,
       unsigned int bit_rate = DEF_BIT_RATE, unsigned int data_bits = DEF_DATA_BITS)
   {
    config(clock, protocol, mode, bit_rate, data_bits);
  }

  void config(unsigned int clock, unsigned int protocol, unsigned int mode, unsigned int bit_rate, unsigned int data_bits) {
    reg(SCKDIV) = 0x3; // default reset

    switch (mode) {
        case 0:
            reg(FMT) = FMT_FLASH;
            break;
        case 1:
            reg(FMT) = FMT_NON_FLASH;
            break;
    }

    reg(TXMARK) = mode << 0;
    reg(RXMARK) = 0x0 << 0;
  }

  int check_ie() {
      return reg(IE);
  }

  int check_ip() {
      return reg(IP);
  }

  bool rxd_ok() {
      rxd_now = reg(RXDATA);
      return !(reg(RXDATA) & RXD_EMPTY);
  }

  int get() {
//    while(!rxd_ok());
    return reg(RXDATA);
  }

  bool try_get(int * data) {
    if (rxd_ok()) {
      *data = get();
      return true;
    }
    return false;
  }
  
  bool txd_ok() {
    return !(reg(TXDATA) & TXD_FULL);
  }

  void put(int data) {
    while(!txd_ok());

    Reg8 data8 = data;
    reg(TXDATA) = (data8 & 0xFF);
  }

  bool try_put(int data) {
    if (txd_ok()) {
      put(data);
      return true;
    }
    return false;
  }
  
  int read(char * data, unsigned int max_size) {
    for(unsigned int i = 0; i < max_size; i++)
      data[i] = get();
    return 0;
  }
  
  int write(const char * data, unsigned int size) {
    for(unsigned int i = 0; i < size; i++)
        put(data[i]);
    return 0;
  }

    void flush() { while(!(reg(IP) & TXWM)); }
  bool ready_to_get() { return rxd_ok(); };
  bool ready_to_put() { return txd_ok(); };

  void int_enable(bool receive = true, bool transmit = true, bool time_out = true, bool overrun = true);
  void int_disable(bool receive = true, bool transmit = true, bool time_out = true, bool overrun = true);
  
};

__END_SYS

#endif

