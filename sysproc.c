#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
int nextpidp = 1;
extern void forkret(void);
extern void trapret(void);

//void wakeup1(void *chan);


static struct proc*
allocproc(void)
{
  cprintf("entering allocproc here\n");
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  //cprintf("Entering this for loop\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    //cprintf("Searching for an UNUSED slot\n");
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  cprintf("Could not find an unused slot\n");
  return 0;

found:
  cprintf("UNUSED slot found!\n");
  p->state = EMBRYO;
  p->pid = nextpidp++;
  release(&ptable.lock);
 if((p->kstack = kalloc()) == 0){
    cprintf("Failure to allocate kstack\n");
    p->state = UNUSED;
    return 0;
  }
  cprintf("Allocated stack\n");
  sp = p->kstack + KSTACKSIZE;
  
 sp -= sizeof *p->tf;
  cprintf("Assinging trapframe\n");
  p->tf = (struct trapframe*)sp;
  cprintf("Finished trapframe\n");
 sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  cprintf("Returning p\n");
  return p;
}



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
    struct spinlock lock;
};

#define SEM_NUM 32

static struct semaphore SEM_STORE[SEM_NUM];

void _sem_init(){
    int n;

    for(n = 0; n < SEM_NUM; n++){
        struct semaphore sem;
        sem.value = sem.max = sem.active = 0;
        
        char lock_name[6];
        lock_name[0] = 's';
        lock_name[1] = 'e';
        lock_name[2] = 'm';
        lock_name[3] = (char) (n / 10);
        lock_name[4] = (char) (n % 10);
        lock_name[5] = '\0';

        struct spinlock lock;
        initlock(&lock, lock_name);

        sem.lock = lock;
        SEM_STORE[n] = sem;
    }
}

int sys_sem_init(void){
    int semId;
    int n;

    argint(0, &semId);
    argint(1, &n);

    if(semId < 0 || semId >= SEM_NUM){
        // cprintf("Attempted to initialize semaphore with invalid ID %d.\n", semId);
        return -1;
    }

    acquire(&SEM_STORE[semId].lock);

    if(SEM_STORE[semId].active){
        release(&SEM_STORE[semId].lock);
        return -1;
    }

    SEM_STORE[semId].value = SEM_STORE[semId].max = n;
    SEM_STORE[semId].active = 1;

    release(&SEM_STORE[semId].lock);

    return semId;
}

int sys_sem_destroy(void){
    int semId;

    argint(0, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_STORE[semId].lock);

    if(!SEM_STORE[semId].active){
        release(&SEM_STORE[semId].lock);
        return -1;
    }

    SEM_STORE[semId].active = 0;

    release(&SEM_STORE[semId].lock);

    return semId;
}

// WARNING: ASSUMES YOU ALREADY HAVE A LOCK.
int _can_acquire(int semId){
    // Somebody messed up.
    if(!SEM_STORE[semId].active){
        panic("_can_acquire");
    }

    int current_value = SEM_STORE[semId].value;

    return current_value > 0;
}

int sys_sem_wait(void){
    int semId;

    argint(0, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_STORE[semId].lock);

    if(!SEM_STORE[semId].active){
        release(&SEM_STORE[semId].lock);
        return -1;
    }

    // We only want to wake up a single one.
    while(!_can_acquire(semId)){
        proc->chan = &SEM_STORE[semId];
        sleep(&SEM_STORE[semId], &SEM_STORE[semId].lock);
        // when sleep returns we have the lock again
    }

    SEM_STORE[semId].value--;

    release(&SEM_STORE[semId].lock);

    // semaphore is good (y)

    return semId;
}

int sys_sem_signal(void){
    int semId;

    argint(0, &semId);

    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_STORE[semId].lock);

    if(!SEM_STORE[semId].active){
        release(&SEM_STORE[semId].lock);
        return -1;
    }

    if(SEM_STORE[semId].value == SEM_STORE[semId].max){
        release(&SEM_STORE[semId].lock);
        return -1;
    }

    // It should be safe to acquire this since if we're calling
    // this we probably got past sem_wait.
    SEM_STORE[semId].value++;
    wakeup(&SEM_STORE[semId]);

    release(&SEM_STORE[semId].lock);

    return 0;
}


