// Host-side Ext4FS read-path tests.
//
// Compiles the REAL kernel driver (src/fs/ext4/ext4.cpp) against a file-backed
// BlockDevice and drives it through the same VNodeOps the kernel uses, then
// asserts it reads back exactly what mke2fs wrote. Because ext4.cpp only needs
// the heap + string helpers (no Console/graphics), the only kernel facilities
// this harness has to supply are kmalloc/kfree and the VNode/FileSystem
// constructors.
//
// Coverage across three golden images (tools/ext4-fixtures/make-images.sh):
//   ext4.img    - extents, 4 KiB blocks, 32-bit group descriptors
//   ext2.img    - classic block map (direct/indirect), 1 KiB blocks
//   ext4def.img - full default feature set: 64-bit descriptors, flex_bg,
//                 metadata_csum, HTree directories, journal
//
// Build/run: tools/ext4-fixtures/run-ext4-fixtures.sh.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include <memory/heap.hpp>
#include <fs/ext4/ext4.hpp>

// ---------------------------------------------------------------------------
// Kernel-facility shims (satisfy the symbols src/fs/ext4/ext4.cpp references)
// ---------------------------------------------------------------------------

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
void* krealloc(void* ptr, size_t newSize) { return realloc(ptr, newSize); }
void* kmalloc_aligned(size_t size, size_t /*align*/) { return malloc(size); }

// Clock shim: the driver stamps inode timestamps with time_get_unix(). Use a
// fixed value so host-generated images are deterministic and e2fsck-clean.
uint64_t boot_unix_time = 1700000000ull;
uint64_t time_get_unix() { return 1700000000ull; }

VNode::VNode(FileSystem* f, uint64_t i, FileType t) {
    ops = nullptr;
    refCount = 0;
    fs = f;
    inode = i;
    type = t;
    data = nullptr;
}
VNode::~VNode() {}

FileSystem::FileSystem(const char* n) {
    size_t i = 0;
    if (n) {
        for (; n[i] && i < sizeof(name) - 1; ++i) {
            name[i] = n[i];
        }
    }
    name[i] = '\0';
}
FileSystem::~FileSystem() {}

// ---------------------------------------------------------------------------
// File-backed block device
// ---------------------------------------------------------------------------

class FileBlockDevice : public BlockDevice {
public:
    FileBlockDevice(FILE* file, uint64_t size) : file(file), size(size) {}

    bool read(uint64_t offset, void* buffer, uint64_t sz) override {
        if (offset > size || sz > size - offset) {
            return false;
        }
        if (fseek(file, static_cast<long>(offset), SEEK_SET) != 0) {
            return false;
        }
        return fread(buffer, 1, sz, file) == sz;
    }
    bool write(uint64_t offset, const void* buffer, uint64_t sz) override {
        if (offset > size || sz > size - offset) {
            return false;
        }
        if (fseek(file, static_cast<long>(offset), SEEK_SET) != 0) {
            return false;
        }
        if (fwrite(buffer, 1, sz, file) != sz) {
            return false;
        }
        fflush(file);
        return true;
    }
    uint64_t getSize() override { return size; }

private:
    FILE* file;
    uint64_t size;
};

// In-memory device for negative probe tests.
class MemBlockDevice : public BlockDevice {
public:
    MemBlockDevice(const uint8_t* data, uint64_t size) : data(data), size(size) {}
    bool read(uint64_t offset, void* buffer, uint64_t sz) override {
        if (offset > size || sz > size - offset) return false;
        std::memcpy(buffer, data + offset, sz);
        return true;
    }
    bool write(uint64_t, const void*, uint64_t) override { return false; }
    uint64_t getSize() override { return size; }

private:
    const uint8_t* data;
    uint64_t size;
};

// ---------------------------------------------------------------------------
// Tiny test harness
// ---------------------------------------------------------------------------

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);            \
        }                                                                      \
    } while (0)

static void freeVNode(VNode* v) {
    if (!v) return;
    if (v->getData()) kfree(v->getData());
    delete v;
}

static const char kHello[] = "Hello, ext4 from InstantOS!\n";
static const char kNested[] = "nested file contents\n";
static const char kSlowTarget[] =
    "/an/intentionally/long/symlink/target/path/that/exceeds/sixty/characters";
static constexpr uint64_t kBigSize = 400000;
static constexpr int kManyCount = 600;   // files file_0 .. file_599

// Read every entry of `dir` into a heap buffer. Returns count; caller frees.
static DirEntry* readAllDir(VNode* dir, uint64_t* countOut) {
    const uint64_t cap = 2048;
    DirEntry* buf = static_cast<DirEntry*>(std::malloc(cap * sizeof(DirEntry)));
    uint64_t count = 0;
    if (dir->ops->readdir(dir, buf, cap, &count) != 0) {
        std::free(buf);
        *countOut = 0;
        return nullptr;
    }
    *countOut = count;
    return buf;
}

