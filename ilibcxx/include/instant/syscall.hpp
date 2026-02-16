#pragma once
#include <cstdint>

struct OSInfo {
    char osname[10];
    char loggedOnUser[32];
    char cpuname[64];
    char maxRamGB[8];
    char usedRamGB[8];
    uint8_t major, minor, patch;
    uint64_t buildnum;
};

extern "C" {
    long syscall0(long num);
    long syscall1(long num, long arg1);
    long syscall2(long num, long arg1, long arg2);
    long syscall3(long num, long arg1, long arg2, long arg3);
    long syscall4(long num, long arg1, long arg2, long arg3, long arg4);
    long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5);
}

namespace instant {
    typedef long ssize_t;
    typedef unsigned long size_t;
    typedef int pid_t;

    class sys {
    public:
        enum class Syscall : long {
            OSInfo,
            ProcInfo,
            Exit,
            Write,
            Read,
            Open,
            Close,
            ProcessID,
            Fork,
            Exec,
            Wait,
            Kill,
            MapArea,
            UnmapArea,
            YieldProcess,
            Sleep,
            GetTime,
            Clear,
            FramebufferInfo,
            FramebufferMapping,
            Signal,
            SignalReturn,
            CreateProcess = Fork,
            CloseProcess = Kill,
            WaitForProcess = Wait,
            KillProcess = Kill
        };

        static long syscall0(Syscall num) {
            return ::syscall0(static_cast<long>(num));
        }

        static long syscall1(Syscall num, long arg1) {
            return ::syscall1(static_cast<long>(num), arg1);
        }

        static long syscall2(Syscall num, long arg1, long arg2) {
            return ::syscall2(static_cast<long>(num), arg1, arg2);
        }

        static long syscall3(Syscall num, long arg1, long arg2, long arg3) {
            return ::syscall3(static_cast<long>(num), arg1, arg2, arg3);
        }

        static long syscall4(Syscall num, long arg1, long arg2, long arg3, long arg4) {
            return ::syscall4(static_cast<long>(num), arg1, arg2, arg3, arg4);
        }

        static long syscall5(Syscall num, long arg1, long arg2, long arg3, long arg4, long arg5) {
            return ::syscall5(static_cast<long>(num), arg1, arg2, arg3, arg4, arg5);
        }
    };

    inline OSInfo get_os_info() {
        OSInfo info;
        sys::syscall1(sys::Syscall::OSInfo, reinterpret_cast<long>(&info));
        return info;
    }

    inline void exit(int status) {
        sys::syscall1(sys::Syscall::Exit, static_cast<long>(status));
    }

    inline ssize_t write(int fd, const void* buf, size_t count) {
        return sys::syscall3(sys::Syscall::Write, static_cast<long>(fd),
                       reinterpret_cast<long>(buf), static_cast<long>(count));
    }

    inline ssize_t read(int fd, void* buf, size_t count) {
        return sys::syscall3(sys::Syscall::Read, static_cast<long>(fd),
                       reinterpret_cast<long>(buf), static_cast<long>(count));
    }

    inline pid_t getpid() {
        return static_cast<pid_t>(sys::syscall0(sys::Syscall::ProcessID));
    }

    inline void yield() {
        sys::syscall0(sys::Syscall::YieldProcess);
    }

    inline unsigned long get_time() {
        return static_cast<unsigned long>(sys::syscall0(sys::Syscall::GetTime));
    }

    inline void clear_screen() {
        sys::syscall0(sys::Syscall::Clear);
    }

    inline int sleep(unsigned long milliseconds) {
        return static_cast<int>(sys::syscall1(sys::Syscall::Sleep, static_cast<long>(milliseconds)));
    }
}
