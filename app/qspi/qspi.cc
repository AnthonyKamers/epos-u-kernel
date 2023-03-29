// EPOS PC UART Mediator Test Program

#include <utility/ostream.h>
//#include <machine/riscv/riscv_uart.h>
#include <machine/riscv/riscv_qspi.h>
#include <machine.h>

using namespace EPOS;
OStream cout;
QSPI qspi;

int main()
{
    cout << "Teste QSPI \n" << endl;

    for (int i = 0; i < 2; i++) {
        bool ready = qspi.ready_to_put();

        int ie0 = qspi.check_ie();
        int ip0 = qspi.check_ip();

        cout << "ie: " << ie0 << endl;
        cout << "ip: " << ip0 << endl;

        if (ready) {
            cout << "ready to put: " << i << endl;
            qspi.put(i);

            int ie = qspi.check_ie();
            int ip = qspi.check_ip();

            cout << "ie: " << ie << endl;
            cout << "ip: " << ip << endl;

            bool ready_get = qspi.ready_to_get();
            cout << "ready to get: " << ready_get << endl;

            cout << "rxd_now: " << qspi.rxd_now << endl;

            bool get = qspi.get();
            cout << "get: " << get << endl;

            // after get
            ie = qspi.check_ie();
            ip = qspi.check_ip();

            cout << "ie after get: " << ie << endl;
            cout << "ip after get: " << ip << endl << endl;

//            qspi.flush();
        } else {
            cout << "not ready to put" << endl;
        }
    }

//    for (int i = 0; i < 5; i++) {
//        bool ready = qspi.ready_to_put();
//
//        if (ready) {
//            cout << "it is ready to put" << endl;
//            int value = 3;
//
//            qspi.put(value);
//
//            ready = qspi.ready_to_put();
//            cout << "it is ready to put: " << ready << endl;
//
//            bool get = qspi.ready_to_get();
//            cout << "ready to get: " << get << endl;
//
//            cout << "bla" << endl;
//
//            int value_get = qspi.get();
//
//            cout << "bla1" << endl;
//
//            cout << "value get: " << value_get << endl;
//        } else {
//            cout << "it is not ready to put" << endl;
//        }



//        cout << "QSPI ready to put" << i << ": " << ready << endl;
//    }

//    QSPI qspi(22729000, 0, 0, 4, 4);
//
//    for(int i = 0; i < 256; i++) {
//        qspi.put(i);
//      int c = qspi.get();
//      if(c != i)
//        cout << " failed (" << c << ", should be " << i << ")!" << endl;
//    }


    return 0;
}
