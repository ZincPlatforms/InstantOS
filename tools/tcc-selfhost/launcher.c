// Headless TinyCC self-host driver, bash-free.
//
// Earlier versions drove /bin/tcc through interactive bash over a PTY, but
// bash's fork/exec path (GNU bash on mlibc) intermittently faults on InstantOS,
// which is unrelated to tcc and flaky. This launcher instead SYS_SPAWNs the
// compiler directly (the same spawn path the kernel uses everywhere) and waits,
// so the self-host is exercised over a clean, deterministic path:
//
//   stage2: /bin/tcc (clang-built, InstantOS-targeted) compiles the tcc 0.9.27
//           sources  ->  /tmp/tcc2
//   check : tcc2 is an ELF and runs (tcc2 -v)
//   stage3: tcc2 compiles the SAME sources -> /tmp/tcc3   (real self-host proof)
//
// Child stdout/stderr are redirected to /tmp/td.log (dup2 before spawn); the log
// is dumped to serial at the end so tcc's own diagnostics are visible.

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n, u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_SLEEP=15, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

// Freestanding: clang may lower aggregate zero-init to a memset call, and there
// is no libc linked in, so provide the primitives it expects.
void* memset(void* d,int c,u64 n){unsigned char* p=(unsigned char*)d;for(u64 i=0;i<n;i++)p[i]=(unsigned char)c;return d;}
void* memcpy(void* d,const void* s,u64 n){unsigned char* a=(unsigned char*)d;const unsigned char* b=(const unsigned char*)s;for(u64 i=0;i<n;i++)a[i]=b[i];return d;}

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

// Redirect the calling process's stdout+stderr to /tmp/td.log so spawned
// children (which inherit fds) log there.
static void redirect_child_output(void){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/td.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ sc2(SYS_DUP2,(u64)fd,1); sc2(SYS_DUP2,(u64)fd,2); if(fd!=1&&fd!=2) sc1(SYS_CLOSE,(u64)fd); }
}

// Spawn path with argv/envp, wait, return the child's exit status (kernel encodes
// a fatal signal as 128+signo, e.g. 139 = SIGSEGV).
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

static i64 file_size(const char* p){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)p,O_RDONLY,0);
    if(fd<0) return -1;
    char b[4096]; i64 tot=0,n;
    while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)b,sizeof(b)))>0) tot+=n;
    sc1(SYS_CLOSE,(u64)fd);
    return tot;
}

// Byte-exact comparison: the self-host fixpoint test. A correct self-hosting
// compiler recompiling itself yields a bit-identical binary (tcc2 == tcc3).
static int files_equal(const char* pa,const char* pb){
    i64 fa=(i64)sc3(SYS_OPEN,(u64)pa,O_RDONLY,0);
    i64 fb=(i64)sc3(SYS_OPEN,(u64)pb,O_RDONLY,0);
    if(fa<0||fb<0){ if(fa>=0)sc1(SYS_CLOSE,(u64)fa); if(fb>=0)sc1(SYS_CLOSE,(u64)fb); return -1; }
    char ba[4096], bb[4096]; int eq=1;
    for(;;){
        i64 na=(i64)sc3(SYS_READ,(u64)fa,(u64)ba,sizeof(ba));
        i64 nb=(i64)sc3(SYS_READ,(u64)fb,(u64)bb,sizeof(bb));
        if(na!=nb){ eq=0; break; }
        if(na<=0) break;
        for(i64 i=0;i<na;i++) if(ba[i]!=bb[i]){ eq=0; break; }
        if(!eq) break;
    }
    sc1(SYS_CLOSE,(u64)fa); sc1(SYS_CLOSE,(u64)fb);
    return eq;
}