static bool listContains(const DirEntry* e, uint64_t n, const char* name) {
    for (uint64_t i = 0; i < n; ++i) {
        if (std::strcmp(e[i].name, name) == 0) return true;
    }
    return false;
}

static bool dirHas(VNode* dir, const char* name) {
    uint64_t n = 0;
    DirEntry* e = readAllDir(dir, &n);
    if (!e) return false;
    bool found = listContains(e, n, name);
    std::free(e);
    return found;
}

static void testImage(const char* path, const char* label,
                      bool expect64bit, bool expectExtents) {
    std::printf("== %s (%s) ==\n", label, path);

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::printf("  FAIL: cannot open %s (run make-images.sh first)\n", path);
        ++g_failures;
        ++g_checks;
        return;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    FileBlockDevice dev(f, static_cast<uint64_t>(fileSize));

    // The boot path uses probe() to pick ext4 vs FAT; it must accept this image.
    CHECK(Ext4FS::probe(&dev));

    Ext4FS fs(&dev);
    CHECK(fs.mount("/") == 0);
    CHECK(fs.isMounted());
    CHECK(fs.is64Bit() == expect64bit);
    CHECK(fs.hasExtentFeature() == expectExtents);
    if (expect64bit) {
        CHECK(fs.descriptorSize() >= 64);
    } else {
        CHECK(fs.descriptorSize() == 32);
    }

    VNode* root = fs.getRoot();
    CHECK(root != nullptr);
    if (!root) { std::fclose(f); return; }

    CHECK(dirHas(root, "hello.txt"));
    CHECK(dirHas(root, "dir"));
    CHECK(dirHas(root, "big.bin"));
    CHECK(dirHas(root, "link"));
    CHECK(dirHas(root, "many"));
    CHECK(dirHas(root, "slowlink"));

    // hello.txt: lookup + stat + read exact contents.
    VNode* hello = root->ops->lookup(root, "hello.txt");
    CHECK(hello != nullptr);
    if (hello) {
        FileStats st;
        CHECK(hello->ops->stat(hello, &st) == 0);
        CHECK(st.type == FileType::Regular);
        CHECK(st.size == sizeof(kHello) - 1);
        char buf[64] = {0};
        int64_t n = hello->ops->read(hello, buf, sizeof(buf), 0);
        CHECK(n == static_cast<int64_t>(sizeof(kHello) - 1));
        CHECK(std::memcmp(buf, kHello, sizeof(kHello) - 1) == 0);
        freeVNode(hello);
    }

    // dir/nested.txt: descend one level, then read.
    VNode* dir = root->ops->lookup(root, "dir");
    CHECK(dir != nullptr);
    if (dir) {
        FileStats st;
        dir->ops->stat(dir, &st);
        CHECK(st.type == FileType::Directory);
        VNode* nested = dir->ops->lookup(dir, "nested.txt");
        CHECK(nested != nullptr);
        if (nested) {
            char buf[64] = {0};
            int64_t n = nested->ops->read(nested, buf, sizeof(buf), 0);
            CHECK(n == static_cast<int64_t>(sizeof(kNested) - 1));
            CHECK(std::memcmp(buf, kNested, sizeof(kNested) - 1) == 0);
            freeVNode(nested);
        }
        freeVNode(dir);
    }

    // big.bin: multi-block file. Verifies extent (ext4) / indirect (ext2) block
    // resolution and the deterministic byte[i] = i % 251 pattern.
    VNode* big = root->ops->lookup(root, "big.bin");
    CHECK(big != nullptr);
    if (big) {
        FileStats st;
        big->ops->stat(big, &st);
        CHECK(st.size == kBigSize);
        uint8_t* buf = static_cast<uint8_t*>(std::malloc(kBigSize));
        int64_t n = big->ops->read(big, buf, kBigSize, 0);
        CHECK(n == static_cast<int64_t>(kBigSize));
        bool patternOk = true;
        for (uint64_t i = 0; i < kBigSize; ++i) {
            if (buf[i] != static_cast<uint8_t>(i % 251)) { patternOk = false; break; }
        }
        CHECK(patternOk);
        // Read at a non-zero, non-block-aligned offset mid-file.
        const uint64_t off = 123456, len = 50000;
        int64_t m = big->ops->read(big, buf, len, off);
        CHECK(m == static_cast<int64_t>(len));
        bool offsetOk = true;
        for (uint64_t i = 0; i < len; ++i) {
            if (buf[i] != static_cast<uint8_t>((off + i) % 251)) { offsetOk = false; break; }
        }
        CHECK(offsetOk);
        std::free(buf);
        freeVNode(big);
    }

    // many/: large directory. Linear (multi-block) on ext4.img/ext2.img, HTree
    // on ext4def.img. Read the full listing and a late entry either way.
    VNode* many = root->ops->lookup(root, "many");
    CHECK(many != nullptr);
    if (many) {
        uint64_t n = 0;
        DirEntry* e = readAllDir(many, &n);
        CHECK(e != nullptr);
        if (e) {
            CHECK(n >= static_cast<uint64_t>(kManyCount));   // + "." and ".."
            CHECK(listContains(e, n, "file_0"));
            CHECK(listContains(e, n, "file_599"));
            std::free(e);
        }
        // Look up and read a late entry (exercises HTree leaf traversal).
        VNode* late = many->ops->lookup(many, "file_599");
        CHECK(late != nullptr);
        if (late) {
            char buf[16] = {0};
            int64_t r = late->ops->read(late, buf, sizeof(buf), 0);
            CHECK(r == 4);   // "599\n"
            CHECK(std::memcmp(buf, "599\n", 4) == 0);
            freeVNode(late);
        }
        freeVNode(many);
    }

    // link -> hello.txt : fast (inline) symlink.
    VNode* link = root->ops->lookup(root, "link");
    CHECK(link != nullptr);
    if (link) {
        FileStats st;
        link->ops->stat(link, &st);
        CHECK(st.type == FileType::Symlink);
        char target[64] = {0};
        int64_t n = link->ops->readlink(link, target, sizeof(target) - 1);
        CHECK(n == 9);
        CHECK(std::strcmp(target, "hello.txt") == 0);
        freeVNode(link);
    }

    // slowlink : slow (out-of-line, data-block) symlink.
    VNode* slow = root->ops->lookup(root, "slowlink");
    CHECK(slow != nullptr);
    if (slow) {
        FileStats st;
        slow->ops->stat(slow, &st);
        CHECK(st.type == FileType::Symlink);
        CHECK(st.size == sizeof(kSlowTarget) - 1);
        char target[128] = {0};
        int64_t n = slow->ops->readlink(slow, target, sizeof(target) - 1);
        CHECK(n == static_cast<int64_t>(sizeof(kSlowTarget) - 1));
        CHECK(std::memcmp(target, kSlowTarget, sizeof(kSlowTarget) - 1) == 0);
        freeVNode(slow);
    }

    // statfs sanity.
    FsStats fst;
    CHECK(root->ops->statfs(root, &fst) == 0);
    CHECK(fst.fsType == 0xEF53);
    CHECK(fst.nameMax == 255);
    CHECK(fst.totalBlocks > 0);

    std::fclose(f);
}

