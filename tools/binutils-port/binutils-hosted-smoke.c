// Headless P4 Step-B oracle (/bin/login): exercises the HOSTED binutils
// (as/ld/ar/ranlib/nm/strip built to RUN on InstantOS). It assembles a shipped
// hello.s with in-OS `as`, links it with in-OS `ld` (+ mlibc crt1 + -lc), runs
// the result, round-trips an archive with ar+ranlib, lists symbols with nm, and
// strips the binary -- reporting BINUTILSHOST_* markers over serial (syscall 88).

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n, u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

// clang lowers some aggregate inits to memset/memcpy; provide them (freestanding).
void* memset(void* d,int c,u64 n){unsigned char* p=(unsigned char*)d;for(u64 i=0;i<n;i++)p[i]=(unsigned char)c;return d;}
void* memcpy(void* d,const void* s,u64 n){unsigned char* a=(unsigned char*)d;const unsigned char* b=(const unsigned char*)s;for(u64 i=0;i<n;i++)a[i]=b[i];return d;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

static i64 g_logfd = -1;
static void open_log(void){ g_logfd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bh.log",O_WRONLY|O_CREAT|O_TRUNC,0644); if(g_logfd>=0){sc2(SYS_DUP2,(u64)g_logfd,1);sc2(SYS_DUP2,(u64)g_logfd,2);} }
static void stdout_to_log(void){ if(g_logfd>=0) sc2(SYS_DUP2,(u64)g_logfd,1); }
static void stdout_to_file(const char* path){ i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_WRONLY|O_CREAT|O_TRUNC,0644); if(fd>=0){sc2(SYS_DUP2,(u64)fd,1); if(fd!=1) sc1(SYS_CLOSE,(u64)fd);} }

static i64 spawn_wait(const char* path, const char* const* argv, const char* const* envp){
    u64 pid=sc3(SYS_SPAWN,(u64)path,(u64)argv,(u64)envp);
    if((i64)pid<0||pid==(u64)-1) return -1;
    int status=0; sc3(SYS_WAIT,pid,(u64)&status,0);
    return (i64)(int)status;
}
static int is_elf(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0); if(fd<0) return 0;
    char m[4]={0}; i64 n=(i64)sc3(SYS_READ,(u64)fd,(u64)m,4); sc1(SYS_CLOSE,(u64)fd);
    return n==4 && m[0]==0x7f && m[1]=='E' && m[2]=='L' && m[3]=='F';
}
static int file_matches(const char* path, const char* expected){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0); if(fd<0) return 0;
    char buf[512]; i64 total=0,n;
    while(total<(i64)sizeof(buf) && (n=(i64)sc3(SYS_READ,(u64)fd,(u64)(buf+total),(u64)(sizeof(buf)-(u64)total)))>0) total+=n;
    sc1(SYS_CLOSE,(u64)fd);
    unsigned elen=slen(expected); if((unsigned)total!=elen) return 0;
    for(unsigned i=0;i<elen;i++) if(buf[i]!=expected[i]) return 0;
    return 1;
}
// substring search in a (small) file's contents
static int file_contains(const char* path, const char* needle){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0); if(fd<0) return 0;
    static char buf[8192]; i64 total=0,n;
    while(total<(i64)sizeof(buf)-1 && (n=(i64)sc3(SYS_READ,(u64)fd,(u64)(buf+total),(u64)(sizeof(buf)-1-(u64)total)))>0) total+=n;
    sc1(SYS_CLOSE,(u64)fd); buf[total]=0;
    unsigned nl=slen(needle); if(nl==0) return 1;
    for(i64 i=0;i+(i64)nl<=total;i++){ unsigned j=0; while(j<nl && buf[i+j]==needle[j]) j++; if(j==nl) return 1; }
    return 0;
}
static void dump_log(void){
    serial("\n---BHLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bh.log",O_RDONLY,0);
    if(fd>=0){ char b[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)b,sizeof(b)))>0) serial_n(b,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

static const char* const ENVP[]={"PATH=/bin","HOME=/","TMPDIR=/tmp","SHELL=/bin/sh",0};
static const char* const A_AS[]     = {"as","/bin/hello.s","-o","/tmp/hello.o",0};
static const char* const A_LD[]     = {"ld","-pie","-z","now","-rpath","/lib/mlibc","/lib/mlibc/crt1.o","/tmp/hello.o","-L","/lib/mlibc","-lc","-o","/tmp/hello",0};
static const char* const A_HELLO[]  = {"hello",0};
static const char* const A_AR_C[]   = {"ar","rcs","/tmp/lib.a","/tmp/hello.o",0};
static const char* const A_RANLIB[] = {"ranlib","/tmp/lib.a",0};
static const char* const A_AR_T[]   = {"ar","t","/tmp/lib.a",0};
static const char* const A_NM[]     = {"nm","/tmp/hello.o",0};
static const char* const A_STRIP[]  = {"strip","/tmp/hello",0};

void _start(void){
    serial("\n[BINUTILSHOST] start (in-OS as/ld/ar/ranlib/nm/strip)\n");
    open_log();

    // 1. as: assemble the shipped hello.s -> hello.o
    i64 rc = spawn_wait("/bin/as", A_AS, ENVP);
    serial("BINUTILSHOST_AS_RC="); put_dec(rc); serial("\n");
    int as_ok = (rc==0) && is_elf("/tmp/hello.o");   // hello.o is a relocatable ELF
    serial(as_ok ? "BINUTILSHOST_AS_OK\n" : "BINUTILSHOST_AS_FAIL\n");

    // 2. ld: link hello.o + mlibc crt1 + -lc -> hello (in-OS GNU ld!)
    rc = spawn_wait("/bin/ld", A_LD, ENVP);
    serial("BINUTILSHOST_LD_RC="); put_dec(rc); serial("\n");
    int ld_ok = (rc==0) && is_elf("/tmp/hello");
    serial(ld_ok ? "BINUTILSHOST_LD_ELF_OK\n" : "BINUTILSHOST_LD_ELF_BAD\n");

    // 3. run the in-OS-linked binary; expect the hello marker + exit 0
    stdout_to_file("/tmp/hello.out");
    rc = spawn_wait("/tmp/hello", A_HELLO, ENVP);
    stdout_to_log();
    int run_ok = (rc==0) && file_matches("/tmp/hello.out", "HELLO_FROM_GNU_AS_LD\n");
    serial("BINUTILSHOST_RUN_RC="); put_dec(rc); serial("\n");
    serial(run_ok ? "BINUTILSHOST_RUN_OK\n" : "BINUTILSHOST_RUN_FAIL\n");

    // 4. ar + ranlib round-trip: build an archive, index it, list it
    i64 arc = spawn_wait("/bin/ar", A_AR_C, ENVP);
    i64 rlc = spawn_wait("/bin/ranlib", A_RANLIB, ENVP);
    stdout_to_file("/tmp/ar.out");
    spawn_wait("/bin/ar", A_AR_T, ENVP);
    stdout_to_log();
    int ar_ok = (arc==0) && (rlc==0) && file_contains("/tmp/ar.out", "hello.o");
    serial(ar_ok ? "BINUTILSHOST_AR_OK\n" : "BINUTILSHOST_AR_FAIL\n");

    // 5. nm: hello.o should list the 'main' symbol
    stdout_to_file("/tmp/nm.out");
    spawn_wait("/bin/nm", A_NM, ENVP);
    stdout_to_log();
    int nm_ok = file_contains("/tmp/nm.out", "main");
    serial(nm_ok ? "BINUTILSHOST_NM_OK\n" : "BINUTILSHOST_NM_FAIL\n");

    // 6. strip: shrink the binary; it must remain a valid ELF
    rc = spawn_wait("/bin/strip", A_STRIP, ENVP);
    int strip_ok = (rc==0) && is_elf("/tmp/hello");
    serial("BINUTILSHOST_STRIP_RC="); put_dec(rc); serial("\n");
    serial(strip_ok ? "BINUTILSHOST_STRIP_OK\n" : "BINUTILSHOST_STRIP_FAIL\n");

    dump_log();
    int score = as_ok + ld_ok + run_ok + ar_ok + nm_ok + strip_ok;
    serial("BINUTILSHOST_SCORE="); put_dec(score); serial("/6\n");
    if (score==6) serial("BINUTILSHOST_ALL_OK\n");
    serial("BINUTILSHOST_DONE\n");
    sc1(SYS_EXIT,0);
}
