#include "PELib.h"
#include <cstdarg>
#include <functional>

using namespace std;

static PE pe;

void vdie(const string &src, int line, int column, const char *format, va_list arg) {
    if (line > 0)
        fprintf(stderr, "%s[%d,%d] ", src.c_str(), line, column);
    vfprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    exit(1);
}

void die(const string &src, int line, int column, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    die(src, line, column, format, arg);
    va_end(arg);
}

struct Symbol {
    string src;
    int line = 0, column = 0;
    Address addr;

    void die(const char *format, ...) {
        va_list arg;
        va_start(arg, format);
        vdie(src, line, column, format, arg);
        va_end(arg);
    }
};

bool operator!(const Symbol &sym) {
    return !*sym.addr.addr;
}

map<string, Symbol> funcs;

void link() {
    pe.link();
    for (auto p: funcs) {
        auto sym = p.second;
        if (!sym) sym.die("undefined: %s", p.first.c_str());
    }
}

enum Token { Word, Num, Str, Other };

class Lexer {
private:
    FILE *file;
    int cur = 0;
    string src;
    int curline = 1, curcol = 0;

public:
    Token type = Other;
    string token;
    int line = 1, column = 0;

    Lexer(const string &src): src(src) {
        file = fopen(src.c_str(), "r");
        readc();
    }

    ~Lexer() {
        if (file) fclose(file);
    }

    int readc() {
        if (!file) cur = -1;
        if (cur < 0) return cur;
        cur = fgetc(file);
        if (cur == '\n') { ++curline; curcol = 0; }
        curcol++;
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
        line = curline;
        column = curcol;
        type = t;
        token.clear();
        while (f(cur)) {
            token += cur;
            readc();
        }
        return true;
    }
};

Address func(const string &src, Lexer *lexer = NULL) {
    if (funcs.find(src) == funcs.end()) {
        Symbol sym;
        sym.src = src;
        sym.addr = pe.sym(src, true);
        if (lexer) {
            sym.line = lexer->line;
            sym.column = lexer->column;
        }
        funcs[src] = sym;
    }
    return funcs[src].addr;
}

string getstr(string s) {
    if (s.size() >= 2 && s[0] == '"' && s[1] == '"')
        s = s.substr(1, s.size() - 2);
    string ret;
    bool esc = false;
    for (size_t i = 0; i < s.size(); i++) {
        char ch = s[i];
        if (esc) {
            switch (ch) {
            case 'a': ret += '\a'; break;
            case 'b': ret += '\b'; break;
            case 'n': ret += '\n'; break;
            case 'f': ret += '\f'; break;
            case 't': ret += '\t'; break;
            case 'v': ret += '\v'; break;
            case '0': ret += '\0'; break;
            default : ret += ch  ; break;
            }
            esc = false;
        }
        else if (ch == '\\')
            esc = true;
        else
            ret += ch;
    }
    return ret;
}

void parse(const string &src) {
    Lexer lexer(src);
    while (lexer.read()) {
        printf("%s[%d,%d] %d:%s\n",
            src.c_str(), lexer.line, lexer.column,
            lexer.type, lexer.token.c_str());
    }
}

int main(int argc, char *argv[])
{
    pe.select();

    call(func("main"));
    push(eax);
    call(ptr[pe.import("msvcrt.dll", "exit")]);
    jmp(curtext->addr());

    for (int i = 1; i < argc; i++) {
        parse(argv[i]);
    }
    link();

    auto f = fopen("output.exe", "wb");
    if (!f) return 1;
    pe.write(f);
    fclose(f);
}
