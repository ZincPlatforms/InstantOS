#include "cstdio.hpp"
#include "string.hpp"
#include "unistd.hpp"
#include "cstdlib.hpp"
#include <stdarg.h>

double atof(const char* str) {
    if (!str) return 0.0;
    
    double result = 0.0;
    double sign = 1.0;
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == '-') {
        sign = -1.0;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0 + (*str - '0');
        str++;
    }
    
    if (*str == '.') {
        str++;
        double frac = 0.1;
        while (*str >= '0' && *str <= '9') {
            result += (*str - '0') * frac;
            frac *= 0.1;
            str++;
        }
    }
    
    return sign * result;
}

namespace std {
    int putchar(int c) {
        char ch = static_cast<char>(c);
        return write(STDOUT_FILENO, &ch, 1) == 1 ? c : -1;
    }
    
    int puts(const char* s) {
        if (!s) return -1;
        size_t len = strlen(s);
        if (write(STDOUT_FILENO, s, len) != static_cast<ssize_t>(len)) return -1;
        if (putchar('\n') == -1) return -1;
        return 0;
    }
    
    static void print_num(long long num, int base, bool uppercase = false) {
        char buf[32];
        int i = 0;
        bool neg = false;
        unsigned long long unum;
        
        if (num == 0) {
            putchar('0');
            return;
        }
        
        if (num < 0 && base == 10) {
            neg = true;
            unum = static_cast<unsigned long long>(-num);
        } else {
            unum = static_cast<unsigned long long>(num);
        }
        
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        
        while (unum > 0) {
            buf[i++] = digits[unum % base];
            unum /= base;
        }
        
        if (neg) putchar('-');
        
        while (i > 0) {
            putchar(buf[--i]);
        }
    }
    
    int printf(const char* fmt, ...) {
        if (!fmt) return -1;
        
        va_list ap;
        va_start(ap, fmt);
        int count = 0;
        
        while (*fmt) {
            if (*fmt == '%') {
                fmt++;
                
                bool is_long = false;
                bool is_long_long = false;
                
                if (*fmt == 'l') {
                    is_long = true;
                    fmt++;
                    if (*fmt == 'l') {
                        is_long_long = true;
                        fmt++;
                    }
                }
                
                switch (*fmt) {
                    case 'd':
                    case 'i': {
                        long long v;
                        if (is_long_long) {
                            v = va_arg(ap, long long);
                        } else if (is_long) {
                            v = va_arg(ap, long);
                        } else {
                            v = va_arg(ap, int);
                        }
                        print_num(v, 10);
                        break;
                    }
                    
                    case 'u': {
                        unsigned long long v;
                        if (is_long_long) {
                            v = va_arg(ap, unsigned long long);
                        } else if (is_long) {
                            v = va_arg(ap, unsigned long);
                        } else {
                            v = va_arg(ap, unsigned int);
                        }
                        print_num(static_cast<long long>(v), 10);
                        break;
                    }
                    
                    case 'x': {
                        unsigned long long v;
                        if (is_long_long) {
                            v = va_arg(ap, unsigned long long);
                        } else if (is_long) {
                            v = va_arg(ap, unsigned long);
                        } else {
                            v = va_arg(ap, unsigned int);
                        }
                        print_num(static_cast<long long>(v), 16, false);
                        break;
                    }
                    
                    case 'X': {
                        unsigned long long v;
                        if (is_long_long) {
                            v = va_arg(ap, unsigned long long);
                        } else if (is_long) {
                            v = va_arg(ap, unsigned long);
                        } else {
                            v = va_arg(ap, unsigned int);
                        }
                        print_num(static_cast<long long>(v), 16, true);
                        break;
                    }
                    
                    case 'p': {
                        void* ptr = va_arg(ap, void*);
                        putchar('0');
                        putchar('x');
                        print_num(reinterpret_cast<long long>(ptr), 16, false);
                        break;
                    }
                    
                    case 's': {
                        const char* s = va_arg(ap, const char*);
                        if (!s) s = "(null)";
                        while (*s) {
                            putchar(*s++);
                            count++;
                        }
                        break;
                    }
                    
                    case 'c': {
                        int c = va_arg(ap, int);
                        putchar(c);
                        count++;
                        break;
                    }
                    
                    case '%':
                        putchar('%');
                        count++;
                        break;
                    
                    default:
                        putchar('%');
                        if (is_long_long) {
                            putchar('l');
                            putchar('l');
                        } else if (is_long) {
                            putchar('l');
                        }
                        putchar(*fmt);
                        count += is_long_long ? 4 : (is_long ? 3 : 2);
                        break;
                }
            } else {
                putchar(*fmt);
                count++;
            }
            fmt++;
        }
        
        va_end(ap);
        return count;
    }
    
