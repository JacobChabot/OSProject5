#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/msg.h>

#define MAX_PROC 7
#define MAX_RES 20
pid_t pidArray[MAX_PROC];
volatile bool signalReceived = false;

// signal handler for CTRL C
void intHandler(int signal) {
	printf("\n\nTerminating children, freeing memory and exiting....\n");
	signalReceived = true;
}

struct timer {
	unsigned int seconds;
	unsigned int nanoseconds;
};

struct descriptor { // some resources may be shareable
	int id; // resource id
	int inventory; // total number of resources
	int instances; // number of instances for one resource
	int request[MAX_PROC]; 
	int allocation;
	int release;
};

// implement allocatoin and request n x m matrices where n is number of processes
// and m is number of resources
struct graph {
	int allocation[MAX_PROC][MAX_RES];
	int request[MAX_PROC][MAX_RES];
	int available[MAX_RES];
}resource;

// message queue struct
struct reqMsg {
	int processNum;
	int instances;
	int resource;
};

// return time in milliseconds, used to fork processes at multiple intervals 
long long timeDiff(struct timeval start, struct timeval end) {
	return (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_usec - start.tv_usec) / 1000LL;
}

void forkChild(int);
bool reqAvail(const int *, const int *, const int, const int);
bool deadlock(const int *, const int, const int, const int *, const int *);
void printGraph();

int main(int argc, char* argv[]) {
	printf("Main\n");

	// generate key and allocate shared memory for clock
	struct timer * clock;
	int clockId;
	key_t key1 = ftok("clockkey", 0);
	if ((clockId = shmget(key1, sizeof(struct timer), IPC_CREAT | 0666)) == -1) {
		perror("clock shmget");
		exit(1);
	}
	clock = (struct timer *) shmat(clockId, NULL, 0);
	if (clock == (void *)(-1)) {
		perror("clock shmat");
		exit(1);
	}
	clock->seconds = 0; // set initial values of clock
	clock->nanoseconds = 0;
	unsigned int timeTemp = time(NULL);

	// processes PID array memory allocation
	int shmPid;
	key_t key2 = ftok("./oss.c", 0);
	if ((shmPid = shmget(key2, 5*sizeof(pid_t), IPC_CREAT | 0666)) == -1) {
                perror("pid shmget");
                exit(1);
        }
	pid_t * pid = shmat(shmPid, NULL, 0);
	if (pid == (int *)(-1)) {
                perror("pid shmat");
                exit(1);
        }

	// generate key and allocate shared memory for resource descriptors
	struct descriptor * rd;
	int descId;
	key_t key3 = ftok("./user_proc.c", 0);
	if ((descId = shmget(key3, 5*sizeof(struct descriptor), IPC_CREAT | 0666)) == -1) {
		perror("oss desc shmget");
		exit(1);
	}
	rd = (struct descriptor *) shmat(descId, NULL, 0);
	if (rd == (void *)(-1)) {
		perror("oss desc shmat");
		exit(1);
	}
	
	// set values for graph
	for (int i = 0; i < MAX_PROC; i++)
                for (int j = 0; j < MAX_RES; j++) {
                        resource.allocation[i][j] = 0; // set initialize values of resouce allocation graph to -1
                        resource.request[i][j] = -1;
                }

	// loop through resource descriptor assigning values
	int i;
	srand(time(NULL));
	for (i = 1; i <= MAX_RES; i++) {
		rd[i].id = i;
		rd[i].allocation = -1; // process is currently free
		rd[i].inventory = 20; // set total number of resources to 20
		rd[i].instances = (rand() % 10) + 1; // number of instances between 1 and 10

		resource.available[i-1] = rd[i].instances;	// set currently available resources to total instances for that resource
	}
	
	printGraph();

	// create message queue
	key_t key4 = ftok("./Makefile", 0);
	int msgId = msgget(key4, IPC_CREAT | 0666);
	if (msgId == -1) {
		perror("oss msgget");
		exit(1);
	}

	struct timeval start_time, current_time;
	struct reqMsg message;
	srand(time(NULL));
	int random = (rand() % 500) + 1; // generate random number between 1 and 500 milliseconds
	int processCounter = 0;
	gettimeofday(&start_time, NULL);
	
	// fork randomly between 1 and 500 milliseconds
	do {
		timeTemp = time(NULL) - timeTemp;
       	 	clock->seconds = timeTemp;
        	clock->nanoseconds = timeTemp * 1e9; // keep track of time for clock
		
		gettimeofday(&current_time, NULL);

		// randomly fork if random is equal to the current ms time and n number of processes have not been executed
		if (random == timeDiff(start_time, current_time) && processCounter != MAX_PROC) {
			forkChild(processCounter);
			
			int random = (rand() % 500) + 1; // generate new number
			processCounter = processCounter + 1; // increment process counter
        		sleep(1);

			gettimeofday(&start_time, NULL); // reset milliseconds timer
		}	
        
	} while (processCounter < MAX_PROC);
	
	// main loop
	processCounter = 0;
	int j;
	do {
		
		// accept signal and if signal received, loop through processes to terminate
                signal(SIGINT, intHandler);
                if (signalReceived == true) {
                        for (i = 0; i < MAX_PROC; i++) {
                                kill(pid[i], SIGTERM);
                        }
                        shmctl(clockId, IPC_RMID, NULL);
        		shmctl(shmPid, IPC_RMID, NULL);
       	 		shmctl(descId, IPC_RMID, NULL);
			break;
                }

		if (processCounter < MAX_PROC) {
                	// look for message from processes requesting resources
                	if (msgrcv(msgId, &message, sizeof(struct reqMsg) - sizeof(long), 0, 0) == -1) {
                        	perror("msgrcv");
                        	exit(1);
                	}
			processCounter++;

			printf("\nRequest received from process %d requesting %d instances of resource %d.\n", message.processNum, message.instances, message.resource);
                	resource.request[message.processNum][message.resource] = message.instances; // set request matrix to incoming request
		}
		
		// check for deadlock, if ok to proceed, send signal to process to continue and update structures
		if (deadlock(resource.available, MAX_RES, MAX_PROC, &resource.request[0][0], &resource.allocation[0][0])) {
			if (resource.allocation[message.processNum][message.resource] == 0) { // if resource is not in use, proceed
				printf("Process can continue\n");
				
				resource.allocation[message.processNum][message.resource] = message.instances;
                                resource.available[message.resource] = resource.available[message.resource] - message.instances;
                                rd[message.resource].allocation = message.processNum;
				if (kill(pid[message.processNum], SIGCONT) == -1) { // signal to process it can continue
                                perror("kill");
                                exit(EXIT_FAILURE);
                        	}

				sleep(1); // sleep to allow process to simulate work
				
				// reset data structures
				resource.available[message.resource] = rd[message.resource].instances;
				resource.allocation[message.processNum][message.resource] = 0;
				resource.request[message.processNum][message.resource] = -1;
			}
			else { // have process loop back and send a new request
				if (kill(pid[message.processNum], SIGCONT) == -1) { // signal to process it can continue
                                perror("kill");
                                exit(EXIT_FAILURE);
                                }
				resource.request[message.processNum][message.resource] = -1;
			}	

		}
	
		// loop through request matrix and if all requests are empty, break loop
		int j;
		for (i = 0; i < MAX_PROC; i++) {
                	for (j = 0; j < MAX_RES; j++) {
                        	if (resource.request[i][j] == -1)
					break;
                	}
         
        	}

	} while (i == MAX_PROC && j == MAX_RES);



	for (i = 0; i < MAX_PROC; i++)
		wait(0); // wait for child

	// free memory
	shmctl(clockId, IPC_RMID, NULL);
	shmctl(shmPid, IPC_RMID, NULL);
	shmctl(descId, IPC_RMID, NULL);
	
	printGraph();

	printf("\noss finished\n");
	return 0;
}

