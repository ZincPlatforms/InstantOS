// Phase 7 P7.1 - in-OS gcc rebuilds binutils (bash-free driver).
//
// The HOSTED gcc on the gcchost ext4 (/usr/bin/gcc) + the GNU userland ports on
// the initrd (/bin: sh make sed grep tar gzip awk cmp + coreutils) rebuild GNU
// binutils-2.42 from source, ON InstantOS:
//
//   untar    : gzip -dc /bin/binutils-insrc.tar.gz | tar -x  -> /work/binutils-2.42
//   configure: native (build=host=target=x86_64-unknown-instantos); huge sh/sed/
//              grep/awk/expr + gcc compile+link probe pass across bfd/opcodes/gas/ld
//   make     : compile the whole tree with the in-OS gcc -> gas/as-new, ld/ld-new
//   sanity   : run the freshly built as/ld (--version) in-OS
//
// Existing as/ar/ranlib (hosted binutils in /usr/bin) bootstrap the new build.
// Children write stdout/stderr to /dev/console (-> framebuffer + serial). Progress
// via GCCSELF_* serial markers (syscall 88). GCCSELF_BINUTILS_OK == P7.1 green.

typedef unsigned long u64;
typedef long i64;

static u64 sc1(u64 n, u64 a){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx):"rcx","r11","memory");return r;}
static u64 sc2(u64 n,u64 a,u64 b){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10):"rcx","r11","memory");return r;}
static u64 sc3(u64 n,u64 a,u64 b,u64 c){u64 r;register u64 rax asm("rax")=n;register u64 rbx asm("rbx")=a;register u64 r10 asm("r10")=b;register u64 rdx asm("rdx")=c;asm volatile("syscall":"=a"(r):"a"(rax),"r"(rbx),"r"(r10),"r"(rdx):"rcx","r11","memory");return r;}

enum {
    SYS_EXIT=2, SYS_WRITE=3, SYS_READ=4, SYS_OPEN=5, SYS_CLOSE=6,
    SYS_WAIT=10, SYS_SLEEP=15, SYS_MKDIR=32, SYS_UNLINK=34, SYS_DUP2=37, SYS_SPAWN=40, SYS_SERIAL=88,
};
enum { O_RDONLY=0, O_WRONLY=1, O_RDWR=2, O_CREAT=0100, O_TRUNC=01000 };

static unsigned slen(const char* s){unsigned n=0;while(s[n])n++;return n;}
static void serial(const char* s){sc2(SYS_SERIAL,(u64)s,slen(s));}
static void serial_n(const char* s,u64 n){sc2(SYS_SERIAL,(u64)s,n);}
static void put_dec(i64 v){char b[24];int i=23;b[23]=0;if(v==0){serial("0");return;}int neg=0;u64 u=(u64)v;if(v<0){neg=1;u=(u64)(-v);}while(u){b[--i]=(char)('0'+u%10);u/=10;}if(neg)b[--i]='-';serial(&b[i]);}
static void put_hex(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int nib=(int)((v>>((15-i)*4))&0xf);b[2+i]=(char)(nib<10?'0'+nib:'a'+nib-10);}b[18]=0;serial(b);}
static u64 rd16(const unsigned char* b){return (u64)b[0]|((u64)b[1]<<8);}
static u64 rd32(const unsigned char* b){return (u64)b[0]|((u64)b[1]<<8)|((u64)b[2]<<16)|((u64)b[3]<<24);}
static u64 rd64(const unsigned char* b){u64 v=0;for(int i=0;i<8;i++)v|=((u64)b[i])<<(8*i);return v;}


