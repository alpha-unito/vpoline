#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <bits/signum-arch.h>
#include <sys/wait.h>

int child_func(void *arg) {
    int fd = openat(AT_FDCWD, "testfile.txt", O_RDONLY);
    char buf[128];
    int n = read(fd, buf, sizeof(buf));
    buf[n] = '\0';
    n = atoi(buf);
    assert(n == getpid());
    return 0;
}

int main() {
    char child_stack[8192];
    pid_t ppid = getpid();
    pid_t pid = clone(child_func, child_stack + sizeof(child_stack),
                        SIGCHLD, &ppid);

    if (pid == -1) {
        perror("Clone failed\n");
        return 1;
    }


    int status;
    wait(&status);
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
        fprintf(stderr, "Child assertion failed\n");
        return 1;
    }

    char buf[128];
    int fd = openat(AT_FDCWD, "testfile2.txt", O_RDONLY);
    int n = read(fd, buf, sizeof(buf));
    buf[n] = '\0';
    n = atoi(buf);
    assert(n == ppid);

    return 0;
}