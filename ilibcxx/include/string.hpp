#pragma once

#include <instant/syscall.hpp>
#include <cstddef>
#include <cstdint>

namespace std {
    size_t strlen(const char* str);
    
    char* strcpy(char* dest, const char* src);
    
    int strcmp(const char* s1, const char* s2);
    
    void* memset(void* s, int c, size_t n);
    void* memcpy(void* dest, const void* src, size_t n);
    
    namespace string_ops {
        size_t length(const char* str);
        bool equal(const char* s1, const char* s2);
        
        template<typename T>
        void fill(T* ptr, const T& value, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                ptr[i] = value;
            }
        }
        
        template<typename T>
        void copy(T* dest, const T* src, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                dest[i] = src[i];
            }
        }
    }
}

namespace std {
    class string {
    private:
        char* data_;
        size_t size_;
        size_t capacity_;
        
        void ensure_capacity(size_t new_size);
        
    public:
        string();
        string(const char* cstr);
        string(const string& other);
        string(string&& other) noexcept;
        ~string();
        
        string& operator=(const string& other);
        string& operator=(string&& other) noexcept;
        string& operator=(const char* cstr);
        
        char& operator[](size_t index);
        const char& operator[](size_t index) const;
        
        string& operator+=(const string& other);
        string& operator+=(const char* cstr);
        string& operator+=(char c);
        
        friend string operator+(const string& lhs, const string& rhs);
        friend string operator+(const string& lhs, const char* rhs);
        friend string operator+(const char* lhs, const string& rhs);
        
        friend bool operator==(const string& lhs, const string& rhs);
        friend bool operator!=(const string& lhs, const string& rhs);
        friend bool operator<(const string& lhs, const string& rhs);
        
        const char* c_str() const;
        size_t size() const;
        size_t length() const;
        bool empty() const;
        
        void clear();
        void reserve(size_t new_capacity);
        void resize(size_t new_size, char fill_char = '\0');
        
        size_t find(const string& substr, size_t pos = 0) const;
        size_t find(const char* substr, size_t pos = 0) const;
        size_t find(char c, size_t pos = 0) const;
        
        string substr(size_t pos = 0, size_t len = SIZE_MAX) const;
    };
}