// P1.4 power-cut / journal-replay driver. Runs as /bin/login on the ext4 root
// and hammers the filesystem with a sustained create/write/fsync/unlink loop,
// printing heartbeats. The harness SIGKILLs QEMU mid-loop (simulated power
// loss); because each op is fsync'd, committed state (and the JBD2 journal)
// reaches the backing image, so:
//   * host `e2fsck -fy` must replay the journal and end consistent, and
//   * a subsequent normal kernel boot must remount + recover the image.
typedef unsigned long u64; typedef long i64; typedef unsigned char u8;

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
#define SC1(n,a)         syscall5((n),(i64)(a),0,0,0,0)
#define SC2(n,a,b)       syscall5((n),(i64)(a),(i64)(b),0,0,0)
#define SC3(n,a,b,c)     syscall5((n),(i64)(a),(i64)(b),(i64)(c),0,0)

enum { N_WRITE=3, N_OPEN=5, N_CLOSE=6, N_UNLINK=34, N_MKDIR=32, N_SLEEP=15, N_SERIAL=88, N_FSYNC=130 };
enum { O_WRONLY=1, O_CREAT=0100, O_TRUNC=01000 };

static u64 slen(const char* s){u64 n=0;while(s[n])n++;return n;}
static void sw(const char* s){ SC2(N_SERIAL,s,slen(s)); }
static void sdec(i64 v){ char b[24]; int i=23; b[23]=0; if(v==0){ sw("0"); return; } u64 u=(u64)v; while(u){ b[--i]=(char)('0'+u%10); u/=10; } sw(&b[i]); }
static void iname(char* buf,const char* pfx,int n){ int i=0; while(pfx[i]){buf[i]=pfx[i];i++;} char t[12]; int j=0; if(n==0)t[j++]='0'; while(n){t[j++]=(char)('0'+n%10);n/=10;} while(j>0)buf[i++]=t[--j]; buf[i]='\0'; }

__attribute__((force_align_arg_pointer))
void _start(void){
    sw("\n[fscrash] BEGIN sustained ext4 churn (kill me any time)\n");
    SC2(N_MKDIR,"/crash",0755);   // tolerate EEXIST

    static u8 buf[4096];
    for(int i=0;i<4096;i++) buf[i]=(u8)(i&0xff);

    char nm[40];
    long op=0;
    for(;;){
        int slot = (int)(op % 64);
        iname(nm,"/crash/f",slot);
        i64 h=SC3(N_OPEN,nm,O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(h>=0){
            SC3(N_WRITE,h,buf,4096);
            SC1(N_FSYNC,h);          // make the write + journal durable on the image
            SC1(N_CLOSE,h);
        }
        if((op & 7)==7){             // periodically delete an older slot (churn frees)
            iname(nm,"/crash/f",(int)((op/8)%64));
            SC1(N_UNLINK,nm);
        }
        if((op % 50)==0){ sw("[fscrash] tick "); sdec(op); sw("\n"); }
        op++;
    }
}
