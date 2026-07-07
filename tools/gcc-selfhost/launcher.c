// Phase 7 P7.0 - in-OS GCC build-environment oracle (bash-free driver).
//
// Proves the `./configure && make` build pipeline works in-OS with the HOSTED
// gcc (on the gcchost ext4 at /usr/bin/gcc) driving GNU make + /bin/sh + the
// coreutils ports (sed/grep/tar/gzip). This is the foundation the binutils and
// GCC bootstrap rungs (P7.1/P7.2) build on.
//
//   unpack  : gzip -dc proj.tar.gz | tar -x  -> /tmp/proj            (gzip+tar)
//   configure: sh configure  (gcc compile+version probe, sed Makefile) (sh+sed+grep+gcc)
//   make    : make -C /tmp/proj  (recipes fork /bin/sh -> gcc)         (make+gcc)
//   run     : /tmp/proj/app exits 0 (a()+b()-7 == 0)
//   incr    : edit a.c (a()=10), make rebuilds by mtime, app exits 7
//   -j2     : parallel rebuild after removing objects
//
// Child stdout/stderr -> /tmp/gs.log (dumped to serial at the end). Progress via
// GCCSELF_* serial markers (syscall 88). GCCSELF_MAKE_OK == P7.0 green.

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
static void open_log(void){
    g_logfd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gs.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
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
// Decode the child exit code from a wait status (the kernel encodes it as
// code<<8). spawn_wait returns the raw status; use this when the exit *value*
// matters (e.g. the app returns 0 then 7).
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
    serial("\n---GSLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gs.log",O_RDONLY,0);
    if(fd>=0){ char buf[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

// gcc lives on the ext4 (/usr/bin); the make/sh/coreutils ports live on the
// initrd (/bin). Both dirs on PATH so `make` recipes ("gcc -c ...") resolve.
static const char* const ENVP[]={"PATH=/usr/bin:/bin","HOME=/","TMPDIR=/tmp","SHELL=/bin/sh",0};

static const char* A_C_V2 = "int a(void){ return 10; }\n";

static const char* const ARGV_GUNZIP[]    = {"gzip","-dc","/bin/proj.tar.gz",0};
static const char* const ARGV_UNTAR[]     = {"tar","-xf","/tmp/proj.tar","-C","/tmp",0};
static const char* const ARGV_CONFIGURE[] = {"sh","-c","cd /tmp/proj && sh configure",0};
static const char* const ARGV_MAKE[]      = {"make","-C","/tmp/proj",0};
static const char* const ARGV_MAKE_J2[]   = {"make","-C","/tmp/proj","-j2",0};
static const char* const ARGV_APP[]       = {"app",0};

void _start(void){
    serial("\n[GCCSELF] start: in-OS gcc build-environment oracle (P7.0)\n");
    open_log();

    serial("[GCCSELF] gzip -dc /bin/proj.tar.gz > /tmp/proj.tar\n");
    stdout_to_file("/tmp/proj.tar");
    i64 rc = spawn_wait("/bin/gzip", ARGV_GUNZIP, ENVP);
    stdout_to_log();
    serial("GCCSELF_GUNZIP_RC="); put_dec(rc); serial("\n");
    serial("[GCCSELF] tar -xf /tmp/proj.tar -C /tmp\n");
    rc = spawn_wait("/bin/tar", ARGV_UNTAR, ENVP);
    serial("GCCSELF_UNTAR_RC="); put_dec(rc); serial("\n");
    serial(file_exists("/tmp/proj/configure") && file_exists("/tmp/proj/Makefile.in")
           ? "GCCSELF_UNTAR_OK\n" : "GCCSELF_UNTAR_FAIL\n");

    serial("[GCCSELF] ./configure (sh + sed + grep + gcc)\n");
    rc = spawn_wait("/bin/sh", ARGV_CONFIGURE, ENVP);
    serial("GCCSELF_CONFIGURE_RC="); put_dec(rc); serial("\n");
    serial(file_exists("/tmp/proj/Makefile") ? "GCCSELF_MAKEFILE_OK\n" : "GCCSELF_MAKEFILE_FAIL\n");

    serial("[GCCSELF] make -C /tmp/proj (recipes fork gcc)\n");
    rc = spawn_wait("/bin/make", ARGV_MAKE, ENVP);
    serial("GCCSELF_MAKE_RC="); put_dec(rc); serial("\n");
    int app_elf = is_elf("/tmp/proj/app");
    serial(app_elf ? "GCCSELF_APP_ELF_OK\n" : "GCCSELF_APP_ELF_BAD\n");

    i64 run = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("GCCSELF_APP_RUN_RC="); put_dec(run); serial("\n");

    serial("[GCCSELF] sleep, edit a.c, re-make (incremental mtime rebuild)\n");
    sc1(SYS_SLEEP, 1500);
    write_file("/tmp/proj/a.c", A_C_V2);
    i64 incr = spawn_wait("/bin/make", ARGV_MAKE, ENVP);
    serial("GCCSELF_INCR_MAKE_RC="); put_dec(incr); serial("\n");
    i64 incr_run = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("GCCSELF_INCR_APP_RC="); put_dec(incr_run); serial("\n");   // expect 7

    serial("[GCCSELF] make -j2 (parallel rebuild)\n");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/app");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/main.o");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/a.o");
    sc1(SYS_UNLINK, (u64)"/tmp/proj/b.o");
    i64 j2 = spawn_wait("/bin/make", ARGV_MAKE_J2, ENVP);
    serial("GCCSELF_J2_MAKE_RC="); put_dec(j2); serial("\n");
    int j2_elf = is_elf("/tmp/proj/app");
    serial(j2_elf ? "GCCSELF_J2_APP_ELF_OK\n" : "GCCSELF_J2_APP_ELF_BAD\n");
    i64 j2_run = spawn_wait("/tmp/proj/app", ARGV_APP, ENVP);
    serial("GCCSELF_J2_APP_RC="); put_dec(j2_run); serial("\n");

    // P7.0: gcc + make + sh drive a full clean build (app exits 0), an
    // incremental mtime rebuild (app now exits 7), and a parallel -j2 rebuild
    // (also 7) -- the in-OS build environment works. rc/incr/j2 are make's own
    // statuses (exit 0 == status 0); the app checks decode the exit value.
    if(rc==0 && app_elf && exit_code(run)==0 && incr==0 && exit_code(incr_run)==7 &&
       j2==0 && j2_elf && exit_code(j2_run)==7)
        serial("GCCSELF_MAKE_OK\n");

    dump_log();
    serial("GCCSELF_DONE\n");
    sc1(SYS_EXIT,0);
}
