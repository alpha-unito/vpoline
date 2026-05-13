#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    switch (pid) {
        case -1:
            perror("fork");
            break;
        case 0:
            printf("Child process: PID = %d\n", getpid());
            exit(EXIT_SUCCESS);
            break;
        default:
            printf("Parent process: PID = %d\n", getpid());
            printf("Parent waiting for child exit...\n");
            int status;
            wait(&status);
            printf("Child exited with status %d\n", WEXITSTATUS(status));
            break;
    }

    return 0;
}
