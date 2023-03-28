// EPOS PC UART Mediator Test Program

#include <utility/ostream.h>
//#include <machine/riscv/riscv_uart.h>
//#include <machine/riscv/riscv_qspi.h>
#include <machine.h>

using namespace EPOS;
OStream cout;

int main()
{
    cout << "Teste QSPI \n" << endl;

    QSPI qspi(22729000, 0, 0, 4, 4);

    for(int i = 0; i < 256; i++) {
        qspi.put(i);
      int c = qspi.get();
      if(c != i)
        cout << " failed (" << c << ", should be " << i << ")!" << endl;
    }


    return 0;
}