// Exercises the write path against a writable image, then verifies persistence
// by re-mounting read-only. On-disk correctness is checked separately with
// P1.4 random-op write fuzzer. Hammers the driver with a randomized sequence of
// create/write/append/truncate/chmod/rename/unlink operations; on-disk
// consistency is validated afterward by `e2fsck` in run-ext4-fixtures.sh (the
// host oracle). Uses live lookup() results as ground truth so the harness never
// touches a stale node.
static uint32_t g_fuzzRng = 0;
static uint32_t fuzzRand() { g_fuzzRng = g_fuzzRng * 1664525u + 1013904223u; return g_fuzzRng; }

static void testFuzz(const char* path, uint32_t seed, int ops) {
    std::printf("== fuzz (%s, seed=%u, ops=%d) ==\n", path, seed, ops);
    FILE* f = std::fopen(path, "r+b");
    if (!f) { std::printf("  FAIL: cannot open %s\n", path); ++g_failures; ++g_checks; return; }
    fseek(f, 0, SEEK_END); long fileSize = ftell(f); fseek(f, 0, SEEK_SET);
    FileBlockDevice dev(f, static_cast<uint64_t>(fileSize));
    Ext4FS fs(&dev);
    if (fs.mount("/") != 0 || !fs.isWritable()) {
        std::printf("  FAIL: mount/writable %s\n", path); ++g_failures; ++g_checks; std::fclose(f); return;
    }
    VNode* root = fs.getRoot();
    g_fuzzRng = seed;

    const int NF = 48;
    char nm[NF][16];
    for (int i = 0; i < NF; ++i) std::snprintf(nm[i], sizeof(nm[i]), "z%d", i);
    static uint8_t buf[9000];
    int created = 0, wrote = 0, truncd = 0, renamed = 0, removed = 0, chmodd = 0;

    for (int step = 0; step < ops; ++step) {
        int i = fuzzRand() % NF;
        VNode* n = root->ops->lookup(root, nm[i]);
        if (!n) {
            VNode* nn = nullptr;
            if (root->ops->create(root, nm[i], 0644, &nn) == 0 && nn) {
                ++created;
                uint32_t len = fuzzRand() % 9000;
                if (len) { for (uint32_t k = 0; k < len; ++k) buf[k] = static_cast<uint8_t>(fuzzRand()); nn->ops->write(nn, buf, len, 0); ++wrote; }
                freeVNode(nn);
            }
            continue;
        }
        FileStats st{}; n->ops->stat(n, &st);
        switch (fuzzRand() % 6) {
            case 0: {   // append
                uint32_t len = 1 + (fuzzRand() % 9000);
                for (uint32_t k = 0; k < len; ++k) buf[k] = static_cast<uint8_t>(fuzzRand());
                n->ops->write(n, buf, len, st.size); ++wrote;
            } break;
            case 1:     // truncate (shrink or grow, incl. to 0)
                n->ops->truncate(n, st.size ? (fuzzRand() % (st.size + 1)) : (fuzzRand() % 9000)); ++truncd;
                break;
            case 2:     // chmod
                n->ops->chmod(n, 0400 | (fuzzRand() % 0400)); ++chmodd;
                break;
            case 3: {   // rename to another slot (may overwrite an existing file)
                int j = fuzzRand() % NF;
                if (j != i && root->ops->rename(root, nm[i], root, nm[j]) == 0) ++renamed;
            } break;
            default:    // unlink
                if (root->ops->unlink(root, nm[i]) == 0) ++removed;
                break;
        }
        freeVNode(n);
    }

    // Tidy up so the post-fuzz image is a clean tree for e2fsck to bless.
    for (int i = 0; i < NF; ++i) {
        VNode* n = root->ops->lookup(root, nm[i]);
        if (n) { freeVNode(n); root->ops->unlink(root, nm[i]); }
    }
    std::printf("  fuzz: created=%d wrote=%d trunc=%d chmod=%d renamed=%d removed=%d\n",
                created, wrote, truncd, chmodd, renamed, removed);
    CHECK(true);   // survived without crashing; e2fsck is the real consistency oracle
    std::fclose(f);
}

