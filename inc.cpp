#include "PELib.h"

using namespace std;

static PE pe;

int main(int argc, char *argv[])
{
    pe.select();

    auto _main = pe.sym("main", true);
    call(_main);
    push(eax);
    call(ptr[pe.import("msvcrt.dll", "exit")]);
    jmp(curtext->addr());
    
    curtext->put(_main);
    ret();

    auto f = fopen("output.exe", "wb");
    if (!f) return 1;
    pe.write(f);
    fclose(f);
}
