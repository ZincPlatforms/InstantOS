// Headless "buildworld" smoke driver (bash-free, direct spawn).
//
// Proves GNU make runs on InstantOS driving a real compile: it writes a small
// multi-file C project to /tmp/proj, runs /bin/make (which forks recipes that
// invoke /bin/tcc), then checks the produced binary is an ELF and runs with the
// expected exit status. It then edits one source and re-runs make to prove
// incremental, mtime-based rebuilds work.
//
// make's recipe children inherit stdout/stderr, which we redirect to
// /tmp/bw.log; the log is dumped to serial at the end. Progress is reported via
// BUILDWORLD_* serial markers (syscall 88).

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

void* memset(void* d,int c,u64 n){unsigned char* p=(unsigned char*)d;for(u64 i=0;i<n;i++)p[i]=(unsigned char)c;return d;}
void* memcpy(void* d,const void* s,u64 n){unsigned char* a=(unsigned char*)d;const unsigned char* b=(const unsigned char*)s;for(u64 i=0;i<n;i++)a[i]=b[i];return d;}

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

static i64 g_logfd = -1;
// Point child stdout/stderr at /tmp/bw.log and remember the fd so a temporary
// per-tool stdout redirect (e.g. gzip -dc > /tmp/proj.tar) can be undone.
static void open_log(void){
    g_logfd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bw.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(g_logfd>=0){ sc2(SYS_DUP2,(u64)g_logfd,1); sc2(SYS_DUP2,(u64)g_logfd,2); }
}
static void stdout_to_log(void){ if(g_logfd>=0) sc2(SYS_DUP2,(u64)g_logfd,1); }
static void stdout_to_file(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ sc2(SYS_DUP2,(u64)fd,1); if(fd!=1) sc1(SYS_CLOSE,(u64)fd); }
}
static int file_exists(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0; sc1(SYS_CLOSE,(u64)fd); return 1;
}

static int write_file(const char* path, const char* data){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return -1;
    u64 n=slen(data), off=0;
    while(off<n){ i64 w=(i64)sc3(SYS_WRITE,(u64)fd,(u64)(data+off),n-off); if(w<=0){ sc1(SYS_CLOSE,(u64)fd); return -1; } off+=(u64)w; }
    sc1(SYS_CLOSE,(u64)fd);
    return 0;
}

static i64 spawn_wait(const char* path, const char* const* argv, const char* const* envp){
    u64 pid=sc3(SYS_SPAWN,(u64)path,(u64)argv,(u64)envp);
    if((i64)pid<0||pid==(u64)-1) return -1;
    int status=0;
    sc3(SYS_WAIT,pid,(u64)&status,0);
    return (i64)(int)status;
}

static int is_elf(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0;
    char m[4]={0};
    i64 n=(i64)sc3(SYS_READ,(u64)fd,(u64)m,4);
    sc1(SYS_CLOSE,(u64)fd);
    return n==4 && m[0]==0x7f && m[1]=='E' && m[2]=='L' && m[3]=='F';
}

