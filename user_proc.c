#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
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

int main() {
	
	int shmPid;
        key_t key2 = ftok("./oss.c", 0);
        if ((shmPid = shmget(key2, sizeof(pid_t), IPC_CREAT | 0666)) == -1) {
                perror("pid shmget");
                exit(1);
        }
        int * pid = shmat(shmPid, NULL, 0);
        if (pid == (int *)(-1)) {
                perror("clock shmat");
                exit(1);
        }
	pid[0] = getpid();
	
	int i;
	for (i = 0; i < 5; i++) {
		printf("child process %d executing\n", pid[0]);
		sleep(1);
	}
	return 0;
}
