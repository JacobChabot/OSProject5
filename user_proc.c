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
#include <sys/msg.h>

struct descriptor { // some resources may be shareable
        int id; // resource id
        int inventory; // total number of resources
        int instances;
	int request[5];
        int allocation;
        int release;
};

// message queue struct
struct reqMsg {
        int processNum;
        int count;
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
	
	// create message queue
        key_t key3 = ftok("./Makefile", 0);
        int msgId = msgget(key3, IPC_CREAT | 0666);
        if (msgId == -1) {
                perror("msgget");
                exit(1);
        }

	srand(time(NULL));
	int maximumClaim = (rand() % 10) + 1;

	// process should claim resource between 1 and 250 ms
	struct timeval start_time, current_time;
	struct reqMsg message;
	gettimeofday(&start_time, NULL);
	srand(time(NULL));
	int msRandom = (rand() % 250) + 1; // generate random number between 1 and 250 milliseconds
	usleep(msRandom * 1000);
	bool resAcq = false; // resource acquired is false

	//jacob: processes will sometimes get stuck in loop and not request resources 
	do {
		gettimeofday(&current_time, NULL);
		printf("user proc %d sending request\n", j);
		// Send resource request
    		message.processNum = j; // send local process number
		message.count = maximumClaim; // send maximum claim (request)
		if (msgsnd(msgId, &message, sizeof(struct reqMsg) - sizeof(long), 0) == -1) {
        		perror("msgsnd");
        		exit(EXIT_FAILURE);
    		}
		/*
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
		*/	
	} while (false); 

	printf("child process %d finished.\n", j);

	return 0;
}
