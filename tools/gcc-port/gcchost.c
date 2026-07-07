// P6.5/P6.6/P6.7: headless in-OS driver for the hosted GCC (/usr/bin/gcc,g++).
// bash-free, direct SYS_SPAWN (same pattern as tcc-selfhost/launcher.c). Runs as
// /bin/login. The gcc install tree lives on the ext4 root (/usr); runtime .so's
// (libc/libstdc++/libgcc_s/ld-instantos) are on the initrd /lib/mlibc.
//
//   GCCHOST_CC1_OK : gcc -v ok; gcc /hello.c -o /tmp/hello; /tmp/hello runs (rc 0)
//   GCCHOST_CXX_OK : g++ /hello.cpp (iostream+exceptions+threads) -> runs (rc 0)
// Child stdout/stderr -> /tmp/gcc.log, dumped to serial at the end.

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n,u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_SLEEP=15, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

void* memset(void* d,int c,u64 n){unsigned char* p=(unsigned char*)d;for(u64 i=0;i<n;i++)p[i]=(unsigned char)c;return d;}
void* memcpy(void* d,const void* s,u64 n){unsigned char* a=(unsigned char*)d;const unsigned char* b=(const unsigned char*)s;for(u64 i=0;i<n;i++)a[i]=b[i];return d;}

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}

static void redirect_child_output(void){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gcc.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ sc2(SYS_DUP2,(u64)fd,1); sc2(SYS_DUP2,(u64)fd,2); if(fd!=1&&fd!=2) sc1(SYS_CLOSE,(u64)fd); }
}

