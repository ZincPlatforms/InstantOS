// Minimal reproducer for the kill-storm scheduler race (P1.1 follow-up).
// Runs as /bin/login. Forks a batch of children that loop over a syscall
// boundary, SIGKILLs them all, then reaps them with a BLOCKING wait (the
// pattern that hangs on the last child). The kernel ktrace watchdog detects the
// stuck (SIGKILL-pending-but-alive) process and dumps the scheduler trace ring.
typedef unsigned long u64; typedef long i64;

static i64 syscall5(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e){
    i64 r;
    register i64 rax asm("rax")=n;
    register i64 rbx asm("rbx")=a;
    register i64 r10 asm("r10")=b;
    register i64 rdx asm("rdx")=c;
    register i64 r8  asm("r8")=d;
    register i64 r9  asm("r9")=e;
    asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx),"r"(r8),"r"(r9):"rcx","r11","memory");
    return r;
}
#define SC0(n)           syscall5((n),0,0,0,0,0)
#define SC1(n,a)         syscall5((n),(i64)(a),0,0,0,0)
#define SC2(n,a,b)       syscall5((n),(i64)(a),(i64)(b),0,0,0)
#define SC3(n,a,b,c)     syscall5((n),(i64)(a),(i64)(b),(i64)(c),0,0)

enum { N_EXIT=2, N_FORK=8, N_WAIT=10, N_KILL=11, N_YIELD=14, N_SLEEP=15, N_SERIAL=88 };
enum { SIG_KILL=9 };

static u64 slen(const char* s){u64 n=0;while(s[n])n++;return n;}
static void sw(const char* s){ SC2(N_SERIAL,s,slen(s)); }
static void sdec(i64 v){ char b[24]; int i=23; b[23]=0; if(v==0){ sw("0"); return; } int neg=v<0; u64 u=neg?(u64)(-v):(u64)v; while(u){ b[--i]=(char)('0'+u%10); u/=10; } if(neg)b[--i]='-'; sw(&b[i]); }

#define KN 8

__attribute__((force_align_arg_pointer))
void _start(void){
    sw("\n[killrace] BEGIN\n");

    i64 kids[KN]; int spawned=0;
    for(int i=0;i<KN;i++){
        i64 pid=SC0(N_FORK);
        if(pid==0){ for(;;){ SC0(N_YIELD); SC1(N_SLEEP,5); } } // child: killable loop
        if(pid>0) kids[spawned++]=pid; else break;
    }
    sw("[killrace] forked="); sdec(spawned); sw("\n");

    for(int i=0;i<spawned;i++) SC2(N_KILL,kids[i],SIG_KILL);
    sw("[killrace] killed all\n");

    for(int i=0;i<spawned;i++){
        sw("[killrace] reap i="); sdec(i); sw(" pid="); sdec(kids[i]); sw("\n");
        int st=0;
        i64 w=SC3(N_WAIT,kids[i],&st,0);   // BLOCKING wait -- hangs on the racy child
        sw("[killrace] got w="); sdec(w); sw(" code="); sdec((st>>8)&0xff); sw("\n");
    }

    sw("[killrace] DONE (all reaped)\n");
    for(;;){ SC1(N_SLEEP,1000); }
}
