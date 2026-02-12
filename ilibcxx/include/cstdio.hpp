#pragma once

#include "instant/syscall.hpp"

namespace std {
    using size_t = instant::size_t;
    using ssize_t = instant::ssize_t;
    
    constexpr int STDIN_FILENO = 0;
    constexpr int STDOUT_FILENO = 1;
    constexpr int STDERR_FILENO = 2;
    
    int putchar(int c);
    int puts(const char* s);
    int printf(const char* fmt, ...);
    int sprintf(char* str, const char* fmt, ...);
    int snprintf(char* str, size_t size, const char* fmt, ...);
    
    double atof(const char* str);
    
    namespace io {
        class output_stream {
        private:
            int fd_;
            
        public:
            explicit output_stream(int fd = STDOUT_FILENO) : fd_(fd) {}
            
            output_stream& operator<<(const char* str);
            output_stream& operator<<(char c);
            output_stream& operator<<(int n);
            output_stream& operator<<(unsigned int n);
            output_stream& operator<<(long n);
            output_stream& operator<<(unsigned long n);
            output_stream& operator<<(long long n);
            output_stream& operator<<(unsigned long long n);
            output_stream& operator<<(double d);
            output_stream& operator<<(bool b);
            output_stream& operator<<(const void* ptr);
            
                    output_stream& operator<<(output_stream& (*manip)(output_stream&));
            
            void flush();
            void write(const char* data, size_t len);
        };
        
        class input_stream {
        private:
            int fd_;
            
        public:
            explicit input_stream(int fd = STDIN_FILENO) : fd_(fd) {}
            
            input_stream& operator>>(char& c);
            input_stream& operator>>(char* str);
            input_stream& operator>>(int& n);
            input_stream& operator>>(unsigned int& n);
            input_stream& operator>>(long& n);
            input_stream& operator>>(unsigned long& n);
            input_stream& operator>>(double& d);
            
            size_t read(char* buffer, size_t count);
            int getchar();
            char* getline(char* buffer, size_t size);
        };
        
            inline output_stream& endl(output_stream& os) {
            os << '\n';
            os.flush();
            return os;
        }
        
        inline output_stream& flush(output_stream& os) {
            os.flush();
            return os;
        }
        
            extern output_stream out;
        extern output_stream err;
        extern input_stream in;
    }
    
    namespace format {
        class formatter {
        private:
            char* buffer_;
            size_t capacity_;
            size_t position_;
            
        public:
            formatter(char* buf, size_t cap) 
                : buffer_(buf), capacity_(cap), position_(0) {}
            
            void put_char(char c);
            void put_string(const char* str);
            void put_number(long long num, int base = 10);
            void put_unsigned(unsigned long long num, int base = 10);
            void put_hex(unsigned long long num, bool uppercase = false);
            void put_pointer(const void* ptr);
            
            size_t size() const { return position_; }
            const char* data() const { return buffer_; }
        };
        
            int vsnprintf_impl(char* str, size_t size, const char* fmt, __builtin_va_list ap);
    }
    
    class FILE {
    private:
        int fd_;
        bool is_open_;
        
    public:
        FILE(int fd) : fd_(fd), is_open_(true) {}
        ~FILE() { if (is_open_) close(); }
        
        ssize_t write(const void* data, size_t size);
        ssize_t read(void* data, size_t size);
        void close();
        bool is_open() const { return is_open_; }
        int fileno() const { return fd_; }
    };
    
    extern FILE* stdin;
    extern FILE* stdout;
    extern FILE* stderr;
}

using std::putchar;
using std::puts;
using std::printf;
using std::sprintf;
using std::snprintf;