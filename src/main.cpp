#include <cpu/process/process.hpp>
#include <cpu/process/exec.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/syscall/syscall.hpp>
#include <cpu/gdt/gdt.hpp>

extern GDT* gdt;

int main(){
    const char* argv[] = { "/autopong", nullptr };
    Process* userProc = ProcessExecutor::loadUserBinaryWithArgs("/autopong", 1, argv);

    if (!userProc) {
        userProc = ProcessExecutor::loadUserBinaryWithArgs("/autopong", 1, argv);
    }

    if (!userProc) {
        for(;;) asm volatile("hlt");
    }

    Scheduler::get().addProcess(userProc);
    Scheduler::get().setCurrentProcess(userProc);
    userProc->setState(ProcessState::Running);

    gdt->setKernelStack(userProc->getKernelStack());
    Syscall::get().setKernelStack(userProc->getKernelStack());

    asm volatile("sti");

    switchContext(nullptr, userProc->getContext());

    for(;;);
    return 0;
}