// P1.4 HTree-directory write: modifying an indexed directory must convert it to
// linear and keep every entry, staying e2fsck-clean (validated by the script).
static void testHtreeWrite(const char* path) {
    std::printf("== htree write (%s) ==\n", path);
    FILE* f = std::fopen(path, "r+b");
    if (!f) { std::printf("  FAIL: cannot open %s\n", path); ++g_failures; ++g_checks; return; }
    fseek(f, 0, SEEK_END); long fileSize = ftell(f); fseek(f, 0, SEEK_SET);
    FileBlockDevice dev(f, static_cast<uint64_t>(fileSize));
    Ext4FS fs(&dev);
    if (fs.mount("/") != 0 || !fs.isWritable()) { std::printf("  FAIL: mount %s\n", path); ++g_failures; ++g_checks; std::fclose(f); return; }
    VNode* root = fs.getRoot();

    VNode* many = root->ops->lookup(root, "many");
    CHECK(many != nullptr);
    if (many) {
        // create a new entry -> triggers HTree->linear conversion of the 600-entry dir
        VNode* nf = nullptr;
        CHECK(many->ops->create(many, "converted_new", 0644, &nf) == 0);
        if (nf) { const char* m = "x"; nf->ops->write(nf, m, 1, 0); freeVNode(nf); }
        // original entries must survive the conversion
        VNode* a = many->ops->lookup(many, "file_0");
        VNode* b = many->ops->lookup(many, "file_599");
        VNode* c = many->ops->lookup(many, "converted_new");
        CHECK(a != nullptr); CHECK(b != nullptr); CHECK(c != nullptr);
        if (a) freeVNode(a); if (b) freeVNode(b); if (c) freeVNode(c);
        // remove one entry on the now-linear dir
        CHECK(many->ops->unlink(many, "file_300") == 0);
        CHECK(many->ops->lookup(many, "file_300") == nullptr);
        // count remaining: 600 - 1 removed + 1 added = 600
        uint64_t n = 0; DirEntry* e = readAllDir(many, &n);
        int real = 0; for (uint64_t i = 0; i < n; ++i) if (std::strcmp(e[i].name, ".") && std::strcmp(e[i].name, "..")) ++real;
        CHECK(real == 600);
        std::free(e);
        freeVNode(many);
    }
    std::fclose(f);
}