static void dump_log(void){
    serial("\n---TCCLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/td.log",O_RDONLY,0);
    if(fd>=0){ char buf[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

static const char* const ENVP[]={"PATH=/bin","HOME=/","TMPDIR=/tmp",0};

// Build outputs land here. Default is /tmp (RamFS). The P1.4 ext4 variant builds
// with -DOUT_PREFIX="" so the compiler writes tcc2/tcc3/hello2 to the ext4 root,
// proving a real toolchain builds on the on-disk filesystem (then e2fsck-clean).
#ifndef OUT_PREFIX
#define OUT_PREFIX "/tmp"
#endif
#define P_TCC2  OUT_PREFIX "/tcc2"
#define P_TCC3  OUT_PREFIX "/tcc3"
#define P_HELLO OUT_PREFIX "/hello2"

// tcc self-compile argv (ONE_SOURCE=0: 10 core TUs compiled + linked with crt +
// libc). argv[0] and the output path differ per stage; the source list is shared.
#define TCC_SRCS \
    "/lib/tcc/src/tcc.c","/lib/tcc/src/libtcc.c","/lib/tcc/src/tccpp.c","/lib/tcc/src/tccgen.c", \
    "/lib/tcc/src/tccelf.c","/lib/tcc/src/tccasm.c","/lib/tcc/src/tccrun.c","/lib/tcc/src/x86_64-gen.c", \
    "/lib/tcc/src/x86_64-link.c","/lib/tcc/src/i386-asm.c"

static const char* const ARGV_STAGE2[]={
    "tcc","-DTCC_TARGET_X86_64","-DONE_SOURCE=0","-DCONFIG_TCC_STATIC","-DCONFIG_TCCBOOT","-I/lib/tcc/src",
    "-o",P_TCC2, TCC_SRCS, 0
};
static const char* const ARGV_STAGE3[]={
    "tcc2","-DTCC_TARGET_X86_64","-DONE_SOURCE=0","-DCONFIG_TCC_STATIC","-DCONFIG_TCCBOOT","-I/lib/tcc/src",
    "-o",P_TCC3, TCC_SRCS, 0
};
static const char* const ARGV_TCC2_V[]={"tcc2","-v",0};
static const char* const ARGV_TCC2_HELLO[]={"tcc2","/bin/tcc-hello.c","-o",P_HELLO,0};
static const char* const ARGV_HELLO2_RUN[]={"hello2",0};

void _start(void){
    serial("\n[SELFHOST] start (bash-free, direct spawn)\n");
    redirect_child_output();

    // stage2: build tcc2 with the clang-built /bin/tcc
    serial("[SELFHOST] stage2: /bin/tcc compiling tcc sources -> /tmp/tcc2\n");
    i64 rc=spawn_wait("/bin/tcc", ARGV_STAGE2, ENVP);
    serial("SELFHOST_STAGE2_RC="); put_dec(rc); serial("\n");
    serial(is_elf(P_TCC2)?"SELFHOST_TCC2_ELF_OK\n":"SELFHOST_TCC2_ELF_BAD\n");

    // tcc2 runs?
    rc=spawn_wait(P_TCC2, ARGV_TCC2_V, ENVP);
    serial("SELFHOST_TCC2_VERSION_RC="); put_dec(rc); serial("\n");

    // tcc2 compiles + links a small program?
    rc=spawn_wait(P_TCC2, ARGV_TCC2_HELLO, ENVP);
    serial("SELFHOST_HELLO_CC_RC="); put_dec(rc); serial("\n");
    serial(is_elf(P_HELLO)?"SELFHOST_HELLO_ELF_OK\n":"SELFHOST_HELLO_ELF_BAD\n");

    // stage3: tcc2 rebuilds the whole compiler -> /tmp/tcc3 (self-host fixpoint)
    serial("[SELFHOST] stage3: /tmp/tcc2 compiling tcc sources -> /tmp/tcc3\n");
    rc=spawn_wait(P_TCC2, ARGV_STAGE3, ENVP);
    serial("SELFHOST_STAGE3_RC="); put_dec(rc); serial("\n");
    serial(is_elf(P_TCC3)?"SELFHOST_TCC3_ELF_OK\n":"SELFHOST_TCC3_ELF_BAD\n");

    // Run the tcc2-built hello program (returns 0 on success).
    rc=spawn_wait(P_HELLO, ARGV_HELLO2_RUN, ENVP);
    serial("SELFHOST_HELLO_RUN_RC="); put_dec(rc); serial("\n");

    // Self-host fixpoint: sizes + byte-exact tcc2 vs tcc3.
    serial("SELFHOST_TCC2_SIZE="); put_dec(file_size(P_TCC2)); serial("\n");
    serial("SELFHOST_TCC3_SIZE="); put_dec(file_size(P_TCC3)); serial("\n");
    int eq=files_equal(P_TCC2,P_TCC3);
    serial(eq==1?"SELFHOST_FIXPOINT_OK\n":(eq==0?"SELFHOST_FIXPOINT_DIFF\n":"SELFHOST_FIXPOINT_ERR\n"));

    dump_log();
    serial("SELFHOST_DONE\n");
    sc1(SYS_EXIT,0);
}
