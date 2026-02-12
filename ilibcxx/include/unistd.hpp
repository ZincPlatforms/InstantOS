#pragma once

#include "instant/syscall.hpp"
#include <cstdio.hpp>
#include <string.hpp>

namespace std {
    using pid_t = instant::pid_t;
    
    [[noreturn]] void exit(int status);
    
    ssize_t write(int fd, const void* buf, size_t count);
    ssize_t read(int fd, void* buf, size_t count);
    
    pid_t getpid();
    void yield();
    int execv(const char* path, char* const argv[]);
    int execve(const char* path, char* const argv[], char* const envp[]);
    
    long syscall(long num, long arg1 = 0, long arg2 = 0, long arg3 = 0, long arg4 = 0, long arg5 = 0);
    
    namespace process {
        class process_info {
        private:
            pid_t pid_;
            
        public:
            explicit process_info(pid_t pid) : pid_(pid) {}
            
            pid_t id() const { return pid_; }
            bool is_valid() const { return pid_ > 0; }
            
            void yield_to();
            void terminate();
            int wait();
        };
        
        namespace current {
            pid_t id();
            void yield();
            [[noreturn]] void exit(int status);
            void sleep(unsigned int seconds);
        }
        
        class process_builder {
        private:
            const char* path_;
            char* const* argv_;
            char* const* envp_;
            
        public:
            process_builder(const char* path) : path_(path), argv_(nullptr), envp_(nullptr) {}
            
            process_builder& args(char* const argv[]) {
                argv_ = argv;
                return *this;
            }
            
            process_builder& environment(char* const envp[]) {
                envp_ = envp;
                return *this;
            }
            
            int exec() {
                if (envp_) {
                    return execve(path_, argv_, envp_);
                } else {
                    return execv(path_, argv_);
                }
            }
        };
        
        inline process_builder create(const char* path) {
            return process_builder(path);
        }
    }
    
    namespace io {
        class file_descriptor {
        private:
            int fd_;
            bool owns_fd_;
            
        public:
            explicit file_descriptor(int fd, bool owns = false) : fd_(fd), owns_fd_(owns) {}
            
            ~file_descriptor() {
                if (owns_fd_ && fd_ >= 0) {
                    close();
                }
            }
            
            file_descriptor(file_descriptor&& other) noexcept 
                : fd_(other.fd_), owns_fd_(other.owns_fd_) {
                other.fd_ = -1;
                other.owns_fd_ = false;
            }
            
            file_descriptor& operator=(file_descriptor&& other) noexcept {
                if (this != &other) {
                    if (owns_fd_ && fd_ >= 0) close();
                    fd_ = other.fd_;
                    owns_fd_ = other.owns_fd_;
                    other.fd_ = -1;
                    other.owns_fd_ = false;
                }
                return *this;
            }
            
            file_descriptor(const file_descriptor&) = delete;
            file_descriptor& operator=(const file_descriptor&) = delete;
            
            int get() const { return fd_; }
            bool is_valid() const { return fd_ >= 0; }
            
            ssize_t write(const void* buf, size_t count) {
                return ::std::write(fd_, buf, count);
            }
            
            ssize_t read(void* buf, size_t count) {
                return ::std::read(fd_, buf, count);
            }
            
            void close();
            
            file_descriptor& operator<<(const char* str) {
                if (str) write(str, strlen(str));
                return *this;
            }
            
            file_descriptor& operator<<(char c) {
                write(&c, 1);
                return *this;
            }
            
            file_descriptor& operator<<(int n) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", n);
                return *this << buf;
            }
        };
        
        extern file_descriptor stdin_fd;
        extern file_descriptor stdout_fd;
        extern file_descriptor stderr_fd;
    }
    
    namespace system {
        struct os_info {
            char osname[16];
            char logged_user[32];
            char cpu_name[64];
            char max_ram_gb[8];
            char used_ram_gb[8];
            
            void update();
        };
        
        os_info get_os_info();
        unsigned long get_time();
        void clear_screen();
    }
    
    namespace memory {
        void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
        int munmap(void* addr, size_t length);
        
        class mapped_memory {
        private:
            void* addr_;
            size_t length_;
            
        public:
            mapped_memory(size_t length, int prot = 0, int flags = 0, int fd = -1, long offset = 0)
                : length_(length) {
                addr_ = mmap(nullptr, length, prot, flags, fd, offset);
                if (addr_ == (void*)-1) {
                    addr_ = nullptr;
                }
            }
            
            ~mapped_memory() {
                if (addr_) {
                    munmap(addr_, length_);
                }
            }
            
            mapped_memory(mapped_memory&& other) noexcept 
                : addr_(other.addr_), length_(other.length_) {
                other.addr_ = nullptr;
                other.length_ = 0;
            }
            
            mapped_memory& operator=(mapped_memory&& other) noexcept {
                if (this != &other) {
                    if (addr_) munmap(addr_, length_);
                    addr_ = other.addr_;
                    length_ = other.length_;
                    other.addr_ = nullptr;
                    other.length_ = 0;
                }
                return *this;
            }
            
            mapped_memory(const mapped_memory&) = delete;
            mapped_memory& operator=(const mapped_memory&) = delete;
            
            void* get() const { return addr_; }
            size_t size() const { return length_; }
            bool is_valid() const { return addr_ != nullptr; }
            
            template<typename T>
            T* as() const {
                return static_cast<T*>(addr_);
            }
        };
    }
}

using std::write;
using std::read;
using std::getpid;
using std::yield;
using std::execv;
using std::execve;
using std::syscall;

#define STDIN_FILENO std::STDIN_FILENO
#define STDOUT_FILENO std::STDOUT_FILENO
#define STDERR_FILENO std::STDERR_FILENO