// Phase 7 P7.1 - in-OS gcc rebuilds binutils (bash-free driver).
//
// The HOSTED gcc on the gcchost ext4 (/usr/bin/gcc) + the GNU userland ports on
// the initrd (/bin: sh make sed grep tar gzip awk cmp + coreutils) rebuild GNU
// binutils-2.42 from source, ON InstantOS:
//
//   untar    : gzip -dc /bin/binutils-insrc.tar.gz | tar -x  -> /tmp/binutils-2.42
//   configure: native (build=host=target=x86_64-unknown-instantos); huge sh/sed/
//              grep/awk/expr + gcc compile+link probe pass across bfd/opcodes/gas/ld
//   make     : compile the whole tree with the in-OS gcc -> gas/as-new, ld/ld-new
//   sanity   : run the freshly built as/ld (--version) in-OS
//
// Existing as/ar/ranlib (hosted binutils in /usr/bin) bootstrap the new build.
// Child stdout/stderr -> /tmp/gs.log (dumped to serial at the end). Progress via
// GCCSELF_* serial markers. GCCSELF_BINUTILS_OK == P7.1 green.

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n, u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_SLEEP=15, SYS_MKDIR=32, SYS_UNLINK=34, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

static i64 g_logfd = -1;
static void open_log(void){
    g_logfd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gs.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(g_logfd>=0){ sc2(SYS_DUP2,(u64)g_logfd,1); sc2(SYS_DUP2,(u64)g_logfd,2); }
}
static int file_exists(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0; sc1(SYS_CLOSE,(u64)fd); return 1;
}
static i64 spawn_wait(const char* path, const char* const* argv, const char* const* envp){
    u64 pid=sc3(SYS_SPAWN,(u64)path,(u64)argv,(u64)envp);
    if((i64)pid<0||pid==(u64)-1) return -1;
    int status=0;
    sc3(SYS_WAIT,pid,(u64)&status,0);
    return (i64)(int)status;
}
static i64 exit_code(i64 status){ return (status>>8)&0xFF; }
static int is_elf(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0;
    char m[4]={0};
    i64 n=(i64)sc3(SYS_READ,(u64)fd,(u64)m,4);
    sc1(SYS_CLOSE,(u64)fd);
    return n==4 && m[0]==0x7f && m[1]=='E' && m[2]=='L' && m[3]=='F';
}
static void dump_log(void){
    serial("\n---GSLOG(tail)---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gs.log",O_RDONLY,0);
    if(fd>=0){ char buf[2048]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

// gcc + as/ar/ranlib on the ext4 (/usr/bin); the GNU userland ports on the
// initrd (/bin). Both on PATH so configure/make find every tool.
static const char* const ENVP[]={
    "PATH=/usr/bin:/bin","HOME=/","TMPDIR=/tmp","SHELL=/bin/sh","CONFIG_SHELL=/bin/sh",
    "MAKEINFO=true","LC_ALL=C","LANG=C",0
};

static const char* const ARGV_UNTAR[] = {"sh","-c",
    "gzip -dc /bin/binutils-insrc.tar.gz | tar -x -C /tmp",0};
static const char* const ARGV_CONFIGURE[] = {"sh","-c",
    "cd /tmp/bu-build && /tmp/binutils-2.42/configure "
    "--host=x86_64-unknown-instantos --build=x86_64-unknown-instantos "
    "--target=x86_64-unknown-instantos --prefix=/usr --disable-nls --disable-werror "
    "--without-zstd --without-debuginfod --without-msgpack --disable-gdb --disable-gold "
    "--disable-gprofng --disable-libctf --disable-plugins --enable-default-hash-style=sysv",0};
static const char* const ARGV_MAKE[] = {"sh","-c",
    "cd /tmp/bu-build && make -j2 MAKEINFO=true all-gas all-ld all-binutils",0};
static const char* const ARGV_AS_V[] = {"as-new","--version",0};
static const char* const ARGV_LD_V[] = {"ld-new","--version",0};

void _start(void){
    serial("\n[GCCSELF] start: in-OS gcc rebuilds binutils (P7.1)\n");
    open_log();

    serial("[GCCSELF] untar binutils-2.42 source\n");
    i64 rc = spawn_wait("/bin/sh", ARGV_UNTAR, ENVP);
    serial("GCCSELF_BU_UNTAR_RC="); put_dec(rc); serial("\n");
    int untar_ok = file_exists("/tmp/binutils-2.42/configure");
    serial(untar_ok ? "GCCSELF_BU_UNTAR_OK\n" : "GCCSELF_BU_UNTAR_FAIL\n");

    sc3(SYS_MKDIR,(u64)"/tmp/bu-build",0755,0);
    serial("[GCCSELF] configure (native instantos; sh+sed+grep+awk+gcc probes)\n");
    rc = spawn_wait("/bin/sh", ARGV_CONFIGURE, ENVP);
    serial("GCCSELF_BU_CONFIGURE_RC="); put_dec(rc); serial("\n");
    int cfg_ok = file_exists("/tmp/bu-build/Makefile") && file_exists("/tmp/bu-build/gas/Makefile")
              && file_exists("/tmp/bu-build/ld/Makefile");
    serial(cfg_ok ? "GCCSELF_BU_CONFIGURE_OK\n" : "GCCSELF_BU_CONFIGURE_FAIL\n");

    serial("[GCCSELF] make (in-OS gcc compiles bfd/opcodes/gas/ld/binutils)\n");
    rc = spawn_wait("/bin/sh", ARGV_MAKE, ENVP);
    serial("GCCSELF_BU_MAKE_RC="); put_dec(rc); serial("\n");
    int as_elf = is_elf("/tmp/bu-build/gas/as-new");
    int ld_elf = is_elf("/tmp/bu-build/ld/ld-new");
    serial(as_elf ? "GCCSELF_BU_AS_ELF_OK\n" : "GCCSELF_BU_AS_ELF_BAD\n");
    serial(ld_elf ? "GCCSELF_BU_LD_ELF_OK\n" : "GCCSELF_BU_LD_ELF_BAD\n");

    i64 as_run = -1, ld_run = -1;
    if(as_elf){ as_run = spawn_wait("/tmp/bu-build/gas/as-new", ARGV_AS_V, ENVP);
        serial("GCCSELF_BU_AS_RUN_RC="); put_dec(as_run); serial("\n"); }
    if(ld_elf){ ld_run = spawn_wait("/tmp/bu-build/ld/ld-new", ARGV_LD_V, ENVP);
        serial("GCCSELF_BU_LD_RUN_RC="); put_dec(ld_run); serial("\n"); }
    if(as_elf && exit_code(as_run)==0) serial("GCCSELF_BU_AS_OK\n");
    if(ld_elf && exit_code(ld_run)==0) serial("GCCSELF_BU_LD_OK\n");

    if(untar_ok && cfg_ok && rc==0 && as_elf && ld_elf &&
       exit_code(as_run)==0 && exit_code(ld_run)==0)
        serial("GCCSELF_BINUTILS_OK\n");

    dump_log();
    serial("GCCSELF_DONE\n");
    sc1(SYS_EXIT,0);
}