// `e2fsck -fn` by run-ext4-fixtures.sh.
static void testWriteImage(const char* path) {
    std::printf("== write path (%s) ==\n", path);
    FILE* f = std::fopen(path, "r+b");
    if (!f) {
        std::printf("  FAIL: cannot open %s r+b\n", path);
        ++g_failures; ++g_checks;
        return;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    FileBlockDevice dev(f, static_cast<uint64_t>(fileSize));
    Ext4FS fs(&dev);
    CHECK(fs.mount("/") == 0);
    CHECK(fs.isWritable());
    VNode* root = fs.getRoot();
    if (!root || !fs.isWritable()) { std::fclose(f); return; }

    // create + write + read-back.
    const char* msg = "hello ext4 write path\n";
    const uint64_t msgLen = std::strlen(msg);
    VNode* nf = nullptr;
    CHECK(root->ops->create(root, "wtest.txt", 0644, &nf) == 0);
    if (nf) {
        CHECK(nf->ops->write(nf, msg, msgLen, 0) == (int64_t)msgLen);
        char buf[64] = {0};
        CHECK(nf->ops->read(nf, buf, sizeof(buf), 0) == (int64_t)msgLen);
        CHECK(std::memcmp(buf, msg, msgLen) == 0);
        freeVNode(nf);
    }

    // mkdir + nested file.
    VNode* nd = nullptr;
    CHECK(root->ops->mkdir(root, "wdir", 0755, &nd) == 0);
    if (nd) {
        VNode* c = nullptr;
        CHECK(nd->ops->create(nd, "child.txt", 0644, &c) == 0);
        if (c) {
            const char* m2 = "child contents\n";
            CHECK(c->ops->write(c, m2, std::strlen(m2), 0) == (int64_t)std::strlen(m2));
            freeVNode(c);
        }
        freeVNode(nd);
    }

    // Large file spanning indirect blocks: create, write pattern, read back,
    // then truncate and re-verify.
    VNode* big = nullptr;
    CHECK(root->ops->create(root, "big.dat", 0644, &big) == 0);
    if (big) {
        const uint64_t n = 200000;
        uint8_t* data = static_cast<uint8_t*>(std::malloc(n));
        for (uint64_t i = 0; i < n; ++i) data[i] = static_cast<uint8_t>((i * 7 + 3) % 253);
        CHECK(big->ops->write(big, data, n, 0) == (int64_t)n);
        uint8_t* rb = static_cast<uint8_t*>(std::malloc(n));
        CHECK(big->ops->read(big, rb, n, 0) == (int64_t)n);
        CHECK(std::memcmp(data, rb, n) == 0);
        CHECK(big->ops->truncate(big, 5000) == 0);
        FileStats st; big->ops->stat(big, &st);
        CHECK(st.size == 5000);
        CHECK(big->ops->read(big, rb, n, 0) == 5000);
        CHECK(std::memcmp(data, rb, 5000) == 0);
        std::free(data); std::free(rb);
        freeVNode(big);
    }

    // Metadata ops.
    VNode* cf = root->ops->lookup(root, "wtest.txt");
    if (cf) {
        CHECK(cf->ops->chmod(cf, 0600) == 0);
        CHECK(cf->ops->chown(cf, 1000, 1000) == 0);
        FileStats st; cf->ops->stat(cf, &st);
        CHECK((st.mode & 07777) == 0600);
        CHECK(st.uid == 1000 && st.gid == 1000);
        freeVNode(cf);
    }

    // rename, hardlink, symlink.
    CHECK(root->ops->rename(root, "wtest.txt", root, "wtest2.txt") == 0);
    VNode* gone = root->ops->lookup(root, "wtest.txt");
    CHECK(gone == nullptr);
    if (gone) freeVNode(gone);

    CHECK(root->ops->link(root, "wtest2.txt", root, "wlink.txt") == 0);
    VNode* lk = root->ops->lookup(root, "wlink.txt");
    CHECK(lk != nullptr);
    if (lk) { FileStats st; lk->ops->stat(lk, &st); CHECK(st.links == 2); freeVNode(lk); }

    VNode* sl = nullptr;
    CHECK(root->ops->symlink(root, "myslink", "wtest2.txt", &sl) == 0);
    if (sl) freeVNode(sl);
    VNode* sll = root->ops->lookup(root, "myslink");
    CHECK(sll != nullptr);
    if (sll) {
        FileStats st; sll->ops->stat(sll, &st);
        CHECK(st.type == FileType::Symlink);
        char t[64] = {0};
        CHECK(sll->ops->readlink(sll, t, sizeof(t) - 1) == 10);
        CHECK(std::strcmp(t, "wtest2.txt") == 0);
        freeVNode(sll);
    }

    // unlink (drops link count 2 -> 1) and rmdir (after emptying the dir).
    CHECK(root->ops->unlink(root, "wlink.txt") == 0);
    VNode* wd = root->ops->lookup(root, "wdir");
    if (wd) { CHECK(wd->ops->unlink(wd, "child.txt") == 0); freeVNode(wd); }
    CHECK(root->ops->rmdir(root, "wdir") == 0);

    std::fclose(f);

    // Persistence: re-mount read-only and confirm the written data survived.
    FILE* f2 = std::fopen(path, "rb");
    if (f2) {
        fseek(f2, 0, SEEK_END); long s2 = ftell(f2); fseek(f2, 0, SEEK_SET);
        FileBlockDevice dev2(f2, static_cast<uint64_t>(s2));
        Ext4FS fs2(&dev2);
        CHECK(fs2.mount("/") == 0);
        VNode* r2 = fs2.getRoot();
        VNode* v = r2 ? r2->ops->lookup(r2, "wtest2.txt") : nullptr;
        CHECK(v != nullptr);
        if (v) {
            char buf[64] = {0};
            CHECK(v->ops->read(v, buf, sizeof(buf), 0) == (int64_t)msgLen);
            CHECK(std::memcmp(buf, msg, msgLen) == 0);
            freeVNode(v);
        }
        CHECK(r2 && r2->ops->lookup(r2, "wdir") == nullptr);   // rmdir persisted
        std::fclose(f2);
    }
}

// --- JBD2 helpers (big-endian) for crafting a dirty journal ---
static void hwbe32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24); p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);  p[3] = static_cast<uint8_t>(v);
}
static void hwbe16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8); p[1] = static_cast<uint8_t>(v);
}
static uint32_t hbe32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

