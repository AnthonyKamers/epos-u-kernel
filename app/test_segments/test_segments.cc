// EPOS Segment Test Program

#include <memory.h>

using namespace EPOS;

const unsigned ES1_SIZE = 10000;
const unsigned ES2_SIZE = 100000;

int main()
{

//  Funciona
    OStream cout;

    cout << "Segment and Directory test" << endl;

    cout << "My address space's page directory is located at " << reinterpret_cast<void *>(MMU::current()) << "" << endl;
    size_t * a = reinterpret_cast<size_t *>(MMU::current());

    for(int i = 0; i < 8; i++)
    cout << "PD[" << i << "] = " << hex << a[i] << endl;
    Address_Space self(MMU::current());

    cout << "Creating two extra data segments:" << endl;
    Segment * es1 = new (SYSTEM) Segment(ES1_SIZE, Segment::Flags::SYS);
    Segment * es2 = new (SYSTEM) Segment(ES2_SIZE, Segment::Flags::SYS);
    cout << "  extra segment 1 ("<< es1->pt() << ") => " << ES1_SIZE << " bytes, done!" << endl;
    cout << "  extra segment 2 ("<< es2->pt() << ") => " << ES2_SIZE << " bytes, done!" << endl;

    cout << "Attaching segments:" << endl;
    CPU::Log_Addr * extra1 = self.attach(es1);
    CPU::Log_Addr * extra2 = self.attach(es2);
    cout << "  extra segment 1 => " << extra1 << " done!" << endl;
    cout << "  extra segment 2 => " << extra2 << " done!" << endl;

    
   cout << "Clearing segments:";
   memset(extra1, 0, ES1_SIZE);
   memset(extra2, 0, ES2_SIZE);
   cout << "  done!" << endl;

   cout << "Writing on segment 1:";
   *extra1 = 0xdeadbeef;
   cout << "  done!" << endl;
   cout << "Extra segment 1 address = " << hex << extra1 << endl;
   cout << "Extra segment 1 value = " << hex << *extra1 << endl;

   cout << "Detaching segments:";
   self.detach(es1);
   self.detach(es2);
   cout << "  done!" << endl;

   for(int i = 0; i < 8; i++)
       cout << "PD[" << i << "] = " << hex << a[i] << endl;

   cout << "Deleting segments:";
   delete es1;
   delete es2;
   cout << "  done!" << endl;

    cout << "I'm done, bye!" << endl;

    return 0;
}
