// P3.2 fd-inheritance test (mlibc-linked). The parent opens a file on fd 3,
// writes a marker, rewinds, and execve()s itself as "child". The child reads
// fd 3: it only sees the marker if the fd survived exec (the kernel keeps the
// handle; libc must rebuild its fd map at entry). Reports via serial (syscall 88).
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

extern char** environ;

static void serial(const char* s) {
    unsigned long n = 0; while (s[n]) n++;
    register unsigned long a asm("rax") = 88;
    register unsigned long b asm("rbx") = (unsigned long)s;
    register unsigned long c asm("r10") = n;
    asm volatile("syscall" : "+r"(a) : "r"(b), "r"(c) : "rcx", "r11", "memory");
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        char buf[16] = {0};
        long n = read(3, buf, sizeof(buf));
        if (n == 8 && memcmp(buf, "JOBFD_OK", 8) == 0) {
            serial("FDINHERIT_CHILD_READ_OK\n");
        } else {
            serial("FDINHERIT_CHILD_READ_FAIL\n");
        }
        return 0;
    }

    int fd = open("/tmp/fdi.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { serial("FDINHERIT_OPEN_FAIL\n"); return 1; }
    if (fd != 3) { dup2(fd, 3); close(fd); }
    if (write(3, "JOBFD_OK", 8) != 8) { serial("FDINHERIT_WRITE_FAIL\n"); return 1; }
    lseek(3, 0, SEEK_SET);
    serial("FDINHERIT_PARENT_EXEC\n");
    char* av[] = { argv[0], (char*)"child", 0 };
    execve(argv[0], av, environ);
    serial("FDINHERIT_EXEC_FAIL\n");
    return 1;
}
