#include <stdio.h>
#include "defs.h"
#include "spinlock.h"
#include "user.h"

struct semaphore{
    int value;
    int max;
    int active;
    struct spinlock lock;
}

typedef struct semaphore* semaphore;
typedef struct spinlock* spinlock;

#define SEM_NUM 32

semaphore SEM_STORE = semaphore[SEM_NUM];
spinlock SEM_LOCK; 

int sem_init(int semId, int n){
    if(semId < 0 || semId >= SEM_NUM){
        // cprintf("Attempted to initialize semaphore with invalid ID %d.\n", semId);
        return -1;
    }

    acquire(&SEM_LOCK);

    // Check to see if this semaphore has already been initialized.
    if(SEM_STORE[semId] && SEM_STORE[semId]->active){
        release(&SEM_LOCK);
        return -1;
    }

    // Do we need to initialize the semaphore?
    if(!SEM_STORE[semId]){
        SEM_STORE[semId] = (semaphore) malloc(sizeof(*semaphore));
        SEM_STORE[semId]->spinlock = (spinlock) malloc(sizeof(*spinlock));
    }

    SEM_STORE[semId]->value = SEM_STORE[semId]->max = n;
    SEM_STORE[semId]->active = 1;
    SEM_STORE[semId]->spinlock = (spinlock) malloc(sizeof(*spinlock));

    // "sem" + 2 characters for number + '\0'
    char* lock_name = malloc(sizeof(char) * 6);
    sprintf(lock_num, "sem%d", semId);

    initlock(&(SEM_STORE[semId]->spinlock), lock_name);

    release(&SEM_LOCK);

    return semId;
}

int sem_destroy(int semId){
    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_LOCK);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        release(&SEM_LOCK);
        return -1;
    }

    SEM_STORE[semId]->active = 0;
    release(&(SEM_STORE[semId]->spinLock));

    release(&SEM_LOCK);

    return semId;
}

// WARNING: ASSUMES YOU ALREADY HAVE A LOCK.
int _can_acquire(int semId){
    acquire(&SEM_LOCK);

    // Somebody messed up.
    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        panic("_can_acquire");
    }

    int current_value = SEM_STORE[semId]->value;

    release(&SEM_LOCK);

    return current_value > 0;
}

int sem_wait(int semId){
    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_LOCK);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        release(&SEM_LOCK);
        return -1;
    }

    release(&SEM_LOCK);

    // We only want to wake up a single one.
    while(_can_acquire()){
        sleep(&semId, SEM_STORE[semId]->spinlock);
    }

    // Prevent race conditions.
    acquire(&SEM_LOCK);
    SEM_STOqRE[semId]->value--;
    release(&SEM_LOCK);

    return semId;
}

void sem_signal(int semId){
    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_LOCK);

    if(!SEM_STORE[semId] || !SEM_STORE[semId]->active){
        release(&SEM_LOCK);
        return -1;
    }

    if(SEM_STORE[semId]->value == SEM_STORE[semId]->max){
        release(&SEM_LOCK);
        return -1;
    }

    // It should be safe to acquire this since if we're calling
    // this we probably got past sem_wait.
    SEM_STORE[semId]->value++;
    release(&SEM_LOCK);

    wakeup(&usemId);
}