    int sprintf(char* str, const char* fmt, ...) {
        return 0;
    }
    
    int snprintf(char* str, size_t size, const char* fmt, ...) {
        return 0;
    }
    
    namespace io {
        output_stream& output_stream::operator<<(const char* str) {
            if (str) {
                size_t len = strlen(str);
                write(str, len);
            }
            return *this;
        }
        
        output_stream& output_stream::operator<<(char c) {
            ::std::write(fd_, &c, 1);
            return *this;
        }
        
        output_stream& output_stream::operator<<(int n) {
            char buf[32];
            int len = 0;
            bool neg = n < 0;
            unsigned int un = neg ? -n : n;
            
            if (un == 0) {
                buf[len++] = '0';
            } else {
                while (un > 0) {
                    buf[len++] = '0' + (un % 10);
                    un /= 10;
                }
            }
            
            if (neg) buf[len++] = '-';
            
            for (int i = 0; i < len / 2; ++i) {
                char temp = buf[i];
                buf[i] = buf[len - 1 - i];
                buf[len - 1 - i] = temp;
            }
            
            write(buf, len);
            return *this;
        }
        
        output_stream& output_stream::operator<<(unsigned int n) {
            char buf[32];
            int len = 0;
            
            if (n == 0) {
                buf[len++] = '0';
            } else {
                while (n > 0) {
                    buf[len++] = '0' + (n % 10);
                    n /= 10;
                }
            }
            
            for (int i = 0; i < len / 2; ++i) {
                char temp = buf[i];
                buf[i] = buf[len - 1 - i];
                buf[len - 1 - i] = temp;
            }
            
            write(buf, len);
            return *this;
        }
        
        output_stream& output_stream::operator<<(long n) {
            return *this << static_cast<long long>(n);
        }
        
        output_stream& output_stream::operator<<(unsigned long n) {
            return *this << static_cast<unsigned long long>(n);
        }
        
        output_stream& output_stream::operator<<(long long n) {
            char buf[64];
            int len = 0;
            bool neg = n < 0;
            unsigned long long un = neg ? -n : n;
            
            if (un == 0) {
                buf[len++] = '0';
            } else {
                while (un > 0) {
                    buf[len++] = '0' + (un % 10);
                    un /= 10;
                }
            }
            
            if (neg) buf[len++] = '-';
            
            for (int i = 0; i < len / 2; ++i) {
                char temp = buf[i];
                buf[i] = buf[len - 1 - i];
                buf[len - 1 - i] = temp;
            }
            
            write(buf, len);
            return *this;
        }
        
        output_stream& output_stream::operator<<(unsigned long long n) {
            char buf[64];
            int len = 0;
            
            if (n == 0) {
                buf[len++] = '0';
            } else {
                while (n > 0) {
                    buf[len++] = '0' + (n % 10);
                    n /= 10;
                }
            }
            
            for (int i = 0; i < len / 2; ++i) {
                char temp = buf[i];
                buf[i] = buf[len - 1 - i];
                buf[len - 1 - i] = temp;
            }
            
            write(buf, len);
            return *this;
        }
        
        output_stream& output_stream::operator<<(double d) {
            return *this << static_cast<long long>(d);
        }
        
        output_stream& output_stream::operator<<(bool b) {
            return *this << (b ? "true" : "false");
        }
        
        output_stream& output_stream::operator<<(const void* ptr) {
            *this << "0x";
            unsigned long long addr = reinterpret_cast<unsigned long long>(ptr);
            
            char buf[32];
            int len = 0;
            const char* hex = "0123456789abcdef";
            
            if (addr == 0) {
                buf[len++] = '0';
            } else {
                while (addr > 0) {
                    buf[len++] = hex[addr % 16];
                    addr /= 16;
                }
            }
            
            for (int i = 0; i < len / 2; ++i) {
                char temp = buf[i];
                buf[i] = buf[len - 1 - i];
                buf[len - 1 - i] = temp;
            }
            
            write(buf, len);
            return *this;
        }
        
        output_stream& output_stream::operator<<(output_stream& (*manip)(output_stream&)) {
            return manip(*this);
        }
        
        void output_stream::flush() {
        }
        
        void output_stream::write(const char* data, size_t len) {
            ::std::write(fd_, data, len);
        }
        
