#include <string.hpp>
#include <cstdlib.hpp>
#include <cstddef>

extern "C" int memcmp(const void* s1, const void* s2, size_t n);

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = static_cast<const unsigned char*>(s1);
    const unsigned char* p2 = static_cast<const unsigned char*>(s2);
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

namespace std {
    size_t strlen(const char* str) {
        size_t len = 0;
        while (str[len]) len++;
        return len;
    }
    
    char* strcpy(char* dest, const char* src) {
        char* d = dest;
        while ((*d++ = *src++));
        return dest;
    }
    
    int strcmp(const char* s1, const char* s2) {
        while (*s1 && (*s1 == *s2)) {
            s1++;
            s2++;
        }
        return *(unsigned char*)s1 - *(unsigned char*)s2;
    }
    
    void* memset(void* s, int c, size_t n) {
        unsigned char* p = static_cast<unsigned char*>(s);
        while (n--) *p++ = static_cast<unsigned char>(c);
        return s;
    }
    
    void* memcpy(void* dest, const void* src, size_t n) {
        unsigned char* d = static_cast<unsigned char*>(dest);
        const unsigned char* s = static_cast<const unsigned char*>(src);
        while (n--) *d++ = *s++;
        return dest;
    }
    
    namespace string_ops {
        size_t length(const char* str) {
            return strlen(str);
        }
        
        bool equal(const char* s1, const char* s2) {
            return strcmp(s1, s2) == 0;
        }
    }
    
    void string::ensure_capacity(size_t new_size) {
        if (new_size > capacity_) {
            size_t new_capacity = capacity_ == 0 ? 16 : capacity_;
            while (new_capacity < new_size) {
                new_capacity *= 2;
            }
            
            char* new_data = static_cast<char*>(malloc(new_capacity + 1));
            if (data_) {
                memcpy(new_data, data_, size_);
                free(data_);
            }
            data_ = new_data;
            capacity_ = new_capacity;
        }
    }
    
    string::string() : data_(nullptr), size_(0), capacity_(0) {}
    
    string::string(const char* cstr) : data_(nullptr), size_(0), capacity_(0) {
        if (cstr) {
            size_ = strlen(cstr);
            ensure_capacity(size_);
            memcpy(data_, cstr, size_);
            data_[size_] = '\0';
        }
    }
    
    string::string(const string& other) : data_(nullptr), size_(0), capacity_(0) {
        if (other.data_) {
            size_ = other.size_;
            ensure_capacity(size_);
            memcpy(data_, other.data_, size_);
            data_[size_] = '\0';
        }
    }
    
    string::string(string&& other) noexcept 
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    string::~string() {
        if (data_) {
            free(data_);
        }
    }
    
    string& string::operator=(const string& other) {
        if (this != &other) {
            size_ = other.size_;
            ensure_capacity(size_);
            if (other.data_) {
                memcpy(data_, other.data_, size_);
                data_[size_] = '\0';
            }
        }
        return *this;
    }
    
    string& string::operator=(string&& other) noexcept {
        if (this != &other) {
            if (data_) free(data_);
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }
    
    string& string::operator=(const char* cstr) {
        if (cstr) {
            size_ = strlen(cstr);
            ensure_capacity(size_);
            memcpy(data_, cstr, size_);
            data_[size_] = '\0';
        } else {
            clear();
        }
        return *this;
    }
    
    char& string::operator[](size_t index) {
        return data_[index];
    }
    
    const char& string::operator[](size_t index) const {
        return data_[index];
    }
    
    string& string::operator+=(const string& other) {
        if (other.size_ > 0) {
            ensure_capacity(size_ + other.size_);
            memcpy(data_ + size_, other.data_, other.size_);
            size_ += other.size_;
            data_[size_] = '\0';
        }
        return *this;
    }
    
    string& string::operator+=(const char* cstr) {
        if (cstr) {
            size_t len = strlen(cstr);
            if (len > 0) {
                ensure_capacity(size_ + len);
                memcpy(data_ + size_, cstr, len);
                size_ += len;
                data_[size_] = '\0';
            }
        }
        return *this;
    }
    
    string& string::operator+=(char c) {
        ensure_capacity(size_ + 1);
        data_[size_] = c;
        data_[++size_] = '\0';
        return *this;
    }
    
    string operator+(const string& lhs, const string& rhs) {
        string result(lhs);
        result += rhs;
        return result;
    }
    
    string operator+(const string& lhs, const char* rhs) {
        string result(lhs);
        result += rhs;
        return result;
    }
    
    string operator+(const char* lhs, const string& rhs) {
        string result(lhs);
        result += rhs;
        return result;
    }
    
    bool operator==(const string& lhs, const string& rhs) {
        if (lhs.size_ != rhs.size_) return false;
        if (lhs.size_ == 0) return true;
        return memcmp(lhs.data_, rhs.data_, lhs.size_) == 0;
    }
    
    bool operator!=(const string& lhs, const string& rhs) {
        return !(lhs == rhs);
    }
    
    bool operator<(const string& lhs, const string& rhs) {
        size_t min_len = lhs.size_ < rhs.size_ ? lhs.size_ : rhs.size_;
        if (min_len > 0) {
            int cmp = memcmp(lhs.data_, rhs.data_, min_len);
            if (cmp != 0) return cmp < 0;
        }
        return lhs.size_ < rhs.size_;
    }
    
    const char* string::c_str() const {
        return data_ ? data_ : "";
    }
    
    size_t string::size() const {
        return size_;
    }
    
    size_t string::length() const {
        return size_;
    }
    
    bool string::empty() const {
        return size_ == 0;
    }
    
    void string::clear() {
        size_ = 0;
        if (data_) {
            data_[0] = '\0';
        }
    }
    
    void string::reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            ensure_capacity(new_capacity);
        }
    }
    
    void string::resize(size_t new_size, char fill_char) {
        if (new_size > size_) {
            ensure_capacity(new_size);
            for (size_t i = size_; i < new_size; ++i) {
                data_[i] = fill_char;
            }
        }
        size_ = new_size;
        if (data_) {
            data_[size_] = '\0';
        }
    }
    
    size_t string::find(const string& substr, size_t pos) const {
        return find(substr.c_str(), pos);
    }
    
    size_t string::find(const char* substr, size_t pos) const {
        if (!substr || !data_ || pos >= size_) return SIZE_MAX;
            
        size_t substr_len = strlen(substr);
        if (substr_len == 0) return pos;
        if (pos + substr_len > size_) return SIZE_MAX;
            
        for (size_t i = pos; i <= size_ - substr_len; ++i) {
            if (memcmp(data_ + i, substr, substr_len) == 0) {
                return i;
            }
        }
        return SIZE_MAX;
    }
        
    size_t string::find(char c, size_t pos) const {
        if (!data_ || pos >= size_) return SIZE_MAX;
            
        for (size_t i = pos; i < size_; ++i) {
            if (data_[i] == c) {
                return i;
            }
        }
        return SIZE_MAX;
    }
        
    string string::substr(size_t pos, size_t len) const {
        if (!data_ || pos >= size_) return string();
            
        size_t actual_len = len;
        if (pos + len > size_ || len == SIZE_MAX) {
            actual_len = size_ - pos;
        }
        
        string result;
        result.size_ = actual_len;
        result.ensure_capacity(actual_len);
        memcpy(result.data_, data_ + pos, actual_len);
        result.data_[actual_len] = '\0';
        
        return result;
    }
}