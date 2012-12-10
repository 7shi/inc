#include "PELib.h"
#include <functional>

using namespace std;

static PE pe;

enum Token { Word, Num, Str, Other };

class Lexer {
private:
    FILE *file;
    int cur;

public:
    Token type;
    string token;

    Lexer(FILE *file): file(file) {
        cur = fgetc(file);
    }

    int readc() {
        if (cur < 0) return cur;
        cur = fgetc(file);
        return cur;
    }

    static bool isalpha(char ch) {
        return ch == '_'
            || ('A' <= ch && ch <= 'Z')
            || ('a' <= ch && ch <= 'z');
    }

    static bool isnum(char ch) {
        return '0' <= ch && ch <= '9';
    }

    static bool isletter(char ch) {
        return ch == '\'' || isalpha(ch) || isnum(ch);
    }

    bool read() {
        while (cur >= 0) {
            if (isalpha(cur))
                return read(Word, isletter);
            else if (isnum(cur))
                return read(Num, isnum);
            else if (cur == '"') {
                int dq = 0;
                bool esc = false;
                return read(Str, [&](int ch) {
                    if (dq == 2)
                        return false;
                    else if (esc)
                        esc = false;
                    else if (ch == '"')
                        dq++;
                    else if (ch == '\\')
                        esc = true;
                    return true;
                });
            }
            else if (cur > ' ') {
                int c = 0;
                return read(Other, [&](int) { return c++ < 1; });
            }
            readc();
        }
        return false;
    }

private:
    bool read(Token t, function<bool(int)> f) {
        type = t;
        token.clear();
        while (f(cur)) {
            token += cur;
            readc();
        }
        return true;
    }
};

void parse(const string &src) {
    FILE *f = fopen(src.c_str(), "r");
    if (!f) return;
    Lexer lexer(f);
    while (lexer.read()) {
        printf("%d:%s\n", lexer.type, lexer.token.c_str());
    }
    fclose(f);
}

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

    for (int i = 1; i < argc; i++) {
        parse(argv[i]);
    }

    auto f = fopen("output.exe", "wb");
    if (!f) return 1;
    pe.write(f);
    fclose(f);
}
