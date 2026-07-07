// Minimal thread_create diagnostic. Runs as /bin/login and emits a serial marker
// at each step so we can see exactly where a new thread dies:
//   [TP] main begin              -> _start reached
//   [TP] entry=<hex>             -> address of thread_fn (compare to any crash RIP)
//   [TP] thread_create rc=<hex>  -> handle / error
//   [TP] thread entered          -> the new thread actually ran thread_fn (trampoline OK)
//   [TP] thread set val          -> the thread wrote shared memory + did a syscall
//   [TP] main saw val            -> parent observed the shared write
// The thread deliberately does NOT call thread_exit (isolates startup/run from
// teardown/reap). If we see "thread entered" then thread startup works and the
// earlier crash is in exit/reap; if not, startup itself is broken.
typedef unsigned long u64; typedef long i64;
static i64 sc(i64 n,i64 a,i64 b,i64 c){ i64 r; register i64 rax asm("rax")=n; register i64 rbx asm("rbx")=a; register i64 r10 asm("r10")=b; register i64 rdx asm("rdx")=c; asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory"); return r; }
enum { N_EXIT=2, N_YIELD=14, N_SLEEP=15, N_THREADCREATE=74, N_SERIAL=88 };
static u64 slen(const char* s){u64 n=0;while(s[n])n++;return n;}
static void sw(const char* s){ sc(N_SERIAL,(i64)s,(i64)slen(s),0); }
static void shex(u64 v){ char b[19]; b[0]='0';b[1]='x'; for(int i=0;i<16;i++){unsigned q=(v>>((15-i)*4))&0xf; b[2+i]=(char)(q<10?'0'+q:'a'+q-10);} b[18]=0; sw(b); }

static volatile u64 g_val=0;
static void thread_fn(void* arg){
    (void)arg;
    sw("[TP] thread entered\n");
    g_val=0xABCD;
    sw("[TP] thread set val\n");
    for(;;){ sc(N_SLEEP,1000,0,0); }
}

__attribute__((force_align_arg_pointer))
void _start(void){
    sw("\n[TP] main begin\n");
    sw("[TP] entry="); shex((u64)(void*)thread_fn); sw("\n");
    i64 th=sc(N_THREADCREATE,(i64)(void*)thread_fn,0,0);
    sw("[TP] thread_create rc="); shex((u64)th); sw("\n");
    for(int i=0;i<400 && g_val!=0xABCD;i++){ sc(N_YIELD,0,0,0); sc(N_SLEEP,5,0,0); }
    sw("[TP] main saw val="); shex(g_val); sw("\n");
    sw("[TP] DONE\n");
    for(;;){ sc(N_SLEEP,1000,0,0); }
}