static void dump_log(void){
    serial("\n---BWLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bw.log",O_RDONLY,0);
    if(fd>=0){ char buf[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

static const char* const ENVP[]={"PATH=/bin","HOME=/","TMPDIR=/tmp","SHELL=/bin/sh",0};

// The project (main.c/a.c/b.c/Makefile.in/configure) ships as a gzipped tar in
// the initrd (/bin/proj.tar.gz); the launcher unpacks and configures it in-OS.
// main() returns a()+b()-7; with a()=3,b()=4 the app exits 0. The incremental
// test rewrites a.c so a()=10 (app then exits 7).
static const char* A_C_V2 = "int a(void){ return 10; }\n";

static const char* const ARGV_GUNZIP[]    = {"gzip","-dc","/bin/proj.tar.gz",0};
static const char* const ARGV_UNTAR[]     = {"tar","-xf","/tmp/proj.tar","-C","/tmp",0};
static const char* const ARGV_CONFIGURE[] = {"sh","-c","cd /tmp/proj && sh configure",0};
static const char* const ARGV_MAKE[]      = {"make","-C","/tmp/proj",0};
static const char* const ARGV_MAKE_J2[]   = {"make","-C","/tmp/proj","-j2",0};
static const char* const ARGV_APP[]       = {"app",0};
static const char* const ARGV_FDTEST[]    = {"/bin/fdtest",0};

void _start(void){
    serial("\n[BUILDWORLD] start (untar + configure + make + tcc)\n");
    open_log();

    // --- unpack a gzipped tarball: gzip -dc /bin/proj.tar.gz > /tmp/proj.tar,
    //     then tar -x it into /tmp (yielding /tmp/proj). Exercises gzip + tar. ---
    serial("[BUILDWORLD] gzip -dc /bin/proj.tar.gz > /tmp/proj.tar\n");
    stdout_to_file("/tmp/proj.tar");
    i64 rc = spawn_wait("/bin/gzip", ARGV_GUNZIP, ENVP);
    stdout_to_log();
    serial("BUILDWORLD_GUNZIP_RC="); put_dec(rc); serial("\n");
    serial("[BUILDWORLD] tar -xf /tmp/proj.tar -C /tmp\n");
    rc = spawn_wait("/bin/tar", ARGV_UNTAR, ENVP);
    serial("BUILDWORLD_UNTAR_RC="); put_dec(rc); serial("\n");
    serial(file_exists("/tmp/proj/configure") && file_exists("/tmp/proj/Makefile.in")
           ? "BUILDWORLD_UNTAR_OK\n" : "BUILDWORLD_UNTAR_FAIL\n");

    // --- configure-lite: a shell script runs a tcc compile-test, a grep-based
    //     version check, and sed-substitutes Makefile.in -> Makefile. Exercises
    //     bash + sed + grep + tcc and the build environment (PATH/HOME/...). ---
    serial("[BUILDWORLD] ./configure (sh + sed + grep + tcc)\n");
    rc = spawn_wait("/bin/sh", ARGV_CONFIGURE, ENVP);
    serial("BUILDWORLD_CONFIGURE_RC="); put_dec(rc); serial("\n");
    serial(file_exists("/tmp/proj/Makefile") ? "BUILDWORLD_MAKEFILE_OK\n" : "BUILDWORLD_MAKEFILE_FAIL\n");

    // Build with make (recipes fork /bin/tcc).
    serial("[BUILDWORLD] make -C /tmp/proj\n");
    rc = spawn_wait("/bin/make", ARGV_MAKE, ENVP);
    serial("BUILDWORLD_MAKE_RC="); put_dec(rc); serial("\n");
    serial(is_elf("/tmp/proj/app") ? "BUILDWORLD_APP_ELF_OK\n" : "BUILDWORLD_APP_ELF_BAD\n");

    // Run the built binary; expect exit status 0 (3+4-7).
    rc = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("BUILDWORLD_APP_RUN_RC="); put_dec(rc); serial("\n");

    // Incremental rebuild: edit a.c (a()=10). make must rebuild a.o + app by
    // mtime; the app now exits 7. Sleep first so the edit lands in a strictly
    // later wall-clock second than the just-built objects (file mtimes have
    // 1-second granularity), which is what make compares against.
    serial("[BUILDWORLD] sleep, edit a.c, re-run make (incremental)\n");
    sc1(SYS_SLEEP, 1500);
    write_file("/tmp/proj/a.c", A_C_V2);
    rc = spawn_wait("/bin/make", ARGV_MAKE, ENVP);
    serial("BUILDWORLD_INCR_MAKE_RC="); put_dec(rc); serial("\n");
    rc = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("BUILDWORLD_INCR_APP_RC="); put_dec(rc); serial("\n");

    // fd-inheritance across exec (P3.2): /bin/fdtest opens fd 3, execs itself,
    // and the child reads fd 3 (prints FDINHERIT_* markers via serial itself).
    serial("[BUILDWORLD] fd-inheritance test\n");
    spawn_wait("/bin/fdtest", ARGV_FDTEST, ENVP);

    // Parallel build (make -j2): remove the objects and rebuild concurrently.
    // Exercises make's job scheduling (fifo jobserver + concurrent recipe
    // fork/exec/wait). a.c is now V2, so the app exits 7.
    serial("[BUILDWORLD] make -j2 (parallel rebuild)\n");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/app");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/main.o");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/a.o");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/b.o");
    rc = spawn_wait("/bin/make", ARGV_MAKE_J2, ENVP);
    serial("BUILDWORLD_J2_MAKE_RC="); put_dec(rc); serial("\n");
    serial(is_elf("/tmp/proj/app") ? "BUILDWORLD_J2_APP_ELF_OK\n" : "BUILDWORLD_J2_APP_ELF_BAD\n");
    rc = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("BUILDWORLD_J2_APP_RC="); put_dec(rc); serial("\n");

    dump_log();
    serial("BUILDWORLD_DONE\n");
    sc1(SYS_EXIT,0);
}