static i64 spawn_wait(const char* path,const char* const* argv,const char* const* envp){
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

// Byte-exact compare: the tcc self-host fixpoint proof (tcc2 == tcc3).
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
    serial("\n---GCCLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/gcc.log",O_RDONLY,0);
    if(fd>=0){ char buf[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

// gcc must find as/ld (PATH=/usr/bin) and spill temps to /tmp.
static const char* const ENVP[]={"PATH=/usr/bin","HOME=/","TMPDIR=/tmp",0};

static const char* const ARGV_GCC_V[]  ={"gcc","-v",0};
static const char* const ARGV_GCC_CC[] ={"gcc","/hello.c","-o","/tmp/hello",0};
static const char* const ARGV_HELLO[]  ={"hello",0};
static const char* const ARGV_GXX_CC[] ={"g++","/hello.cpp","-o","/tmp/hellocpp",0};
static const char* const ARGV_HELLOCPP[]={"hellocpp",0};

// ── P6.7: gcc rebuilds tcc from source; the gcc-built tcc self-hosts ────────
// The 10 core TinyCC 0.9.27 translation units (ONE_SOURCE=0), shipped at
// /lib/tcc/src with config.instantos.h so the produced tcc bakes the InstantOS
// (libinstant) search paths.
#define TCC_SRCS \
    "/lib/tcc/src/tcc.c","/lib/tcc/src/libtcc.c","/lib/tcc/src/tccpp.c","/lib/tcc/src/tccgen.c", \
    "/lib/tcc/src/tccelf.c","/lib/tcc/src/tccasm.c","/lib/tcc/src/tccrun.c","/lib/tcc/src/x86_64-gen.c", \
    "/lib/tcc/src/x86_64-link.c","/lib/tcc/src/i386-asm.c"

// In-OS gcc builds tcc against the libinstant tcc-sysroot (mirrors
// tools/build-tcc-gcc.sh): freestanding, ilibcxx (/include) + gcc builtin
// headers, crt0.o + -linstant, /lib/ld-instantos.so interpreter. Produces a
// libinstant-linked tcc that runs in-OS.
static const char* const ARGV_GCC_TCC[] = {
    "gcc","-DTCC_TARGET_X86_64","-DONE_SOURCE=0","-DCONFIG_TCC_STATIC","-DCONFIG_TCCBOOT",
    "-fPIC","-ffreestanding","-fno-stack-protector","-nostdinc",
    "-isystem","/include",
    "-isystem","/usr/lib/gcc/x86_64-unknown-instantos/13.3.0/include",
    "-I/lib/tcc/src",
    "-nostdlib","-Wl,--gc-sections","-Wl,--build-id=none","-Wl,--hash-style=sysv",
    "-Wl,-z,max-page-size=0x1000","-pie","-Wl,-e,_start","-Wl,--dynamic-linker,/lib/ld-instantos.so",
    "/lib/crt0.o", TCC_SRCS, "-L/lib","-linstant",
    "-o","/tmp/tcc_gcc", 0
};
static const char* const ARGV_TCC_V[]     = {"tcc_gcc","-v",0};
// tcc self-host: the compiler adds crt0/libinstant/interp itself from its baked
// config, so these stages only pass the target defines + include dir.
static const char* const ARGV_TCC_STAGE2[]= {
    "tcc_gcc","-DTCC_TARGET_X86_64","-DONE_SOURCE=0","-DCONFIG_TCC_STATIC","-DCONFIG_TCCBOOT","-I/lib/tcc/src",
    "-o","/tmp/tcc2", TCC_SRCS, 0
};
static const char* const ARGV_TCC_STAGE3[]= {
    "tcc2","-DTCC_TARGET_X86_64","-DONE_SOURCE=0","-DCONFIG_TCC_STATIC","-DCONFIG_TCCBOOT","-I/lib/tcc/src",
    "-o","/tmp/tcc3", TCC_SRCS, 0
};

void _start(void){
    serial("\n[GCCHOST] start (hosted gcc/g++ on InstantOS)\n");
    redirect_child_output();

    // --- C oracle -------------------------------------------------------
    serial("[GCCHOST] gcc -v\n");
    i64 rc=spawn_wait("/usr/bin/gcc",ARGV_GCC_V,ENVP);
    serial("GCCHOST_GCC_V_RC="); put_dec(rc); serial("\n");

    serial("[GCCHOST] gcc /hello.c -o /tmp/hello\n");
    rc=spawn_wait("/usr/bin/gcc",ARGV_GCC_CC,ENVP);
    serial("GCCHOST_CC_COMPILE_RC="); put_dec(rc); serial("\n");
    int cc_elf=is_elf("/tmp/hello");
    serial(cc_elf?"GCCHOST_CC_ELF_OK\n":"GCCHOST_CC_ELF_BAD\n");
    i64 cc_run=spawn_wait("/tmp/hello",ARGV_HELLO,ENVP);
    serial("GCCHOST_CC_RUN_RC="); put_dec(cc_run); serial("\n");
    if(rc==0 && cc_elf && cc_run==0) serial("GCCHOST_CC1_OK\n");

    // --- C++ oracle (iostream + exceptions + std::thread) ---------------
    serial("[GCCHOST] g++ /hello.cpp -o /tmp/hellocpp\n");
    rc=spawn_wait("/usr/bin/g++",ARGV_GXX_CC,ENVP);
    serial("GCCHOST_CXX_COMPILE_RC="); put_dec(rc); serial("\n");
    int cxx_elf=is_elf("/tmp/hellocpp");
    serial(cxx_elf?"GCCHOST_CXX_ELF_OK\n":"GCCHOST_CXX_ELF_BAD\n");
    i64 cxx_run=spawn_wait("/tmp/hellocpp",ARGV_HELLOCPP,ENVP);
    serial("GCCHOST_CXX_RUN_RC="); put_dec(cxx_run); serial("\n");
    if(rc==0 && cxx_elf && cxx_run==0) serial("GCCHOST_CXX_OK\n");

    // --- TCC oracle: gcc rebuilds tcc from source; tcc then self-hosts ------
    serial("[GCCHOST] gcc builds tcc from source -> /tmp/tcc_gcc\n");
    i64 tbuild=spawn_wait("/usr/bin/gcc",ARGV_GCC_TCC,ENVP);
    serial("GCCHOST_TCC_GCC_BUILD_RC="); put_dec(tbuild); serial("\n");
    int tcc_elf=is_elf("/tmp/tcc_gcc");
    serial(tcc_elf?"GCCHOST_TCC_GCC_ELF_OK\n":"GCCHOST_TCC_GCC_ELF_BAD\n");
    i64 tccv=spawn_wait("/tmp/tcc_gcc",ARGV_TCC_V,ENVP);
    serial("GCCHOST_TCC_GCC_V_RC="); put_dec(tccv); serial("\n");

    serial("[GCCHOST] tcc_gcc self-host stage2 -> /tmp/tcc2\n");
    i64 s2=spawn_wait("/tmp/tcc_gcc",ARGV_TCC_STAGE2,ENVP);
    serial("GCCHOST_TCC_STAGE2_RC="); put_dec(s2); serial("\n");
    int t2=is_elf("/tmp/tcc2");
    serial(t2?"GCCHOST_TCC2_ELF_OK\n":"GCCHOST_TCC2_ELF_BAD\n");

    serial("[GCCHOST] tcc2 self-host stage3 -> /tmp/tcc3\n");
    i64 s3=spawn_wait("/tmp/tcc2",ARGV_TCC_STAGE3,ENVP);
    serial("GCCHOST_TCC_STAGE3_RC="); put_dec(s3); serial("\n");
    int t3=is_elf("/tmp/tcc3");
    serial(t3?"GCCHOST_TCC3_ELF_OK\n":"GCCHOST_TCC3_ELF_BAD\n");

    serial("GCCHOST_TCC2_SIZE="); put_dec(file_size("/tmp/tcc2")); serial("\n");
    serial("GCCHOST_TCC3_SIZE="); put_dec(file_size("/tmp/tcc3")); serial("\n");
    int feq=files_equal("/tmp/tcc2","/tmp/tcc3");
    serial(feq==1?"GCCHOST_TCC_FIXPOINT_OK\n":(feq==0?"GCCHOST_TCC_FIXPOINT_DIFF\n":"GCCHOST_TCC_FIXPOINT_ERR\n"));

    // P6.7: gcc rebuilt tcc in-OS AND that tcc self-hosts (tcc2 == tcc3).
    if(tbuild==0 && tcc_elf && tccv==0 && s2==0 && t2 && s3==0 && t3 && feq==1)
        serial("GCCHOST_TCC_OK\n");

    dump_log();
    serial("GCCHOST_DONE\n");
    sc1(SYS_EXIT,0);
}
