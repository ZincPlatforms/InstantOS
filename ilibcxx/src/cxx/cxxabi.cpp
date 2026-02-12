#include <instant/syscall.hpp>
#include <cstdio.hpp>

extern "C" {

#define MAX_ATEXIT 32

typedef void (*destructor_func)(void*);
typedef void (*init_func)(void);

extern "C" void* __dso_handle = nullptr;
struct atexit_entry {
    destructor_func func;
    void* arg;
    void* dso_handle;
};
void(*funcs[MAX_ATEXIT])();
static int fn_count = 0;
static atexit_entry atexit_funcs[MAX_ATEXIT];
static int atexit_count = 0;

extern init_func __init_array_start[];
extern init_func __init_array_end[];
extern init_func __fini_array_start[];
extern init_func __fini_array_end[];

int __cxa_guard_acquire(long* guard) {
    return !*(char*)guard;
}

void __cxa_guard_release(long* guard) {
    *(char*)guard = 1;
}

void __cxa_guard_abort(long* guard) {
}

void __cxa_pure_virtual() {
    printf("Pure virtual function called!\n");
    syscall0(0);
    while(1);
}

int __cxa_exitfunc(void(*func)()){
    if (fn_count >= MAX_ATEXIT) {
        return -1;
    }
    
    funcs[fn_count] = func;
    fn_count++;
}

int __cxa_atexit(destructor_func func, void* arg, void* dso_handle) {
    if (atexit_count >= MAX_ATEXIT) {
        return -1;
    }
    
    atexit_funcs[atexit_count].func = func;
    atexit_funcs[atexit_count].arg = arg;
    atexit_funcs[atexit_count].dso_handle = dso_handle;
    atexit_count++;
    
    return 0;
}

void __cxa_finalize(void* dso_handle) {
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (dso_handle == nullptr || atexit_funcs[i].dso_handle == dso_handle) {
            if (atexit_funcs[i].func) {
                atexit_funcs[i].func(atexit_funcs[i].arg);
            }
        }
    }

    for (int i = fn_count - 1; i >= 0; i--) {
        if (funcs[i]) {
            funcs[i]();
        }
    }
}

void _init() {
    for (init_func* func = __init_array_start; func < __init_array_end; func++) {
        (*func)();
    }
}

void _fini() {
    __cxa_finalize(nullptr);
    for (init_func* func = __fini_array_start; func < __fini_array_end; func++) {
        (*func)();
    }
}

}
