#include <stdio.h>
#include "defs.h"
#include "spinlock.h"
#include "user.h"

struct semaphore{
    int value;
    int max;
    int active;
    struct spinlock lock;
};

#define SEM_NUM 32

struct semaphore* SEM_STORE[SEM_NUM];

void _sem_init(){
    int n;

    for(n = 0; n < SEM_NUM; n++){
        SEM_STORE[semId] = (struct semaphore*) malloc(sizeof(*SEM_STORE[semId]));
        SEM_STORE[semId]->lock = (struct spinlock*) malloc(sizeof(struct spinlock));

        // "sem" + 2 characters for number + '\0'
        char* lock_name = (char*) malloc(sizeof(char) * 6);
        sprintf(lock_name, "sem%d", semId);

        initlock(&(SEM_STORE[semId]->lock), lock_name);
    }
}

int sem_init(int semId, int n){
    if(semId < 0 || semId >= SEM_NUM){
        // cprintf("Attempted to initialize semaphore with invalid ID %d.\n", semId);
        return -1;
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

int sem_destroy(int semId){
    if(semId < 0 || semId >= SEM_NUM){
        return -1;
    }

    acquire(&SEM_STORE[semId]->lock);

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

int sem_wait(int semId){
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

int sem_signal(int semId){
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
}

/*
proc->chan = chan;
if proc.chan == chan:
    wake

chan can just be the address of the semaphore being used

sem_wait(sem_id){
    // check the counter to see if we can lock the smeahphore

    while not can take sem
}
*/
