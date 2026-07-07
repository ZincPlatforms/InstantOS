#include <fs/ext4/ext4_selftest.hpp>
#include <fs/vfs/vfs.hpp>
#include <graphics/console.hpp>

// Drives the ext4 driver through the kernel VFS against the real (AHCI-backed)
// root device: real block-device reads/writes, real kmalloc, and - on a
// simple-journal volume - write-side JBD2 transactions. See the header.

namespace {

int g_checks = 0;
int g_failures = 0;

// open() flags (see cpu/syscall/fs.cpp): O_WRONLY=0x1, O_RDWR=0x2, O_CREAT=0100,
// O_TRUNC=01000. Read-only is access mode 0.
constexpr int kRdOnly = 0;
constexpr int kWrCreateTrunc = 0x1 | 0100 | 01000;

void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        Console::get().drawText("[ext4-selftest]   FAIL: ");
        Console::get().drawText(what);
        Console::get().drawText("\n");
    }
}

uint64_t cstrLen(const char* s) {
    uint64_t n = 0;
    while (s[n]) ++n;
    return n;
}

bool bytesEqual(const char* a, const char* b, uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

bool writeFile(const char* path, const char* data, uint64_t len) {
    FileDescriptor* fd = nullptr;
    if (VFS::get().open(path, kWrCreateTrunc, &fd, 0644) != 0 || !fd) {
        return false;
    }
    const int64_t w = VFS::get().write(fd, data, len);
    VFS::get().close(fd);
    return w == static_cast<int64_t>(len);
}

int64_t readFile(const char* path, char* buf, uint64_t cap) {
    FileDescriptor* fd = nullptr;
    if (VFS::get().open(path, kRdOnly, &fd) != 0 || !fd) {
        return -1;
    }
    const int64_t n = VFS::get().read(fd, buf, cap);
    VFS::get().close(fd);
    return n;
}

}  // namespace

void ext4RunSelfTest() {
    g_checks = 0;
    g_failures = 0;
    Console::get().drawText("[ext4-selftest] starting on / ...\n");

    // 1. Read a file seeded on the disk (read path over the real device).
    {
        const char* expect = "InstantOS ext4 in-OS test seed\n";
        const uint64_t elen = cstrLen(expect);
        char buf[128] = {0};
        const int64_t n = readFile("/readme.txt", buf, sizeof(buf) - 1);
        check(n == static_cast<int64_t>(elen) && bytesEqual(buf, expect, elen),
              "read seeded /readme.txt");
    }

    // 2. Create + write + read back a new file (write path + journaling).
    {
        const char* msg = "hello from the InstantOS kernel via ext4\n";
        const uint64_t mlen = cstrLen(msg);
        check(writeFile("/e4-selftest.txt", msg, mlen), "create+write /e4-selftest.txt");
        char buf[128] = {0};
        const int64_t n = readFile("/e4-selftest.txt", buf, sizeof(buf) - 1);
        check(n == static_cast<int64_t>(mlen) && bytesEqual(buf, msg, mlen),
              "read back /e4-selftest.txt");
    }

    // 3. mkdir + a nested file.
    {
        check(VFS::get().mkdir("/e4-selfdir", 0755) == 0, "mkdir /e4-selfdir");
        const char* c = "nested\n";
        check(writeFile("/e4-selfdir/child.txt", c, 7), "write /e4-selfdir/child.txt");
        char buf[16] = {0};
        const int64_t n = readFile("/e4-selfdir/child.txt", buf, sizeof(buf) - 1);
        check(n == 7 && bytesEqual(buf, c, 7), "read /e4-selfdir/child.txt");
    }

    // 4. stat the created file.
    {
        FileStats st;
        check(VFS::get().stat("/e4-selftest.txt", &st) == 0 && st.type == FileType::Regular,
              "stat /e4-selftest.txt");
    }

    // 5. Clean up (unlink + rmdir) so the run leaves / unchanged.
    check(VFS::get().unlink("/e4-selfdir/child.txt") == 0, "unlink child.txt");
    check(VFS::get().rmdir("/e4-selfdir") == 0, "rmdir /e4-selfdir");
    check(VFS::get().unlink("/e4-selftest.txt") == 0, "unlink /e4-selftest.txt");

    Console::get().drawText("[ext4-selftest] ");
    Console::get().drawNumber(g_checks);
    Console::get().drawText(" checks, ");
    Console::get().drawNumber(g_failures);
    Console::get().drawText(g_failures == 0 ? " failures [ PASS ]\n" : " failures [ FAIL ]\n");
}