bool reqAvail(const int * req, const int * avail, const int pnum, const int num_res) {
	int i;
	for(i = 0; i < num_res; i++) {
		if (req[pnum*num_res+i] > avail[i])
			break;
	}
	return (i == num_res); 
}
// m = num of resources
// n = num of processes
bool deadlock(const int * available, const int m, const int n, const int * request, const int * allocated) {
	int work[m];
	bool finish[n];

	for (int i = 0; i < m; work[i] = available[i++]);
	for (int i = 0; i < n; finish[i++] = false);

	int p;
	for (p = 0; p < n; p++) {
		if (finish[p]) 
			continue;
		if (reqAvail(request, work, p, m)) {
			finish[p] = true;
			for (int i = 1; i < m; i++)
				work[i] += allocated[p*m+i];
			p = -1;
		}
	}

	for (p = 0; p < n; p++) 
		if(! finish[p])
			break;

	return (p != n);
}

void forkChild(int processCounter) { // function to fork and execute child process
        // convert variables to chars to pass to child processes
        char temp[10];
        snprintf(temp, sizeof(temp), "%d", processCounter);

        pid_t pid;
        pid = fork();

        if (pid == 0) {
                //child
                if (execl("./user_proc.out", "./user_proc.out", temp, NULL) == -1) {
                        perror("execl");
                        exit(1);
                }
                exit(1);
        }
}

void printGraph() {
	
	// print resource graph
        printf("Allocation:\n");
        for (int i = 0; i < MAX_PROC; i++) {
        	for (int j = 0; j < MAX_RES; j++) {
            	printf(" %d ", resource.allocation[i][j]);
        	}
        printf("\n");
    	}
        printf("\n");

	printf("Request:\n");
        for (int i = 0; i < MAX_PROC; i++) {
        	for (int j = 0; j < MAX_RES; j++) {
            		printf(" %d ", resource.request[i][j]);
        	}
        	printf("\n");
    	}
        printf("\n");
        
	printf("Avaialble:\n");
        for (int i = 0; i < MAX_RES; i++)
                printf(" %d ", resource.available[i]);
        printf("\n\n");
}

