#include <utility/ostream.h>
#include <architecture/rv64/rv64_mmu.h>

using namespace EPOS;

OStream cout;

// Architecture Imports
typedef CPU::Reg Reg;
typedef CPU::Phy_Addr Phy_Addr;
typedef CPU::Log_Addr Log_Addr;
typedef MMU::Page Page;
typedef MMU::Page_Flags Flags;
typedef MMU::Page_Table Page_Table;
typedef MMU::Page_Directory Page_Directory;
typedef MMU::PT_Entry PT_Entry;
typedef MMU::PD_Entry PD_Entry;
typedef MMU::Chunk Chunk;
typedef MMU::Directory Directory;

int main()
{
    MMU mmu;

    cout << "Memory map test: " << endl;
    // Aloca o espaço de memória fisica usando as funções desenvolvidas pela MMU
    Phy_Addr phy_addr = mmu.calloc(1);
    cout << "Endereço físico alocado: " << phy_addr << endl;
    // Endereço virtual
    Log_Addr log_addr = mmu.phy2log(phy_addr);
    cout << "Endereço virtual da página: " << log_addr << endl;

    // Escreve alguns dados nesse endereço virtual
    // Array de inteiros
    int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    memcpy(log_addr, data, 10 * sizeof(int));

    // Verifica se os dados escritos represetam corretamente aquilo que era esperado
    int readable_data[sizeof(data) / sizeof(int)];
    memcpy(readable_data, log_addr, 10 * sizeof(int));

    cout << "Dados lidos: " << endl;
    for (int i = 0; i < 10; i++)
        cout << readable_data[i] << " ";
    cout << endl;
    return 0;
}