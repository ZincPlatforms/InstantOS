#pragma once

#include "instant/syscall.hpp"
#include "string.hpp"

namespace std {
    using size_t = instant::size_t;
    
    constexpr int EXIT_SUCCESS = 0;
    constexpr int EXIT_FAILURE = 1;
    
    [[noreturn]] void exit(int status);
    [[noreturn]] void abort();
    void atexit(void (*func)());
    
    void* malloc(size_t size);
    void* calloc(size_t nmemb, size_t size);
    void* realloc(void* ptr, size_t size);
    void free(void* ptr);
    
    namespace memory {
        class allocator {
        public:
            static void* allocate(size_t size);
            static void* allocate_aligned(size_t size, size_t alignment);
            static void deallocate(void* ptr);
            static void* reallocate(void* ptr, size_t new_size);
            
            template<typename T>
            static T* allocate_array(size_t count) {
                return static_cast<T*>(allocate(sizeof(T) * count));
            }
            
            template<typename T>
            static void deallocate_array(T* ptr) {
                deallocate(static_cast<void*>(ptr));
            }
        };
        
            template<typename T>
        class unique_ptr {
        private:
            T* ptr_;
            
        public:
            unique_ptr() : ptr_(nullptr) {}
            explicit unique_ptr(T* ptr) : ptr_(ptr) {}
            
            ~unique_ptr() {
                if (ptr_) {
                    delete ptr_;
                }
            }
            
                    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) {
                other.ptr_ = nullptr;
            }
            
                    unique_ptr& operator=(unique_ptr&& other) noexcept {
                if (this != &other) {
                    if (ptr_) delete ptr_;
                    ptr_ = other.ptr_;
                    other.ptr_ = nullptr;
                }
                return *this;
            }
            
                    unique_ptr(const unique_ptr&) = delete;
            unique_ptr& operator=(const unique_ptr&) = delete;
            
            T* get() const { return ptr_; }
            T& operator*() const { return *ptr_; }
            T* operator->() const { return ptr_; }
            
            explicit operator bool() const { return ptr_ != nullptr; }
            
            T* release() {
                T* temp = ptr_;
                ptr_ = nullptr;
                return temp;
            }
            
            void reset(T* new_ptr = nullptr) {
                if (ptr_) delete ptr_;
                ptr_ = new_ptr;
            }
        };
        
        template<typename T, typename... Args>
        unique_ptr<T> make_unique(Args&&... args) {
            return unique_ptr<T>(new T(args...));
        }
    }
    
    int atoi(const char* str);
    long atol(const char* str);
    long long atoll(const char* str);
    
    double atof(const char* str);
    double strtod(const char* str, char** endptr);
    long strtol(const char* str, char** endptr, int base);
    unsigned long strtoul(const char* str, char** endptr, int base);
    
    namespace convert {
        template<typename T>
        struct from_string;
        
        template<>
        struct from_string<int> {
            static int parse(const char* str) { return atoi(str); }
        };
        
        template<>
        struct from_string<long> {
            static long parse(const char* str) { return atol(str); }
        };
        
        template<>
        struct from_string<long long> {
            static long long parse(const char* str) { return atoll(str); }
        };
        
        template<>
        struct from_string<double> {
            static double parse(const char* str) { return atof(str); }
        };
        
        template<typename T>
        T parse(const char* str) {
            return from_string<T>::parse(str);
        }
        
            template<typename T>
        bool try_parse(const char* str, T& result) {
            try {
                result = parse<T>(str);
                return true;
            } catch (...) {
                return false;
            }
        }
    }
    
    int abs(int n);
    long labs(long n);
    long long llabs(long long n);
    
    template<typename T>
    constexpr T abs(T n) {
        return n < 0 ? -n : n;
    }
    
    namespace random {
        class simple_rng {
        private:
            unsigned long seed_;
            
        public:
            explicit simple_rng(unsigned long seed = 1) : seed_(seed) {}
            
            void set_seed(unsigned long seed) { seed_ = seed; }
            
            unsigned long next() {
                seed_ = seed_ * 1103515245 + 12345;
                return seed_;
            }
            
            int rand() {
                return static_cast<int>(next() % 32768);
            }
            
            int rand(int max) {
                return rand() % max;
            }
            
            int rand(int min, int max) {
                return min + (rand() % (max - min + 1));
            }
            
            double rand_double() {
                return static_cast<double>(rand()) / 32767.0;
            }
        };
        
        extern simple_rng default_rng;
        
        inline int rand() { return default_rng.rand(); }
        inline int rand(int max) { return default_rng.rand(max); }
        inline int rand(int min, int max) { return default_rng.rand(min, max); }
        inline void srand(unsigned long seed) { default_rng.set_seed(seed); }
    }
    
    namespace system {
        char* getenv(const char* name);
        int setenv(const char* name, const char* value, int overwrite);
        int unsetenv(const char* name);
        
        int system(const char* command);
        
        void quick_exit(int status);
        void _Exit(int status);
    }
    
    namespace utility {
        template<typename T>
        constexpr T&& move(T& t) noexcept {
            return static_cast<T&&>(t);
        }
        
        template<typename T>
        constexpr T&& forward(T& t) noexcept {
            return static_cast<T&&>(t);
        }
        
        template<typename T>
        void swap(T& a, T& b) {
            T temp = move(a);
            a = move(b);
            b = move(temp);
        }
        
        template<typename T, size_t N>
        constexpr size_t array_size(const T (&)[N]) {
            return N;
        }
    }
    
    namespace algorithm {
        template<typename T>
        void qsort_impl(T* base, size_t low, size_t high, int (*comp)(const T*, const T*)) {
            if (low < high) {
                size_t pivot = partition(base, low, high, comp);
                if (pivot > 0) qsort_impl(base, low, pivot - 1, comp);
                qsort_impl(base, pivot + 1, high, comp);
            }
        }
        
        template<typename T>
        size_t partition(T* base, size_t low, size_t high, int (*comp)(const T*, const T*)) {
            T pivot = base[high];
            size_t i = low;
            
            for (size_t j = low; j < high; ++j) {
                if (comp(&base[j], &pivot) < 0) {
                    utility::swap(base[i], base[j]);
                    ++i;
                }
            }
            utility::swap(base[i], base[high]);
            return i;
        }
        
        template<typename T>
        void sort(T* base, size_t nmemb, int (*comp)(const T*, const T*)) {
            if (nmemb > 1) {
                qsort_impl(base, 0, nmemb - 1, comp);
            }
        }
        
        template<typename T>
        T* bsearch(const T* key, const T* base, size_t nmemb, int (*comp)(const T*, const T*)) {
            size_t left = 0, right = nmemb;
            
            while (left < right) {
                size_t mid = left + (right - left) / 2;
                int cmp = comp(key, &base[mid]);
                
                if (cmp == 0) return const_cast<T*>(&base[mid]);
                if (cmp < 0) right = mid;
                else left = mid + 1;
            }
            
            return nullptr;
        }
    }
}

using std::malloc;
using std::calloc;
using std::realloc;
using std::free;
using std::exit;
using std::abort;
using std::atoi;
using std::atol;
using std::atoll;
using std::abs;

#define NULL '\0'
#define EXIT_SUCCESS std::EXIT_SUCCESS
#define EXIT_FAILURE std::EXIT_FAILURE