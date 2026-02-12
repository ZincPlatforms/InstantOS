#pragma once

#include "syscall.hpp"

namespace instant::proc {
    struct Process {
        int id;
        const char* name;
        
        Process() {
            static char process_buffer[256];
            sys::sys::syscall1(sys::sys::Syscall::ProcInfo, reinterpret_cast<long>(process_buffer));
            
            name = process_buffer;
            
            id = 0;
            const char* ptr = process_buffer;
            while (*ptr && *ptr >= '0' && *ptr <= '9') {
                id = id * 10 + (*ptr - '0');
                ptr++;
            }
            
            if (*ptr == ':' || *ptr == ' ') {
                name = ptr + 1;
            }
        }
        
        explicit Process(int process_id) : id(process_id), name(nullptr) {
        }
        
        void kill() {
            sys::syscall1(sys::Syscall::Kill, static_cast<long>(id));
        }
        
        int wait() {
            return static_cast<int>(sys::syscall1(sys::Syscall::Wait, static_cast<long>(id)));
        }
        
        bool is_running() const {
            return id > 0;
        }
        
        const char* get_name() const {
            return name ? name : "unknown";
        }
        
        int get_id() const {
            return id;
        }
    };
    
    inline Process current() {
        return Process();
    }
    
    inline Process create(const char* executable_path, const char* const* argv = nullptr) {
        long result = sys::syscall2(sys::Syscall::Exec, 
                                   reinterpret_cast<long>(executable_path),
                                   reinterpret_cast<long>(argv));
        return Process(static_cast<int>(result));
    }
    
    inline Process fork() {
        long result = sys::syscall0(sys::Syscall::Fork);
        return Process(static_cast<int>(result));
    }
    
    inline void yield() {
        sys::syscall0(sys::Syscall::YieldProcess);
    }
    
    [[noreturn]] inline void exit(int status = 0) {
        sys::syscall1(sys::Syscall::Exit, static_cast<long>(status));
        while(1);    }
    
    inline int getpid() {
        return static_cast<int>(sys::syscall0(sys::Syscall::ProcessID));
    }
    
    inline const char* getprocess() {
        static char process_buffer[256];
        sys::syscall1(sys::Syscall::ProcInfo, reinterpret_cast<long>(process_buffer));
        return process_buffer;
    }
    
} // namespace instant::proc