        input_stream& input_stream::operator>>(char& c) {
            ::std::read(fd_, &c, 1);
            return *this;
        }
        
        input_stream& input_stream::operator>>(char* str) {
            size_t i = 0;
            char c;
            while (::std::read(fd_, &c, 1) == 1 && c != ' ' && c != '\n' && c != '\t') {
                str[i++] = c;
            }
            str[i] = '\0';
            return *this;
        }
        
        input_stream& input_stream::operator>>(int& n) {
            char buf[32];
            *this >> buf;
            n = atoi(buf);
            return *this;
        }
        
        input_stream& input_stream::operator>>(unsigned int& n) {
            char buf[32];
            *this >> buf;
            n = static_cast<unsigned int>(atol(buf));
            return *this;
        }
        
        input_stream& input_stream::operator>>(long& n) {
            char buf[32];
            *this >> buf;
            n = atol(buf);
            return *this;
        }
        
        input_stream& input_stream::operator>>(unsigned long& n) {
            char buf[32];
            *this >> buf;
            n = static_cast<unsigned long>(atol(buf));
            return *this;
        }
        
        input_stream& input_stream::operator>>(double& d) {
            char buf[32];
            *this >> buf;
            d = atof(buf);
            return *this;
        }
        
        size_t input_stream::read(char* buffer, size_t count) {
            ssize_t result = ::std::read(fd_, buffer, count);
            return result > 0 ? static_cast<size_t>(result) : 0;
        }
        
        int input_stream::getchar() {
            char c;
            return ::std::read(fd_, &c, 1) == 1 ? c : -1;
        }
        
        char* input_stream::getline(char* buffer, size_t size) {
            if (!buffer || size == 0) return nullptr;
            
            size_t i = 0;
            char c;
            while (i < size - 1 && ::std::read(fd_, &c, 1) == 1) {
                buffer[i++] = c;
                if (c == '\n') break;
            }
            buffer[i] = '\0';
            return i > 0 ? buffer : nullptr;
        }
        
        output_stream out(STDOUT_FILENO);
        output_stream err(STDERR_FILENO);
        input_stream in(STDIN_FILENO);
    }
    
    namespace format {
        void formatter::put_char(char c) {
            if (position_ < capacity_) {
                buffer_[position_++] = c;
            }
        }
        
        void formatter::put_string(const char* str) {
            if (!str) return;
            while (*str && position_ < capacity_) {
                buffer_[position_++] = *str++;
            }
        }
        
        void formatter::put_number(long long num, int base) {
            char buf[32];
            int i = 0;
            bool neg = num < 0;
            unsigned long long unum = neg ? -num : num;
            
            if (unum == 0) {
                put_char('0');
                return;
            }
            
            while (unum > 0) {
                int digit = unum % base;
                buf[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
                unum /= base;
            }
            
            if (neg) put_char('-');
            
            while (i > 0) {
                put_char(buf[--i]);
            }
        }
        
        void formatter::put_unsigned(unsigned long long num, int base) {
            char buf[32];
            int i = 0;
            
            if (num == 0) {
                put_char('0');
                return;
            }
            
            while (num > 0) {
                int digit = num % base;
                buf[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
                num /= base;
            }
            
            while (i > 0) {
                put_char(buf[--i]);
            }
        }
        
        void formatter::put_hex(unsigned long long num, bool uppercase) {
            const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
            char buf[32];
            int i = 0;
            
            if (num == 0) {
                put_char('0');
                return;
            }
            
            while (num > 0) {
                buf[i++] = digits[num % 16];
                num /= 16;
            }
            
            while (i > 0) {
                put_char(buf[--i]);
            }
        }
        
        void formatter::put_pointer(const void* ptr) {
            put_string("0x");
            put_hex(reinterpret_cast<unsigned long long>(ptr), false);
        }
    }
    
    ssize_t FILE::write(const void* data, size_t size) {
        if (!is_open_) return -1;
        return ::std::write(fd_, data, size);
    }
    
    ssize_t FILE::read(void* data, size_t size) {
        if (!is_open_) return -1;
        return ::std::read(fd_, data, size);
    }
    
    void FILE::close() {
        if (fd_ >= 0 && fd_ <= 2) return;
        is_open_ = false;
    }
    
    static FILE stdin_file(STDIN_FILENO);
    static FILE stdout_file(STDOUT_FILENO);
    static FILE stderr_file(STDERR_FILENO);
    
    FILE* stdin = &stdin_file;
    FILE* stdout = &stdout_file;
    FILE* stderr = &stderr_file;
}