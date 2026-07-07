// Headless P4 binutils oracle (/bin/login). Spawns /bin/hello -- a program
// assembled by GNU `as` and linked by GNU `ld` (the x86_64-unknown-instantos
// cross tools) against mlibc -- and verifies it loads (ld-instantos accepts the
// GNU-ld output), runs, and prints its marker. Reports GCC_CC_* via serial.

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n, u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

static i64 spawn_wait(const char* path, const char* const* argv, const char* const* envp){
    u64 pid=sc3(SYS_SPAWN,(u64)path,(u64)argv,(u64)envp);
    if((i64)pid<0||pid==(u64)-1) return -1;
    int status=0;
    sc3(SYS_WAIT,pid,(u64)&status,0);
    return (i64)(int)status;
}

static void stdout_to_file(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ sc2(SYS_DUP2,(u64)fd,1); if(fd!=1) sc1(SYS_CLOSE,(u64)fd); }
}

static int file_matches(const char* path, const char* expected){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0;
    char buf[512]; i64 total=0,n;
    while(total<(i64)sizeof(buf) && (n=(i64)sc3(SYS_READ,(u64)fd,(u64)(buf+total),(u64)(sizeof(buf)-(u64)total)))>0) total+=n;
    sc1(SYS_CLOSE,(u64)fd);
    unsigned elen=slen(expected);
    if((unsigned)total!=elen) return 0;
    for(unsigned i=0;i<elen;i++) if(buf[i]!=expected[i]) return 0;
    return 1;
}

static const char* const ENVP[]={"PATH=/bin","HOME=/","TMPDIR=/tmp","SHELL=/bin/sh",0};
static const char* const ARGV_HELLO[]={"hello",0};
static const char* HELLO_MSG = "HELLO_FROM_GNU_AS_LD\n";

void _start(void){
    serial("\n[GCC_CC] start (run GNU as+ld hello)\n");

    // Capture hello's stdout so we can check what it printed. The driver's own
    // progress goes to serial (syscall 88), so redirecting fd1 is harmless here.
    stdout_to_file("/tmp/hello.out");
    i64 rc = spawn_wait("/bin/hello", ARGV_HELLO, ENVP);
    serial("GCC_CC_HELLO_RC="); put_dec(rc); serial("\n");

    int out_ok = file_matches("/tmp/hello.out", HELLO_MSG);
    serial(out_ok ? "GCC_CC_HELLO_OUTPUT_OK\n" : "GCC_CC_HELLO_OUTPUT_FAIL\n");

    if (rc == 0 && out_ok) serial("GCC_CC_ALL_OK\n");
    serial("GCC_CC_DONE\n");
    sc1(SYS_EXIT,0);
}