static bool copyFile(const char* src, const char* dst) {
    FILE* a = std::fopen(src, "rb"); if (!a) return false;
    FILE* b = std::fopen(dst, "wb"); if (!b) { std::fclose(a); return false; }
    char buf[65536]; size_t n; bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), a)) > 0) {
        if (std::fwrite(buf, 1, n, b) != n) { ok = false; break; }
    }
    std::fclose(a); std::fclose(b); return ok;
}

// Craft a single JBD2 transaction that overwrites journaltest.txt's data block
// with 'N's, mark the journal dirty (+ RECOVER), snapshot the dirty image for a
// Linux e2fsck cross-check, then confirm the driver replays it on mount.
static void testJournalRecovery(const char* path, const char* dirtyPath) {
    std::printf("== journal recovery (%s) ==\n", path);

    FILE* f = std::fopen(path, "r+b");
    if (!f) { std::printf("  FAIL: open %s\n", path); ++g_failures; ++g_checks; return; }
    fseek(f, 0, SEEK_END); long fsz = ftell(f); fseek(f, 0, SEEK_SET);
    FileBlockDevice dev(f, static_cast<uint64_t>(fsz));

    uint32_t bs = 0;
    uint64_t target = 0, j0 = 0, j1 = 0, j2 = 0, j3 = 0;
    {
        Ext4FS fs1(&dev);
        CHECK(fs1.mount("/") == 0);          // clean journal -> replay is a no-op
        bs = fs1.getBlockSize();
        VNode* root = fs1.getRoot();
        VNode* v = root ? root->ops->lookup(root, "journaltest.txt") : nullptr;
        CHECK(v != nullptr);
        if (!v) { std::fclose(f); return; }
        uint32_t jt = static_cast<uint32_t>(v->getInode());
        freeVNode(v);
        CHECK(fs1.resolveInodeBlock(jt, 0, &target));   // file's data block
        CHECK(fs1.resolveInodeBlock(8, 0, &j0));        // journal superblock
        CHECK(fs1.resolveInodeBlock(8, 1, &j1));        // log: descriptor
        CHECK(fs1.resolveInodeBlock(8, 2, &j2));        // log: data
        CHECK(fs1.resolveInodeBlock(8, 3, &j3));        // log: commit
    }

    uint8_t* jsb = static_cast<uint8_t*>(std::malloc(bs));
    dev.read(j0 * bs, jsb, bs);
    const uint32_t jSeq = hbe32(jsb + 24);
    const uint32_t jFirst = hbe32(jsb + 20);

    // Descriptor block: one tag pointing at the file's data block.
    uint8_t* desc = static_cast<uint8_t*>(std::calloc(1, bs));
    hwbe32(desc + 0, 0xC03B3998u); hwbe32(desc + 4, 1); hwbe32(desc + 8, jSeq);
    hwbe32(desc + 12, static_cast<uint32_t>(target));   // t_blocknr
    hwbe16(desc + 16, 0);                               // t_checksum
    hwbe16(desc + 18, 8);                               // t_flags = LAST_TAG
    std::memcpy(desc + 20, jsb + 48, 16);               // uuid = journal s_uuid
    dev.write(j1 * bs, desc, bs);

    // Data block: all 'N'.
    uint8_t* data = static_cast<uint8_t*>(std::malloc(bs));
    std::memset(data, 'N', bs);
    dev.write(j2 * bs, data, bs);

    // Commit block.
    uint8_t* commit = static_cast<uint8_t*>(std::calloc(1, bs));
    hwbe32(commit + 0, 0xC03B3998u); hwbe32(commit + 4, 2); hwbe32(commit + 8, jSeq);
    dev.write(j3 * bs, commit, bs);

    // Journal superblock: mark the log as starting at jFirst (dirty).
    hwbe32(jsb + 28, jFirst);
    dev.write(j0 * bs, jsb, bs);

    // Filesystem superblock: set the RECOVER incompat flag (LE @ 1024+0x60).
    uint8_t feat[4];
    dev.read(1024 + 0x60, feat, 4);
    feat[0] |= 0x04;
    dev.write(1024 + 0x60, feat, 4);

    fflush(f); std::fclose(f);
    std::free(jsb); std::free(desc); std::free(data); std::free(commit);

    // Snapshot the dirty image so Linux e2fsck can replay the same journal.
    CHECK(copyFile(path, dirtyPath));

    // Driver mounts -> journalReplay applies the transaction.
    FILE* g = std::fopen(path, "r+b");
    CHECK(g != nullptr);
    if (!g) return;
    fseek(g, 0, SEEK_END); long gsz = ftell(g); fseek(g, 0, SEEK_SET);
    FileBlockDevice dev2(g, static_cast<uint64_t>(gsz));
    Ext4FS fs2(&dev2);
    CHECK(fs2.mount("/") == 0);
    VNode* r2 = fs2.getRoot();
    VNode* v2 = r2 ? r2->ops->lookup(r2, "journaltest.txt") : nullptr;
    CHECK(v2 != nullptr);
    if (v2) {
        uint8_t* rb = static_cast<uint8_t*>(std::malloc(bs));
        int64_t n = v2->ops->read(v2, rb, bs, 0);
        CHECK(n == static_cast<int64_t>(bs));
        bool allN = (n == static_cast<int64_t>(bs));
        for (int64_t i = 0; i < n; ++i) if (rb[i] != 'N') { allN = false; break; }
        CHECK(allN);   // replayed journal data landed in the file's block
        std::free(rb);
        freeVNode(v2);
    }
    std::fclose(g);
}

