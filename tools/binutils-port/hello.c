// P4 binutils oracle: a minimal mlibc program assembled by GNU `as` and linked
// by GNU `ld` (the x86_64-unknown-instantos cross tools). It writes a known
// marker so the in-OS driver can confirm a GNU-toolchain-built binary runs.
#include <unistd.h>
#include <string.h>

int main(void) {
    static const char msg[] = "HELLO_FROM_GNU_AS_LD\n";
    write(1, msg, strlen(msg));
    return 0;
}