// Bind /dev/console (a real, dup-able char device) to stdin/stdout/stderr.
// autoconf configure dups all three (`exec 6>&1`, `exec 7<&0`); the bare implicit
// kernel console has no backing handle and cannot be duplicated. /dev/console
// writes to the framebuffer console + serial and reads as EOF; being a char
// device it is line-buffered (no block-buffered-file seek-on-flush issues).
static void setup_stdio(void){
    // Open /dev/console once read-write and install it as stdin/stdout/stderr, so
    // all three carry both READ and WRITE rights (autoconf dup()s and both reads
    // and writes them). read() returns EOF; write() -> console + serial.
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/dev/console",O_RDWR,0);
    if(fd<0) return;
    sc2(SYS_DUP2,(u64)fd,0);
    sc2(SYS_DUP2,(u64)fd,1);
    sc2(SYS_DUP2,(u64)fd,2);
    if(fd!=0 && fd!=1 && fd!=2) sc1(SYS_CLOSE,(u64)fd);
}
static int file_exists(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0; sc1(SYS_CLOSE,(u64)fd); return 1;
}
static i64 spawn_wait(const char* path, const char* const* argv, const char* const* envp){
    u64 pid=sc3(SYS_SPAWN,(u64)path,(u64)argv,(u64)envp);
    if((i64)pid<0||pid==(u64)-1) return -1;
    int status=0;
    sc3(SYS_WAIT,pid,(u64)&status,0);
    return (i64)(int)status;
}
static i64 exit_code(i64 status){ return (status>>8)&0xFF; }
static int is_elf(const char* path){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0) return 0;
    char m[4]={0};
    i64 n=(i64)sc3(SYS_READ,(u64)fd,(u64)m,4);
    sc1(SYS_CLOSE,(u64)fd);
    return n==4 && m[0]==0x7f && m[1]=='E' && m[2]=='L' && m[3]=='F';
}
static void dump_file(const char* path){
    serial("\n---DUMP "); serial(path); serial("---\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0){ serial("(open failed)\n"); return; }
    char buf[2048]; i64 n; while((n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf)))>0) serial_n(buf,(u64)n);
    sc1(SYS_CLOSE,(u64)fd);
    serial("\n---ENDDUMP---\n");
}


// gcc + as/ar/ranlib on the ext4 (/usr/bin); the GNU userland ports on the
// initrd (/bin). Both on PATH so configure/make find every tool.
static const char* const ENVP[]={
    "PATH=/usr/bin:/bin","HOME=/","TMPDIR=/work/tmp","SHELL=/bin/sh","CONFIG_SHELL=/bin/sh",
    "MAKEINFO=true","LC_ALL=C","LANG=C",0
};

static const char* const ARGV_UNTAR[] = {"sh","-c",
    "gzip -dc /bin/binutils-insrc.tar.gz | tar -x -C /work",0};
static const char* const ARGV_CONFIGURE[] = {"sh","-c",
    "cd /work/bu-build && /work/binutils-2.42/configure "
    "--host=x86_64-unknown-instantos --build=x86_64-unknown-instantos "
    "--target=x86_64-unknown-instantos --prefix=/usr --disable-nls --disable-werror "
    "--without-zstd --without-debuginfod --without-msgpack --disable-gdb --disable-gold "
    "--disable-gprofng --disable-libctf --disable-plugins --without-isl --disable-lto "
    "--disable-bootstrap --enable-default-hash-style=sysv",0};
static const char* const ARGV_MAKE[] = {"sh","-c",
    "cd /work/bu-build && make -j2 MAKEINFO=true all-gas all-ld all-binutils",0};
static const char* const ARGV_AS_V[] = {"as-new","--version",0};
static const char* const ARGV_LD_V[] = {"ld-new","--version",0};

// --- FAST REPRO for the fork/exec frame-reuse UAF (bug #4) --------------------
// Skips the 108MB untar/configure/make; just hammers bash->gcc->cc1->collect2->ld
// links so the churn-triggered collect2 corruption reproduces in ~minutes.
#define REPRO_BUG4 0
static const char* const ARGV_LINK[] = {"sh","-c","gcc /tmp/t.c -o /tmp/t_out",0};
static void repro_bug4(void){
    // write a trivial translation unit
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/t.c",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ const char* s="int main(void){return 0;}\n"; sc3(SYS_WRITE,(u64)fd,(u64)s,slen(s)); sc1(SYS_CLOSE,(u64)fd); }
    serial("[REPRO] hammering bash->gcc->collect2->ld links (churn UAF probe)\n");
    for(int i=1;i<=150;i++){
        i64 rc = spawn_wait("/bin/sh", ARGV_LINK, ENVP);
        int ok = is_elf("/tmp/t_out");
        serial("[REPRO] i="); put_dec(i); serial(" rc="); put_dec(rc); serial(ok?" ELF_OK\n":" ELF_BAD\n");
        if(!ok){ serial("REPRO_FIRST_FAIL_AT="); put_dec(i); serial("\n"); }
    }
    serial("REPRO_DONE\n");
    sc1(SYS_EXIT,0);
}

