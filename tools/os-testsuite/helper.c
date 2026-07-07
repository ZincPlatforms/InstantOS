// Test oracle spawned by the OS test suite (P1.2 exec/spawn coverage).
//
// Reads argc/argv/envp from the initial stack and exits with a status the
// runner can assert, so we can verify argv/env passing, large argv, and the
// argv layout produced by "#!" shebang resolution. With no recognised command
// it exits 42 (backwards-compatible with the original spawn+wait test).
//
//   (no args)            -> exit 42
//   argv[1]=="argc"      -> exit argc          (verify arg count survived)
//   argv[1]=="arglen"    -> exit strlen(argv[argc-1]) & 0xff  (verify long args)
//   argv[1]=="env"       -> exit int(getenv OSTEST_CODE), else 200 (verify envp)
//   argv[1]=="argeq"     -> exit 7 if argv[2]==argv[3] else 8   (shebang optarg check)

typedef unsigned long u64;

static long sc1(long n, long a){
    long r; register long rax asm("rax")=n; register long rbx asm("rbx")=a;
    asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");
    return r;
}
static void exit_(long code){ sc1(2 /*Exit*/, code); for(;;){} }

static int seq(const char* a, const char* b){
    if(!a||!b) return 0;
    while(*a && *b){ if(*a!=*b) return 0; a++; b++; }
    return *a==*b;
}
static unsigned slen(const char* s){ unsigned n=0; if(s) while(s[n]) n++; return n; }
// parse a small non-negative decimal prefix
static long satoi(const char* s){ long v=0; if(!s) return 0; while(*s>='0'&&*s<='9'){ v=v*10+(*s-'0'); s++; } return v; }

// C entry: sp points at [argc][argv0]..[0][env0]..[0][auxv..]
// (referenced only from the naked _start asm, so keep the symbol.)
__attribute__((used, noinline)) void helper_main(u64* sp){
    long argc = (long)sp[0];
    char** argv = (char**)&sp[1];
    char** envp = (char**)&sp[1 + argc + 1];

    long code = 42;
    if(argc >= 2){
        const char* cmd = argv[1];
        if(seq(cmd,"argc")){
            code = argc;
        } else if(seq(cmd,"arglen")){
            code = (long)(slen(argv[argc-1]) & 0xff);
        } else if(seq(cmd,"env")){
            code = 200;
            for(int i=0; envp[i]; i++){
                const char* e = envp[i];
                // match "OSTEST_CODE="
                const char* key = "OSTEST_CODE=";
                int k=0; while(key[k] && e[k]==key[k]) k++;
                if(key[k]=='\0'){ code = satoi(e+k) & 0xff; break; }
            }
        } else if(seq(cmd,"argeq")){
            code = (argc>=4 && seq(argv[2], argv[3])) ? 7 : 8;
        }
    }
    exit_(code & 0xff);
}

// The initial RSP points at argc (SysV ABI). Capture it before the C prologue
// perturbs it, then align and hand off. (naked: no compiler prologue/epilogue.)
__attribute__((naked)) void _start(void){
    asm volatile(
        "mov %rsp, %rdi\n\t"
        "and $-16, %rsp\n\t"
        "call helper_main\n\t"
    );
}
