#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sched.h>

int child_func(void *arg) {
    pid_t ppid = *(pid_t *)arg;
    pid_t child_pid = getpid();
    printf("Child pid = %d, parent pid = %d\n", child_pid, ppid);
    return 0;
}

int main() {
    char child_stack[8192];
    pid_t ppid = getpid();
    printf("Parent pid = %d\n", ppid);
    pid_t pid = clone(child_func, child_stack + sizeof(child_stack),
                        SIGCHLD, &ppid);

    if (pid == -1) {
        perror("Clone failed\n");
        return 1;
    }

    printf("Parent waiting for child exit...\n");

    int status;
    wait(&status);
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
        fprintf(stderr, "Child assertion failed\n");
        return 1;
    }

    printf("Child thread terminated successfully\n");

    return 0;
}
