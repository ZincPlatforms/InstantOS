#include "cstdlib.hpp"
#include "string.hpp"
#include "unistd.hpp"
#include <cstddef>
#include <cstdint>

using std::uintptr_t;

extern "C" int strncmp(const char* s1, const char* s2, size_t n);
extern "C" char* strcat(char* dest, const char* src);

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n == SIZE_MAX) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;  // Find end of dest
    while ((*d++ = *src++));  // Copy src to end of dest
    return dest;
}
extern int __cxa_exitfunc(void(*func)());
namespace std {
    static char heap_space[1024 * 1024];
    static size_t heap_offset = 0;
    static bool heap_initialized = false;
    
    struct heap_block {
        size_t size;
        bool free;
        heap_block* next;
    };
    
    static heap_block* free_list = nullptr;
    
    static void init_heap() {
        if (!heap_initialized) {
            free_list = reinterpret_cast<heap_block*>(heap_space);
            free_list->size = sizeof(heap_space) - sizeof(heap_block);
            free_list->free = true;
            free_list->next = nullptr;
            heap_initialized = true;
        }
    }
    
    [[noreturn]] void exit(int status) {
        instant::sys::syscall1(instant::sys::Syscall::Exit, status);
        while(1);
    }
    
    [[noreturn]] void abort() {
        exit(EXIT_FAILURE);
    }
    
    void atexit(void (*func)()) {
        // __cxa_exitfunc(func);
    }

    void* malloc(size_t size) {
        if (size == 0) return nullptr;
        
        init_heap();
        
        size = (size + 15) & ~15UL;
        
        heap_block* current = free_list;
        heap_block* prev = nullptr;
        
        while (current) {
            if (current->free && current->size >= size) {
                if (current->size > size + sizeof(heap_block) + 16) {
                    heap_block* new_block = reinterpret_cast<heap_block*>(
                        reinterpret_cast<char*>(current) + sizeof(heap_block) + size);
                    new_block->size = current->size - size - sizeof(heap_block);
                    new_block->free = true;
                    new_block->next = current->next;
                    
                    current->size = size;
                    current->next = new_block;
                }
                
                current->free = false;
                return reinterpret_cast<char*>(current) + sizeof(heap_block);
            }
            prev = current;
            current = current->next;
        }
        
        return nullptr;
    }
    
    void* calloc(size_t nmemb, size_t size) {
        size_t total = nmemb * size;
        void* ptr = malloc(total);
        if (ptr) {
            memset(ptr, 0, total);
        }
        return ptr;
    }
    
    void* realloc(void* ptr, size_t size) {
        if (!ptr) return malloc(size);
        if (size == 0) {
            free(ptr);
            return nullptr;
        }
        
        heap_block* block = reinterpret_cast<heap_block*>(
            reinterpret_cast<char*>(ptr) - sizeof(heap_block));
        
        if (block->size >= size) {
            return ptr;
        }
        
        void* new_ptr = malloc(size);
        if (new_ptr) {
            memcpy(new_ptr, ptr, block->size < size ? block->size : size);
            free(ptr);
        }
        return new_ptr;
    }
    
    void free(void* ptr) {
        if (!ptr) return;
        
        heap_block* block = reinterpret_cast<heap_block*>(
            reinterpret_cast<char*>(ptr) - sizeof(heap_block));
        block->free = true;
        
        if (block->next && block->next->free) {
            block->size += sizeof(heap_block) + block->next->size;
            block->next = block->next->next;
        }
    }
    
    namespace memory {
        void* allocator::allocate(size_t size) {
            return malloc(size);
        }
        
        void* allocator::allocate_aligned(size_t size, size_t alignment) {
            void* ptr = malloc(size + alignment - 1);
            if (!ptr) return nullptr;
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
            return reinterpret_cast<void*>(aligned);
        }
        
        void allocator::deallocate(void* ptr) {
            free(ptr);
        }
        
        void* allocator::reallocate(void* ptr, size_t new_size) {
            return realloc(ptr, new_size);
        }
    }
    
    int atoi(const char* str) {
        return static_cast<int>(atol(str));
    }
    
    long atol(const char* str) {
        if (!str) return 0;
        
        long result = 0;
        int sign = 1;
        
        while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
            str++;
        }
        
        if (*str == '-') {
            sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }
        
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
        
