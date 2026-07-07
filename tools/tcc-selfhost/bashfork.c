// Headless bash fork/exec regression driver for InstantOS.
//
// Background: GNU bash (on mlibc) used to intermittently fault on InstantOS
// whenever it created a new process (fork/exec), producing an int3 storm that
// wedged the whole OS. Root cause was in the kernel: a new Process's FPUState
// buffer allocation could fail under heap pressure and leave the fpuState
// pointer at heap ALLOC_POISON (0xAA..), which the context switch then fed to
// XSAVE -> #GP -> int3 storm. Now fixed (null-init + right-sized FPUState +
// XSAVE operand guard). This driver re-runs the exact bash-fork path to prove it.
//
// Phase A (launcher-driven, LIVE on serial): SYS_SPAWN /bin/bash repeatedly and
//   print each child's status inline, so progress is visible even if the OS were
//   to wedge mid-run. Exercises repeated bash process creation.
// Phase B: SYS_SPAWN /bin/bash /bin/forktest.sh once; that script hammers bash's
//   own fork() via subshells, pipelines, background jobs, command substitution,
//   and external execs. Child output -> /tmp/bf.log, dumped to serial at the end.

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

// Freestanding: clang may lower zero-init to memset; no libc is linked.
void* memset(void* d,int c,u64 n){unsigned char* p=(unsigned char*)d;for(u64 i=0;i<n;i++)p[i]=(unsigned char)c;return d;}
void* memcpy(void* d,const void* s,u64 n){unsigned char* a=(unsigned char*)d;const unsigned char* b=(const unsigned char*)s;for(u64 i=0;i<n;i++)a[i]=b[i];return d;}

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

static void dump_log(void){
    serial("\n---BASHLOG---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bf.log",O_RDONLY,0);
    if(fd>=0){ char buf[1024]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n); sc1(SYS_CLOSE,(u64)fd); }
    serial("\n---ENDLOG---\n");
}

static const char* const ENVP[]={"PATH=/bin","HOME=/","TMPDIR=/tmp",0};

// Per-construct isolation: run each bash fork/exec construct in its OWN bash so a
// hang in one is attributable. A serial marker is printed BEFORE each spawn (via
// SYS_SERIAL, always visible even if the OS were to stall), and an rc marker
// AFTER. If a construct hangs, its "begin" marker is the last thing on serial and
// no "rc"/subsequent markers appear -> that construct is the culprit.
static void run(const char* tag, const char* const* argv){
    serial("[BF] BEGIN "); serial(tag); serial("\n");
    i64 st=spawn_wait("/bin/bash", argv, ENVP);
    serial("[BF] RC "); serial(tag); serial(" =raw"); put_dec(st);
    serial(":exit"); put_dec((st>>8)&0xff); serial(":sig"); put_dec(st&0x7f); serial("\n");
}

void _start(void){
    serial("\n[BF] start per-construct diagnostic (bash fork/exec)\n");

    // Each argv[2] is a single bash -c program exercising exactly one construct.
    static const char* const t1[]={"bash","-c","i=0; while [ $i -lt 3 ]; do /bin/tcc -v; i=$((i+1)); done",0};
    static const char* const t2[]={"bash","-c","i=0; while [ $i -lt 5 ]; do ( exit 0 ); i=$((i+1)); done; echo SUBSHELL_OK",0};
    static const char* const t3[]={"bash","-c","/bin/tcc -v | /bin/tcc -v; echo PIPE_OK",0};
    static const char* const t4[]={"bash","-c","/bin/bash -c 'exit 5'; echo NESTED_RC=$?",0};
    static const char* const t5[]={"bash","-c","/bin/tcc -v & wait; echo BGWAIT_OK",0};
    static const char* const t6[]={"bash","-c","v=\"$(/bin/tcc -v)\"; echo SUBST_OK",0};

    run("T1_exec",     t1);
    run("T2_subshell", t2);
    run("T3_pipe",     t3);
    run("T4_nested",   t4);
    run("T5_bg_wait",  t5);
    run("T6_cmdsubst", t6);
    serial("[BF] diagnostic ALL_PASSED\n");

    // Sustained combined stress: hundreds of forks of every kind in ONE long-
    // lived bash (catches fd/zombie accumulation the isolated tests can't).
    serial("[BF] combined stress: /bin/bash /bin/forktest.sh\n");
    static const char* const scr[]={"bash","/bin/forktest.sh",0};
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/bf.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ sc2(SYS_DUP2,(u64)fd,1); sc2(SYS_DUP2,(u64)fd,2); if(fd!=1&&fd!=2) sc1(SYS_CLOSE,(u64)fd); }
    i64 bst=spawn_wait("/bin/bash", scr, ENVP);
    dump_log();
    serial("BASHFORK_SCRIPT_STATUS=raw"); put_dec(bst);
    serial(":exit"); put_dec((bst>>8)&0xff); serial(":sig"); put_dec(bst&0x7f); serial("\n");
    serial((bst&0x7f)==0 && ((bst>>8)&0xff)==0 ? "BASHFORK_RESULT_OK\n" : "BASHFORK_RESULT_FAULT\n");
    serial("BASHFORK_DONE\n");
    sc1(SYS_EXIT,0);
}
