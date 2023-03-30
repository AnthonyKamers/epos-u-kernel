// EPOS PC QSPI Mediator Test Program

#include <utility/ostream.h>
#include <machine/riscv/riscv_qspi.h>
#include <machine.h>

using namespace EPOS;

OStream cout;
QSPI qspi;

void loop_test() {
    for (int i = 0; i < 3; i++) {
        bool ready = qspi.ready_to_put();

        int ie = qspi.check_ie();
        int ip = qspi.check_ip();

        cout << "ie before: " << ie << endl;
        cout << "ip before: " << ip << endl;

        if (ready) {
            cout << "ready to put value: " << i << endl;
            qspi.put(i);

            ie = qspi.check_ie();
            ip = qspi.check_ip();

            cout << "ie after put: " << ie << endl;
            cout << "ip after put: " << ip << endl;

            if (ip == 2)
                cout << "IP after put = 2 indicates it was written successfully to the FIFO. A interruption is set waiting to receive it." << endl;

            bool ready_get = qspi.ready_to_get();
            cout << "is it ready to get value from FIFO: " << ready_get << endl;

            int get = qspi.get();
            cout << "get: " << get << endl;

            // after get
            ie = qspi.check_ie();
            ip = qspi.check_ip();

            cout << "ie after get: " << ie << endl;
            cout << "ip after get: " << ip << endl;

            if (ip == 0)
                cout << "IP after get = 0 indicates something was written from the register RXData successfully, so it is reset to no interruptions (put/get)." << endl;

            cout << endl;
        } else
            cout << "not ready to put: ERROR" << endl;
    }
}

int main()
{
    cout << "QSPI general test \n" << endl;
    loop_test();

    return 0;
}