int sys_clone(void *(*func) (void *), void *arg, void *stack)
 {
   cprintf("Starting clone\n");
   int i, pid;
   struct proc *np;

   
   //cprintf("Preparing to start sys_clone\n");
   // Allocate process.
   if((np = allocproc()) == 0){
     //cprintf("Failure to allocate process\n");
     return -1;
   }
   
   //cprintf("Succeeded in allocating proccess\n");

   // Copy process state from p.
   if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
     //cprintf("Return -1 for process state from p\n");
     kfree(np->kstack);
     np->kstack = 0;
     np->state = UNUSED;
     return -1;
   }
   //cprintf("Successfully copied process state from p\n");
   np->sz = proc->sz;
   np->parent = proc;
   *np->tf = *proc->tf;
 
   // Clear %eax so that fork returns 0 in the child.
   np->tf->eax = 0;
 
   for(i = 0; i < NOFILE; i++)
     if(proc->ofile[i])
       np->ofile[i] = filedup(proc->ofile[i]);
   np->cwd = idup(proc->cwd);

   safestrcpy(np->name, proc->name, sizeof(proc->name));
 
   pid = np->pid;
   // lock to force the compiler to emit the np->state write last.
   acquire(&ptable.lock);
   np->state = RUNNABLE;
   release(&ptable.lock);


   // Store the function on top of the stack
   // Keep a copy of the original function

   //uint oldEsp = np->tf->eip;




   // Remember to keep track of esp so that we 
   // execute the next instruction: esp + 4
   //
   // Store at esp

   //np->tf->eip = (uint)stack;

   uint userStack = (uint)stack;
   //uint oldEip = np->tf->eip;


   np->kstack[userStack - 4] = (int)func;
   np->kstack[userStack - 8] = (int)arg;
  
   np->tf->eip = np->kstack[userStack-4];
   //np->tf->esp = np->kstack[userStack-8];

   //np->tf->eip = (int)func;

   cprintf("Location of eip %d\n", np->tf->eip);
 
   //np->kstack[esp] = np->tf->eip;
   //np->kstack[esp - 4] = (int)arg;


   //cprintf("PID from clone: %d\n", pid);
   //cprintf("Proc from clone: %d\n", np->parent);
   return pid;

}


int sys_join(int pid, void **stack, void **retval)
{
        cprintf("----- Starting join --------\n");
  	struct proc *p;
	int havekids = 0;


  	acquire(&ptable.lock);

	for(;;)
	{

		cprintf("pid: %d\n", pid);



		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		      //cprintf("For loop for join\n");
		      if(p->parent != proc)
				continue;
		      havekids = 1;
		      if(p->state == ZOMBIE){
			pid = p->pid;
			kfree(p->kstack);
			p->kstack = 0;
			freevm(p->pgdir);
			p->state = UNUSED;
			p->pid = 0;
			p->parent = 0;
			p->name[0] = 0;
			p->killed = 0;
			release(&ptable.lock);

			//uint esp = proc->tf->esp;

			cprintf("EIP is %d\n", proc->tf->eip);

			
			//np->tf->eip = (uint)stack;


			stack = (void*)p->tf->esp;
			retval = (void*) p;
			//uint userStack = (uint)stack;


			cprintf("join: eip %d\n", stack);

		        //np->kstack[userStack - 4] = (int)func;
			//np->kstack[userStack - 8] = (int)arg;
			  
			//np->tf->eip = np->kstack[userStack-4];
			//np->tf->esp = np->kstack[userStack-8];
			
			 
		

			return 0;
		      }
		    }

		if(!havekids || proc->killed){
		  cprintf("Don't have kids || proc was killed\n");
		  release(&ptable.lock);
	          return -1;
	        }
  		sleep(proc, &ptable.lock);  //DOC: wait-sleep
		cprintf("Finished sleeping\n");
	}
        cprintf("join: Exiting joing\n"); 

}






// modify proc table with return value
// match pid
void sys_texit(void *retval)
{

  struct proc *p;
  int fd;

  cprintf("Starting texit!\n");

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }
  //cprintf("texit: Closed all open files\n");


  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;


  //cprintf("texit: Arrived at acquire lock\n");
  acquire(&ptable.lock);

  cprintf("texit: Trying to wake up proc->parent\n");
  cprintf("proc->parent %d\n", proc->parent);
  // Parent might be sleeping in wait().
  wakeup(proc->parent);
  cprintf("texit: Woke up something\n");
  

  acquire(&ptable.lock);
  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