// --- EXEC DIAG: why do freshly-linked binaries fail to run? -------------------
#define REPRO_EXEC 0
static void inspect_elf(const char* path){
    serial("[EXEC] inspect "); serial(path); serial("\n");
    i64 fd=(i64)sc3(SYS_OPEN,(u64)path,O_RDONLY,0);
    if(fd<0){ serial("  open FAILED rc="); put_dec((i64)fd); serial("\n"); return; }
    static unsigned char buf[8192];
    i64 n=(i64)sc3(SYS_READ,(u64)fd,(u64)buf,sizeof(buf));
    sc1(SYS_CLOSE,(u64)fd);
    if(n<64){ serial("  short read n="); put_dec(n); serial("\n"); return; }
    if(!(buf[0]==0x7f&&buf[1]=='E'&&buf[2]=='L'&&buf[3]=='F')){ serial("  not ELF\n"); return; }
    u64 etype=rd16(buf+16), entry=rd64(buf+24), phoff=rd64(buf+32);
    u64 phentsize=rd16(buf+54), phnum=rd16(buf+56);
    serial("  e_type="); put_dec((i64)etype); serial(" (2=EXEC 3=DYN) entry="); put_hex(entry);
    serial(" phnum="); put_dec((i64)phnum); serial("\n");
    for(u64 i=0;i<phnum && phoff+(i+1)*phentsize<=(u64)n;i++){
        const unsigned char* ph=buf+phoff+i*phentsize;
        u64 ptype=rd32(ph), poff=rd64(ph+8), pvaddr=rd64(ph+16), pfilesz=rd64(ph+32);
        if(ptype==3){ serial("  PT_INTERP off="); put_hex(poff); serial(" len="); put_dec((i64)pfilesz);
            serial(" str=\""); if(poff+pfilesz<=(u64)n && pfilesz>0) serial_n((const char*)(buf+poff),pfilesz-1); serial("\"\n"); }
        if(ptype==1){ serial("  PT_LOAD vaddr="); put_hex(pvaddr); serial("\n"); }
    }
}
static const char* const ARGV_CC_T[] = {"sh","-c","gcc /tmp/t.c -o /tmp/t_out 2>/tmp/cc.err; echo cc_exit=$?",0};
static const char* const ARGV_TOUT[]  = {"/tmp/t_out",0};
static const char* const ARGV_TOUT_SH[]= {"sh","-c","/tmp/t_out; echo t_out_via_sh_exit=$?",0};
static const char* const ARGV_CFG[]    = {"sh","-c",
    "cd /tmp/cfgt && echo 'int main(void){return 0;}' > conftest.c && "
    "gcc conftest.c -o conftest && echo compiled && ./conftest; echo relexit=$?",0};
static const char* const ARGV_DOTSLASH[]={"sh","-c","cd /tmp && ./t_out; echo dot_slash_exit=$?",0};
static void repro_exec(void){
    i64 fd=(i64)sc3(SYS_OPEN,(u64)"/tmp/t.c",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){ const char* s="int main(void){return 42;}\n"; sc3(SYS_WRITE,(u64)fd,(u64)s,slen(s)); sc1(SYS_CLOSE,(u64)fd); }
    serial("[EXEC] compile /tmp/t.c -> /tmp/t_out\n");
    i64 crc=spawn_wait("/bin/sh",ARGV_CC_T,ENVP);
    serial("[EXEC] compile rc="); put_dec(crc); serial(is_elf("/tmp/t_out")?" ELF_OK\n":" ELF_BAD\n");
    dump_file("/tmp/cc.err");
    inspect_elf("/tmp/t_out");
    serial("[EXEC] spawn /tmp/t_out directly...\n");
    i64 rc=spawn_wait("/tmp/t_out",ARGV_TOUT,ENVP);
    serial("[EXEC] direct spawn_wait rc="); put_dec(rc); serial(" exit="); put_dec(exit_code(rc)); serial("\n");
    serial("[EXEC] run /tmp/t_out via sh (absolute)...\n");
    spawn_wait("/bin/sh",ARGV_TOUT_SH,ENVP);
    // Mimic configure EXACTLY: compile in a build dir, then run via RELATIVE ./path.
    sc3(SYS_MKDIR,(u64)"/tmp/cfgt",0755,0);
    serial("[EXEC] configure-style: cd /tmp/cfgt; gcc conftest.c -o conftest; ./conftest\n");
    spawn_wait("/bin/sh",ARGV_CFG,ENVP);
    serial("[EXEC] relative exec from /tmp...\n");
    spawn_wait("/bin/sh",ARGV_DOTSLASH,ENVP);
    serial("EXEC_DONE\n");
    sc1(SYS_EXIT,0);
}

