#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  backtrace();
  
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void restore_trapframe(struct proc* p) {
    p->trapframe->kernel_satp = p->src_trapframe.kernel_satp;   // kernel page table
    p->trapframe->kernel_sp = p->src_trapframe.kernel_sp;     // top of process's kernel stack
    p->trapframe->kernel_trap = p->src_trapframe.kernel_trap;   // usertrap()
    p->trapframe->epc = p->src_trapframe.epc;           // saved user program counter
    p->trapframe->kernel_hartid = p->src_trapframe.kernel_hartid; // saved kernel tp

    p->trapframe->ra = p->src_trapframe.ra;
    p->trapframe->sp = p->src_trapframe.sp;
    p->trapframe->gp = p->src_trapframe.gp;
    p->trapframe->tp = p->src_trapframe.tp;
    p->trapframe->t0 = p->src_trapframe.t0;
    p->trapframe->t1 = p->src_trapframe.t1;
    p->trapframe->t2 = p->src_trapframe.t2;
    p->trapframe->s0 = p->src_trapframe.s0;
    p->trapframe->s1 = p->src_trapframe.s1;

    p->trapframe->a0 = p->src_trapframe.a0;
    p->trapframe->a1 = p->src_trapframe.a1;
    p->trapframe->a2 = p->src_trapframe.a2;
    p->trapframe->a3 = p->src_trapframe.a3;
    p->trapframe->a4 = p->src_trapframe.a4;
    p->trapframe->a5 = p->src_trapframe.a5;
    p->trapframe->a6 = p->src_trapframe.a6;
    p->trapframe->a7 = p->src_trapframe.a7;

    p->trapframe->s2 = p->src_trapframe.s2;
    p->trapframe->s3 = p->src_trapframe.s3;
    p->trapframe->s4 = p->src_trapframe.s4;
    p->trapframe->s5 = p->src_trapframe.s5;
    p->trapframe->s6 = p->src_trapframe.s6;
    p->trapframe->s7 = p->src_trapframe.s7;
    p->trapframe->s8 = p->src_trapframe.s8;
    p->trapframe->s9 = p->src_trapframe.s9;
    p->trapframe->s10 = p->src_trapframe.s10;
    p->trapframe->s11 = p->src_trapframe.s11;

    p->trapframe->t3 = p->src_trapframe.t3;
    p->trapframe->t4 = p->src_trapframe.t4;
    p->trapframe->t5 = p->src_trapframe.t5;
    p->trapframe->t6 = p->src_trapframe.t6;
}

uint64 sys_sigalarm(void) {
    int ticks;
    argint(0, &ticks);

    uint64 va_func;
    argaddr(1, &va_func);

    struct proc* p = myproc();

    p->ticks = ticks;
    p->tick_timer = 0;
    p->handler = va_func;
    p->in_handler = 0;
    
    return 0;
}

uint64 sys_sigreturn(void) {
    struct proc* p = myproc();
    p->in_handler = 0;
    p->tick_timer = 0;
    restore_trapframe(p);
  
    return 0;
}