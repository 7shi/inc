#include "PELib.h"
#include <cstdarg>
#include <functional>

using namespace std;

static PE pe;

void vdie(const string &src, int line, int column, const char *format, va_list arg) {
    if (line > 0)
        fprintf(stderr, "%s[%d:%d] ", src.c_str(), line, column);
    vfprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    exit(1);
}

void die(const string &src, int line, int column, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vdie(src, line, column, format, arg);
    va_end(arg);
}

struct Symbol {
    string src;
    int line = 0, column = 0;
    Address addr;

    void clear() {
        *addr.addr = 0;
    }

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

Address func(const string &name, const string &src = "", int line = 0, int column = 0) {
    if (funcs.find(name) == funcs.end()) {
        Symbol sym;
        sym.src = src;
        sym.addr = pe.sym(name, true);
        sym.line = line;
        sym.column = column;
        funcs[name] = sym;
    }
    return funcs[name].addr;
}

void link() {
    for (auto p: funcs) p.second.clear();
    pe.link();
    for (auto p: funcs) {
        auto name = p.first.c_str();
        auto sym = p.second;
        if (!sym) sym.die("undefined: %s", name);
        printf("%x: %s\n", *sym.addr.addr, name);
    }
}

enum Token { Word, Num, Str, Other };

class Lexer {
private:
    FILE *file;
    int cur = 0;
    int curline = 1, curcol = 0;

public:
    string src;
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
        ++curcol;
        if (cur == '\n') { ++curline; curcol = 0; }
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
                        ++dq;
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

string getstr(string s) {
    if (s.size() >= 2 && s[0] == '"' && s[1] == '"')
        s = s.substr(1, s.size() - 2);
    string ret;
    bool esc = false;
    for (int i = 0; i < s.size(); ++i) {
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

template <typename T> int index(const vector<T> &vec, const T &v) {
    for (int i = 0; i < vec.size(); ++i)
        if (vec[i] == v) return i;
    return -1;
}

class Parser {
private:
    Lexer lexer;
    Token type;
    string token;

public:
    Parser(const string &src): lexer(src) {}

    void parse() {
        while (read()) {
            if (token == "function")
                parseFunction();
            else
                die("error: %s", token.c_str());
        }
    }

    void die(const char *format, ...) {
        va_list arg;
        va_start(arg, format);
        vdie(lexer.src, lexer.line, lexer.column, format, arg);
        va_end(arg);
    }

private:
    bool read() {
        if (!lexer.read()) return false;
        type = lexer.type;
        token = lexer.token;
        return true;
    }

    void parseFunction(const string &prefix = "") {
        if (!read() || type != Word)
            die("function: name required");
        curtext->put(func(token));
        push(ebp);
        mov(ebp, esp);
        auto args = parseFunctionArgs();
        bool epi = false;
        while (read()) {
            if (token == "end") {
                if (read() && token == "function") {
                    if (!epi) {
                        leave();
                        ret();
                    }
                    return;
                }
                die("end: 'function' required");
            }
            else if (type == Word) {
                epi = false;
                int l = lexer.line, c = lexer.column;
                auto t = token;
                if (t == "return") {
                    if (read() && type == Num) {
                        mov(eax, atoi(token.c_str()));
                        leave();
                        ret();
                        epi = true;
                        continue;
                    }
                } else if (read()) {
                    if (token == "(") {
                        int nargs = parseCallArgs(args);
                        call(func(t, lexer.src, l, c));
                        if (nargs > 0) add(esp, 4 * nargs);
                        continue;
                    }
                }
                ::die(lexer.src, l, c, "error: %s", t.c_str());
            }
            else
                die("error: %s", token.c_str());
        }
        die("function: 'end function' required");
    }

    vector<string> parseFunctionArgs() {
        if (!read() || token != "(")
            die("function: '(' required");
        vector<string> args;
        while (read()) {
            if (token == ")")
                break;
            else if (type == Word) {
                args.push_back(token);
                if (read()) {
                    if (token == ")")
                        break;
                    else if (token == ",")
                        continue;
                }
                die("function: ',' or ')' required");
            }
            else
                die("function: argument required");
        }
        return args;
    }

    int parseCallArgs(const vector<string> &args) {
        int ret = 0;
        while (read()) {
            if (token == ")")
                break;
            else if (type == Word) {
                int arg = index(args, token);
                if (arg == -1)
                    die("undefined variable: %s", token.c_str());
                // TODO: push(ptr[ebp+X]);
                mov(eax, ebp);
                add(eax, (arg + 1) * 4);
                push(ptr[eax]);
                ++ret;
            }
            else if (type == Num) {
                push(atoi(token.c_str()));
                ++ret;
            }
            else if (type == Str) {
                push(pe.str(getstr(token)));
                ++ret;
            }
            else
                die("function: argument required");
            if (read()) {
                if (token == ")")
                    break;
                else if (token == ",")
                    continue;
            }
            die("function: ',' or ')' required");
        }
    }
};

int main(int argc, char *argv[])
{
    pe.select();

    curtext->put(func("_start"));
    call(func("main"));
    push(eax);
    call(ptr[pe.import("msvcrt.dll", "exit")]);
    jmp(curtext->addr());

    for (int i = 1; i < argc; ++i)
        Parser(argv[i]).parse();
    link();

    auto exe = "output.exe";
    auto f = fopen(exe, "wb");
    if (!f) die("", 0, 0, "can not open: %s", exe);
    pe.write(f);
    fclose(f);
}