void _start(void){
    serial("\n[GCCSELF] start: in-OS gcc rebuilds binutils (P7.1)\n");
    setup_stdio();
#if REPRO_EXEC
    repro_exec();
#endif
#if REPRO_BUG4
    repro_bug4();
#endif

    sc3(SYS_MKDIR,(u64)"/work",0755,0);
    sc3(SYS_MKDIR,(u64)"/work/tmp",0755,0);
    sc3(SYS_MKDIR,(u64)"/work/bu-build",0755,0);
    serial("[GCCSELF] untar binutils-2.42 source\n");
    i64 rc = spawn_wait("/bin/sh", ARGV_UNTAR, ENVP);
    serial("GCCSELF_BU_UNTAR_RC="); put_dec(rc); serial("\n");
    int untar_ok = file_exists("/work/binutils-2.42/configure");
    serial(untar_ok ? "GCCSELF_BU_UNTAR_OK\n" : "GCCSELF_BU_UNTAR_FAIL\n");

    serial("[GCCSELF] configure (native instantos; sh+sed+grep+awk+gcc probes)\n");
    rc = spawn_wait("/bin/sh", ARGV_CONFIGURE, ENVP);
    serial("GCCSELF_BU_CONFIGURE_RC="); put_dec(rc); serial("\n");
    int cfg_ok = file_exists("/work/bu-build/Makefile") && file_exists("/work/bu-build/gas/Makefile")
              && file_exists("/work/bu-build/ld/Makefile");
    serial(cfg_ok ? "GCCSELF_BU_CONFIGURE_OK\n" : "GCCSELF_BU_CONFIGURE_FAIL\n");
    if(!cfg_ok) dump_file("/work/bu-build/config.log");

    serial("[GCCSELF] make (in-OS gcc compiles bfd/opcodes/gas/ld/binutils)\n");
    rc = spawn_wait("/bin/sh", ARGV_MAKE, ENVP);
    serial("GCCSELF_BU_MAKE_RC="); put_dec(rc); serial("\n");
    int as_elf = is_elf("/work/bu-build/gas/as-new");
    int ld_elf = is_elf("/work/bu-build/ld/ld-new");
    serial(as_elf ? "GCCSELF_BU_AS_ELF_OK\n" : "GCCSELF_BU_AS_ELF_BAD\n");
    serial(ld_elf ? "GCCSELF_BU_LD_ELF_OK\n" : "GCCSELF_BU_LD_ELF_BAD\n");

    i64 as_run = -1, ld_run = -1;
    if(as_elf){ as_run = spawn_wait("/work/bu-build/gas/as-new", ARGV_AS_V, ENVP);
        serial("GCCSELF_BU_AS_RUN_RC="); put_dec(as_run); serial("\n"); }
    if(ld_elf){ ld_run = spawn_wait("/work/bu-build/ld/ld-new", ARGV_LD_V, ENVP);
        serial("GCCSELF_BU_LD_RUN_RC="); put_dec(ld_run); serial("\n"); }
    if(as_elf && exit_code(as_run)==0) serial("GCCSELF_BU_AS_OK\n");
    if(ld_elf && exit_code(ld_run)==0) serial("GCCSELF_BU_LD_OK\n");

    if(untar_ok && cfg_ok && rc==0 && as_elf && ld_elf &&
       exit_code(as_run)==0 && exit_code(ld_run)==0)
        serial("GCCSELF_BINUTILS_OK\n");

    serial("GCCSELF_DONE\n");
    sc1(SYS_EXIT,0);
}
