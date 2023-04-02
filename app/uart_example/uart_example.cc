#include <utility/ostream.h>
#include <machine.h>

using namespace EPOS;
UART uart;
OStream cout;

int main()
{
    cout << "Teste UART\n" << endl;

    while (true) {
        // colocar como exemplo
        uart.put('a');

        // ler do teclado
        char c = uart.get();

        cout << "Lido: " << c << endl;
    }

    cout << " passed!" << endl;

    return 0;
}
