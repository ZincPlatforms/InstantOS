#include "unistd.hpp"
#include "string.hpp"

extern "C" size_t strlen(const char* str);
extern "C" int snprintf(char* str, size_t size, const char* fmt, ...);

namespace std {
    ssize_t write(int fd, const void* buf, size_t count) {
        return instant::sys::syscall3(instant::sys::Syscall::Write, fd, 
            reinterpret_cast<long>(buf), static_cast<long>(count));
    }
    
    ssize_t read(int fd, void* buf, size_t count) {
        return instant::sys::syscall3(instant::sys::Syscall::Read, fd,
            reinterpret_cast<long>(buf), static_cast<long>(count));
    }
    
    pid_t getpid() {
        return static_cast<pid_t>(instant::sys::syscall0(instant::sys::Syscall::ProcessID));
    }
    
    void yield() {
        instant::sys::syscall0(instant::sys::Syscall::YieldProcess);
    }
    
    int execv(const char* path, char* const argv[]) {
        return static_cast<int>(instant::sys::syscall3(instant::sys::Syscall::Exec,
            reinterpret_cast<long>(path),
            reinterpret_cast<long>(argv),
            0));
    }
    
    int execve(const char* path, char* const argv[], char* const envp[]) {
        return static_cast<int>(instant::sys::syscall4(instant::sys::Syscall::Exec,
            reinterpret_cast<long>(path),
            reinterpret_cast<long>(argv),
            reinterpret_cast<long>(envp),
            0));
    }
    
    long syscall(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
        return instant::sys::syscall5((instant::sys::Syscall)num, arg1, arg2, arg3, arg4, arg5);
    }
    
    namespace process {
        void process_info::yield_to() {
            ::std::yield();
        }
        
        void process_info::terminate() {
            instant::sys::syscall1(instant::sys::Syscall::Kill, pid_);
        }
        
        int process_info::wait() {
            return static_cast<int>(instant::sys::syscall1(instant::sys::Syscall::Wait, pid_));
        }
        
        namespace current {
            pid_t id() {
                return ::std::getpid();
            }
            
            void yield() {
                ::std::yield();
            }
            
            [[noreturn]] void exit(int status) {
                ::std::exit(status);
            }
            
            void sleep(unsigned int seconds) {
                instant::sys::syscall1(instant::sys::Syscall::Sleep, seconds);
            }
        }
    }
    
    namespace io {
        void file_descriptor::close() {
            if (fd_ >= 0) {
                instant::sys::syscall1(instant::sys::Syscall::Close, fd_);
                fd_ = -1;
            }
        }
        
        file_descriptor stdin_fd(STDIN_FILENO, false);
        file_descriptor stdout_fd(STDOUT_FILENO, false);
        file_descriptor stderr_fd(STDERR_FILENO, false);
    }
    
    namespace system {
        void os_info::update() {
            instant::sys::syscall1(instant::sys::Syscall::OSInfo, reinterpret_cast<long>(this));
        }
        
        os_info get_os_info() {
            os_info info;
            info.update();
            return info;
        }
        
        unsigned long get_time() {
            return static_cast<unsigned long>(instant::sys::syscall0(instant::sys::Syscall::GetTime));
        }
        
        void clear_screen() {
            instant::sys::syscall0(instant::sys::Syscall::Clear);
        }
    }
    
    namespace memory {
        void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
            return reinterpret_cast<void*>(instant::sys::syscall5(instant::sys::Syscall::MapArea,
                reinterpret_cast<long>(addr),
                static_cast<long>(length),
                static_cast<long>(prot),
                static_cast<long>(flags),
                static_cast<long>(fd)));
        }
        
        int munmap(void* addr, size_t length) {
            return static_cast<int>(instant::sys::syscall2(instant::sys::Syscall::UnmapArea,
                reinterpret_cast<long>(addr),
                static_cast<long>(length)));
        }
    }
}