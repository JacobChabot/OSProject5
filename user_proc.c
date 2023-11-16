#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

/*
void signalHandler(int signal) {
	printf("signal handler\n");
	if (signal == SIGSTOP) {
        printf("Child process received SIGSTOP. Pausing...\n");
        pause();  // Wait for SIGCONT
        printf("Child process resumed.\n");
    	} 
	else if (signal == SIGCONT) {
        // Nothing to do here; just wake up from pause()
    	}
}
*/

struct descriptor { // some resources may be shareable
	int id; // resource id
	int inventory; // total number of resources
	int request; 
	int allocation;
	int release;
	int queue; // processes waiting on the resource
};

long long timeDiff(struct timeval start, struct timeval end) {
	return (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_usec - start.tv_usec) / 1000LL;
}

int main(int argc, char* argv[]) {
	
	const int j = atoi(argv[1]); // own process number
	printf("process %d\n", j);


	int shmPid;
        key_t key1 = ftok("./oss.c", 0);
        if ((shmPid = shmget(key1, sizeof(pid_t), IPC_CREAT | 0666)) == -1) {
                perror("pid shmget");
                exit(1);
        }
        int * pid = shmat(shmPid, NULL, 0);
        if (pid == (int *)(-1)) {
                perror("clock shmat");
                exit(1);
        }
	pid[j] = getpid();
	
	// generate key and allocate shared memory for resource descriptors
	struct descriptor * rd;
	int descId;
	key_t key2 = ftok("./user_proc.c", 0);
	if ((descId = shmget(key2, 5*sizeof(struct descriptor), IPC_CREAT | 0666)) == -1) {
		perror("desc shmget");
		exit(1);
	}
	rd = (struct descriptor *) shmat(descId, NULL, 0);
	if (rd == (void *)(-1)) {
		perror("desc shmat");
		exit(1);
	}

	// process should claim resource between 1 and 250 ms
	struct timeval start_time, current_time;
	gettimeofday(&start_time, NULL);
	srand(time(NULL));
	int msRandom = (rand() % 250) + 1; // generate random number between 1 and 250 milliseconds
	usleep(msRandom * 1000);
	bool resAcq = false; // resource acquired

	//jacob: processes will sometimes get stuck in loop and not request resources 
	do {
		gettimeofday(&current_time, NULL);
		if (msRandom == timeDiff(start_time, current_time) && resAcq == false) { // take control of a random resource, simulate some work then release
			int resRandom = (rand() % 20) + 1; // random resource between 1 and 20
			if (rd[resRandom].allocation == -1) { // resource is free
				
				rd[resRandom].allocation = j; // set allocation for current process
				printf("Resource %d allocated to process %d\n", rd[resRandom].id, j);

				usleep(msRandom * 1000); //sleep for msRandom milliseconds to similuate work (usleep accepts microseconds so have to multiply by 1000)

				rd[resRandom].allocation = -1; // set alloation to -1 to free

				resAcq = true;
			}
			else 
				printf("Process %d attempted to acquired Resource %d\n", j, resRandom);

			usleep(500000); //sleep for 500 milliseconds to reduce strain on system
		}	
	} while (resAcq == false); 

	printf("child process %d finished.\n", j);

	return 0;
}
