#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  if (argaddr(0, &p) < 0) return -1;
  return wait(p,myproc()->trapframe->a1);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}


//lab2添加
uint64 sys_yield(void){
  //获取当前正在执行的进程PCB
  struct proc *p=myproc();
  //打印出该进程对应的内核线程在进行上下文切换时，上下文被保存到的地址区间
  printf("Save the context of the process to the memory region from address %p to %p\n", &p->context,&(p->context)+1);
  //打印出该进程的用户态陷入内核态时PC的值
  printf("Current running process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);
  //根据调度器的工作方式模拟一次调度，找到下一个RUNNABLE的进程，同样打印相关信息
  struct proc *pr=p;//临时查找进程指针
  int i=0;//循环变量
  for (i=0;i<NPROC;i++) {
  pr=proc+(p-proc+i)%NPROC;//
    acquire(&pr->lock);//获取进程锁，保持互斥访问
    if (pr->state == RUNNABLE) {//查找到第一个符合条件的进程
      release(&pr->lock);//释放锁
      break;//退出
    }
    release(&pr->lock);
  }
  printf("Next runnable process pid is %d and user pc is %p\n", pr->pid,pr->trapframe->epc);
  //然后将当前进程挂起
  yield();
  return 0;
}