// Exercises WRITE-side journaling: a normal journaled create (commit +
// checkpoint), then a crash-simulated create (commit, skip checkpoint) whose
// transaction is realized only by replaying the journal on the next mount.
static void testWriteJournal(const char* path, const char* dirtyPath) {
    std::printf("== write-side journaling (%s) ==\n", path);

    {
        FILE* f = std::fopen(path, "r+b");
        if (!f) { std::printf("  FAIL: open %s\n", path); ++g_failures; ++g_checks; return; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        FileBlockDevice dev(f, static_cast<uint64_t>(sz));
        Ext4FS fs(&dev);
        CHECK(fs.mount("/") == 0);
        CHECK(fs.journalWriteEnabled());   // simple journal -> write-journaling on
        VNode* root = fs.getRoot();

        // Normal journaled op: commit + checkpoint (lands on the final fs).
        VNode* a = nullptr;
        CHECK(root && root->ops->create(root, "jwn.txt", 0644, &a) == 0);
        if (a) freeVNode(a);

        // Crash simulation: commit the transaction to the journal but skip the
        // checkpoint, so "jwc.txt" exists only in the journal, not on the final fs.
        fs.setJournalSkipCheckpoint(true);
        VNode* b = nullptr;
        CHECK(root && root->ops->create(root, "jwc.txt", 0644, &b) == 0);
        if (b) freeVNode(b);
        fs.setJournalSkipCheckpoint(false);

        std::fclose(f);
    }

    // Snapshot the committed-not-checkpointed state for the Linux cross-check.
    CHECK(copyFile(path, dirtyPath));

    // Re-mount: journalReplay realizes the crash-sim transaction.
    {
        FILE* g = std::fopen(path, "r+b");
        CHECK(g != nullptr);
        if (!g) return;
        fseek(g, 0, SEEK_END); long sz = ftell(g); fseek(g, 0, SEEK_SET);
        FileBlockDevice dev(g, static_cast<uint64_t>(sz));
        Ext4FS fs(&dev);
        CHECK(fs.mount("/") == 0);
        VNode* root = fs.getRoot();
        VNode* jwn = root ? root->ops->lookup(root, "jwn.txt") : nullptr;   // checkpointed
        CHECK(jwn != nullptr);
        if (jwn) freeVNode(jwn);
        VNode* jwc = root ? root->ops->lookup(root, "jwc.txt") : nullptr;   // replayed
        CHECK(jwc != nullptr);
        if (jwc) freeVNode(jwc);
        std::fclose(g);
    }
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "build/ext4-fixtures";
    char ext4Path[512], ext2Path[512], ext4defPath[512];
    std::snprintf(ext4Path, sizeof(ext4Path), "%s/ext4.img", dir);
    std::snprintf(ext2Path, sizeof(ext2Path), "%s/ext2.img", dir);
    std::snprintf(ext4defPath, sizeof(ext4defPath), "%s/ext4def.img", dir);

    std::printf("== Ext4FS read/write tests ==\n");

    // crc32c sanity: the standard CRC-32C check value of "123456789" is
    // 0xE3069283 (with the final XOR); the running form used here is its
    // complement, 0x1CF96D7C.
    CHECK(ext4::crc32c(0xFFFFFFFFu, "123456789", 9) == 0x1CF96D7Cu);

    testImage(ext4Path, "ext4 / extents / 4 KiB / 32-bit desc", false, true);
    testImage(ext2Path, "ext2 / block-map / 1 KiB", false, false);
    testImage(ext4defPath, "ext4 default / 64-bit + flex_bg + csum + HTree", true, true);

    // Negative probe: a device without an ext4 superblock must be rejected so
    // the boot-time dispatch (main.cpp) correctly falls through to FAT instead
    // of misidentifying a non-ext4 volume as ext4.
    std::printf("== probe rejects non-ext4 ==\n");
    {
        uint8_t buf[2048];
        std::memset(buf, 0, sizeof(buf));
        MemBlockDevice zeros(buf, sizeof(buf));
        CHECK(!Ext4FS::probe(&zeros));
        // A FAT-style 0xAA55 boot signature without the ext4 magic is not ext4.
        buf[510] = 0x55;
        buf[511] = 0xAA;
        MemBlockDevice fatish(buf, sizeof(buf));
        CHECK(!Ext4FS::probe(&fatish));
    }

    char ext2wPath[512], ext4wPath[512], ext4cPath[512];
    std::snprintf(ext2wPath, sizeof(ext2wPath), "%s/ext2w.img", dir);
    std::snprintf(ext4wPath, sizeof(ext4wPath), "%s/ext4w.img", dir);
    std::snprintf(ext4cPath, sizeof(ext4cPath), "%s/ext4c.img", dir);
    testWriteImage(ext2wPath);
    testWriteImage(ext4wPath);
    testWriteImage(ext4cPath);   // metadata_csum (default ext4)

    char ext4jPath[512], ext4jDirty[512];
    std::snprintf(ext4jPath, sizeof(ext4jPath), "%s/ext4j.img", dir);
    std::snprintf(ext4jDirty, sizeof(ext4jDirty), "%s/ext4j_dirty.img", dir);
    testJournalRecovery(ext4jPath, ext4jDirty);

    char ext4j2Path[512], ext4j2Dirty[512];
    std::snprintf(ext4j2Path, sizeof(ext4j2Path), "%s/ext4j2.img", dir);
    std::snprintf(ext4j2Dirty, sizeof(ext4j2Dirty), "%s/ext4j2_dirty.img", dir);
    testWriteJournal(ext4j2Path, ext4j2Dirty);

    // P1.4 random-op fuzzer on both feature sets (simple + metadata_csum default),
    // several seeds each; run-ext4-fixtures.sh e2fsck's the results.
    char fuzzPath[512], fuzzcPath[512];
    std::snprintf(fuzzPath, sizeof(fuzzPath), "%s/ext4fuzz.img", dir);
    std::snprintf(fuzzcPath, sizeof(fuzzcPath), "%s/ext4fuzzc.img", dir);
    for (uint32_t s = 1; s <= 3; ++s) testFuzz(fuzzPath, s * 2654435761u, 3000);
    for (uint32_t s = 1; s <= 3; ++s) testFuzz(fuzzcPath, s * 40503u + 7, 3000);

    char htreePath[512];
    std::snprintf(htreePath, sizeof(htreePath), "%s/ext4hw.img", dir);
    testHtreeWrite(htreePath);

    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("EXT4 READ TESTS PASSED\n");
        return 0;
    }
    std::printf("EXT4 READ TESTS FAILED\n");
    return 1;
}
