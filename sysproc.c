#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Halt (shutdown) the system by sending a special
// signal to QEMU.
// Based on: http://pdos.csail.mit.edu/6.828/2012/homework/xv6-syscall.html
// and: https://github.com/t3rm1n4l/pintos/blob/master/devices/shutdown.c
int
sys_halt(void)
{
  char *p = "Shutdown";
  for( ; *p; p++)
    outw(0xB004, 0x2000);
  return 0;
}

struct semaphore{
    int value;
    int max;
    int active;
    struct spinlock* lock;
};

#define SEM_NUM 32

struct semaphore* SEM_STORE[SEM_NUM];

int sys_sem_init(void){
    int semId;
    int n;

    argint(1, &semId);
    argint(2, &n);

    if(semId < 0 || semId >= SEM_NUM){
        // cprintf("Attempted to initialize semaphore with invalid ID %d.\n", semId);
        return -1;
    }

    if(!SEM_STORE[semId]){
        struct semaphore sem;
        sem.value = sem.max = sem.active = 0;
        
        char lock_name[3];
        lock_name[0] = (char) (n / 10);
        lock_name[1] = (char) (n % 10);
        lock_name[2] = '\0';

        struct spinlock lock;
        initlock(&lock, lock_name);

        sem.lock = &lock;
        SEM_STORE[semId] = &sem;
    }

    acquire(SEM_STORE[semId]->lock);

    if(SEM_STORE[semId] && SEM_STORE[semId]->active){
        return -1;
    }

    SEM_STORE[semId]->value = SEM_STORE[semId]->max = n;
    SEM_STORE[semId]->active = 1;

    release(SEM_STORE[semId]->lock);

    return semId;
}

int sys_sem_destroy(void){
    int semId;

    argint(1, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(SEM_STORE[semId]->lock);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        return -1;
    }

    SEM_STORE[semId]->active = 0;

    return semId;
}

// WARNING: ASSUMES YOU ALREADY HAVE A LOCK.
int _can_acquire(int semId){
    // Somebody messed up.
    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        panic("_can_acquire");
    }

    int current_value = SEM_STORE[semId]->value;

    return current_value > 0;
}

int sys_sem_wait(void){
    int semId;

    argint(1, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(SEM_STORE[semId]->lock);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        return -1;
    }

    // We only want to wake up a single one.
    while(!_can_acquire(semId)){
        proc->chan = &semId;
        sleep(&semId, SEM_STORE[semId]->lock);
        // when sleep returns we have the lock again
    }

    SEM_STORE[semId]->value--;

    release(SEM_STORE[semId]->lock);

    // semaphore is good (y)

    return semId;
}

int sys_sem_signal(void){
    int semId;

    argint(1, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(SEM_STORE[semId]->lock);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        return -1;
    }

    if(SEM_STORE[semId]->value == SEM_STORE[semId]->max){
        return -1;
    }

    // It should be safe to acquire this since if we're calling
    // this we probably got past sem_wait.
    SEM_STORE[semId]->value++;
    wakeup(&semId);

    release(SEM_STORE[semId]->lock);

    return 0;
}

