#pragma once

#include <cstdio.hpp>

namespace std {

class ostream {
public:
    ostream& operator<<(const char* str) {
        printf("%s", str);
        return *this;
    }
    
    ostream& operator<<(char c) {
        putchar(c);
        return *this;
    }
    
    ostream& operator<<(int n) {
        printf("%d", n);
        return *this;
    }
    
    ostream& operator<<(unsigned int n) {
        printf("%lu", (unsigned long)n);
        return *this;
    }
    
    ostream& operator<<(long n) {
        printf("%ld", n);
        return *this;
    }
    
    ostream& operator<<(unsigned long n) {
        printf("%lu", n);
        return *this;
    }
    
    ostream& operator<<(ostream& (*manip)(ostream&)) {
        return manip(*this);
    }
};

inline ostream& endl(ostream& os) {
    os << '\n';
    return os;
}

extern ostream cout;

}