        return sign * result;
    }
    
    long long atoll(const char* str) {
        if (!str) return 0;
        
        long long result = 0;
        int sign = 1;
        
        while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
            str++;
        }
        
        if (*str == '-') {
            sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }
        
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
        
        return sign * result;
    }
    
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
    
    double strtod(const char* str, char** endptr) {
        if (endptr) *endptr = const_cast<char*>(str);
        return atof(str);
    }
    
    long strtol(const char* str, char** endptr, int base) {
        if (!str || base < 2 || base > 36) {
            if (endptr) *endptr = const_cast<char*>(str);
            return 0;
        }
        
        long result = 0;
        int sign = 1;
        const char* start = str;
        
        while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
            str++;
        }
        
        if (*str == '-') {
            sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }
        
        if (base == 0) {
            if (*str == '0') {
                if (str[1] == 'x' || str[1] == 'X') {
                    base = 16;
                    str += 2;
                } else {
                    base = 8;
                    str++;
                }
            } else {
                base = 10;
            }
        } else if (base == 16 && *str == '0' && (str[1] == 'x' || str[1] == 'X')) {
            str += 2;
        }
        
        while (*str) {
            int digit;
            if (*str >= '0' && *str <= '9') {
                digit = *str - '0';
            } else if (*str >= 'a' && *str <= 'z') {
                digit = *str - 'a' + 10;
            } else if (*str >= 'A' && *str <= 'Z') {
                digit = *str - 'A' + 10;
            } else {
                break;
            }
            
            if (digit >= base) break;
            
            result = result * base + digit;
            str++;
        }
        
        if (endptr) *endptr = const_cast<char*>(str);
        return sign * result;
    }
    
    unsigned long strtoul(const char* str, char** endptr, int base) {
        return static_cast<unsigned long>(strtol(str, endptr, base));
    }
    
    int abs(int n) {
        return n < 0 ? -n : n;
    }
    
    long labs(long n) {
        return n < 0 ? -n : n;
    }
    
    long long llabs(long long n) {
        return n < 0 ? -n : n;
    }
    
    namespace random {
        simple_rng default_rng;
    }
    
    namespace system {
        static char* env_vars[64];
        static int env_count = 0;
        
        char* getenv(const char* name) {
            if (!name) return nullptr;
            
            size_t name_len = strlen(name);
            for (int i = 0; i < env_count; ++i) {
                if (strncmp(env_vars[i], name, name_len) == 0 && env_vars[i][name_len] == '=') {
                    return env_vars[i] + name_len + 1;
                }
            }
            return nullptr;
        }
        
        int setenv(const char* name, const char* value, int overwrite) {
            if (!name || !value) return -1;
            
            for (int i = 0; i < env_count; ++i) {
                size_t name_len = strlen(name);
                if (strncmp(env_vars[i], name, name_len) == 0 && env_vars[i][name_len] == '=') {
                    if (!overwrite) return 0;
                    
                    free(env_vars[i]);
                    size_t total_len = name_len + strlen(value) + 2;
                    env_vars[i] = static_cast<char*>(malloc(total_len));
                    if (!env_vars[i]) return -1;
                    
                    strcpy(env_vars[i], name);
                    strcat(env_vars[i], "=");
                    strcat(env_vars[i], value);
                    return 0;
                }
            }
            
            if (env_count >= 63) return -1;
            
            size_t total_len = strlen(name) + strlen(value) + 2;
            env_vars[env_count] = static_cast<char*>(malloc(total_len));
            if (!env_vars[env_count]) return -1;
            
            strcpy(env_vars[env_count], name);
            strcat(env_vars[env_count], "=");
            strcat(env_vars[env_count], value);
            env_count++;
            
            return 0;
        }
        
        int unsetenv(const char* name) {
            if (!name) return -1;
            
            size_t name_len = strlen(name);
            for (int i = 0; i < env_count; ++i) {
                if (strncmp(env_vars[i], name, name_len) == 0 && env_vars[i][name_len] == '=') {
                    free(env_vars[i]);
                    for (int j = i; j < env_count - 1; ++j) {
                        env_vars[j] = env_vars[j + 1];
                    }
                    env_count--;
                    return 0;
                }
            }
            return -1;
        }
        
        int system(const char* command) {
            return 0;
        }
        
        void quick_exit(int status) {
            exit(status);
        }
        
        void _Exit(int status) {
            exit(status);
        }
    }
}