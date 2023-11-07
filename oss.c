#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>

struct timer {
	unsigned int seconds;
	unsigned int nanoseconds;
};

struct descriptor {
	int id;
	int request;
	int allocation;
	int release;
};

int main() {
	printf("Main\n");

	key_t key1;


	// generate key and allocate shared memory for clock
	struct timer * clock;
	int clockId;
	key1 = ftok("clockkey", 0);
	if ((clockId = shmget(key1, sizeof(struct timer), IPC_CREAT | 0666)) == -1) {
		perror("shmget ");
		exit(1);
	}
	clock = (struct timer *) shmat(clockId, NULL, 0);
	if (clock == (void *)(-1)) {
		perror("shmat ");
		exit(1);
	}



	// free memory
	shmctl(clockId, IPC_RMID, NULL);
	
	return 0;
}
