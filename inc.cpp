#include "PELib.h"

using namespace std;

int main()
{
    auto f = fopen("label.exe", "wb");
    if (!f) return 1;

    PE pe;
    pe.select();

/*
.intel_syntax
    mov ebx, 0
    jmp label2
label1:
    push ebx
    push "%d\n"
    call printf
    add esp, 8
label2:
    inc ebx
    cmp ebx, 10
    jnz label1
    push 0
    call exit
0:  jmp 0b
.data
format: .ascii "%d\10\0"
*/
    mov(ebx, 0);
    Address label2(0, Abs);
    jmp(label2);
    auto label1 = curtext->addr();
    push(ebx);
    push(pe.str("%d\n"));
    call(ptr[pe.import("msvcrt.dll", "printf")]);
    add(esp, 8);
    curtext->put(label2);
    inc(ebx);
    cmp(ebx, 10);
    jnz(label1);
    push(0);
    call(ptr[pe.import("msvcrt.dll", "exit")]);
    jmp(curtext->addr());

    pe.write(f);
    fclose(f);
}
