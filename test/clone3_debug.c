#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <stdint.h>

int child_func(void *arg) {
    pid_t ppid = *(pid_t *)arg;
    pid_t child_pid = getpid();
    printf("Child pid = %d, parent pid = %d\n", child_pid, ppid);
    return 0;
}

int main() {
    pid_t ppid = getpid();
    printf("Parent pid = %d\n", ppid);


    struct clone_args args = {0};

    args.flags = 0;

    args.exit_signal = SIGCHLD;

    args.stack = 0;
    args.stack_size = 0;

    pid_t pid = syscall(SYS_clone3, &args, sizeof(args));

    if (pid == -1) {
        perror("Clone3 failed");
        return 1;
    }

    if (pid == 0) {
        /*
         * mimicking clone glibc wrapper by calling the function child thread
         * should execute
         */
        int ret = child_func(&ppid);

        exit(ret);
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