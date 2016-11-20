#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define NUM_THREADS 2
#define TARGET_COUNT_PER_THREAD 50000
#define SEMAPHORE_NUM 0

uint g_counter;

void *thread(void *arg)
{
	int i;
	printf(1, "{{{JUST ENTERING THREAD %d: started..}}}.\n", *(int*)arg);
	int counter;

	sleep(10);
	printf(1, "thread %d: started...\n", *(int*)arg);

	for (i=0; i<TARGET_COUNT_PER_THREAD; i++) {
		sem_wait(SEMAPHORE_NUM);
		
		counter = g_counter;
		sleep(0);
		counter++;
		sleep(0);
		g_counter = counter;

		sem_signal(SEMAPHORE_NUM);
	}
	//printf(1, "Preparing to texit thread %d \n", *(int*)arg);
        printf(1,"thread: g_counter: %d\n", g_counter);
	texit(arg);
	return 0;
}

int main(int argc, char **argv)
{
	int i, j;
	int passed = 1;
	int sem_size;
	int final_counter;
	int final_target = NUM_THREADS*TARGET_COUNT_PER_THREAD;

	if (argc >= 2)
		sem_size = NUM_THREADS;
	else
		sem_size = 1;

	// Initialize semaphore to 1
	if (sem_init(SEMAPHORE_NUM, sem_size) < 0)
	{
		printf(1, "main: error initializing semaphore %d\n", SEMAPHORE_NUM);
		exit();
	}

	printf(1, "main: initialized semaphore %d to %d\n", SEMAPHORE_NUM, sem_size);

	// Initialize counter
	g_counter = 0;

	// Set up thread stuff

	// Stacks
	void *stacks[NUM_THREADS];
	// Args
	int *args[NUM_THREADS];

	// Allocate stacks and args and make sure we have them all
	// Bail if something fails
	for (i=0; i<NUM_THREADS; i++) {
		stacks[i] = (void*) malloc(4096);
		if (!stacks[i]) {
			printf(1, "main: could not get stack for thread %d, exiting...\n");
			exit();
		}

		args[i] = (int*) malloc(4);
		if (!args[i]) {
			printf(1, "main: could not get memory (for arg) for thread %d, exiting...\n");
			exit();
		}

		*args[i] = i;
	}

	printf(1, "main: running with %d threads...\n", NUM_THREADS);
	
	int pid[NUM_THREADS];
	// Start all children
	for (i=0; i<NUM_THREADS; i++) {
		pid[i] = clone(thread, args[i], stacks[i]);
		printf(1, "main: created thread with pid %d\n", pid[i]);
	}

        //printf(1, "Number of g_counter: %d\n", g_counter);
	
	// Wait for all children
	for (i=0; i<NUM_THREADS; i++) {
		void *joinstack;
		void* retval;
		int r;
		r = join(pid[i], &joinstack, &retval);

		if (r<0){
			passed = 0;
		}
		if(*(int*)retval != i){
			passed = 0;
		}
		for (j=0; j<NUM_THREADS; j++) {
			if (joinstack == stacks[i]) {
				printf(1, "main: thread %d joined...\n", i);
				break;
			}
		}

	}

	// Check the result
	final_counter = g_counter;
	printf(1, "Final counter is %d, target is %d\n", final_counter, final_target);
	if (passed == 0){
		printf(1, "JOIN ERROR, TEST FAILED");
	}
	else if (final_counter == final_target)
		printf(1, "TEST PASSED!\n");
	else
		printf(1, "TEST FAILED!\n");
	
	// Clean up semaphore
	sem_destroy(SEMAPHORE_NUM);

	// Exit
	exit();
}
