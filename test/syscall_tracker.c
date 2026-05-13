#include <libvpoline.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

static syscall_no_intercept_t syscall_no_intercept = NULL;

static long hook_function(long syscall_number, long a0, long a1,
              long a2, long a3, long a4, long a5, long *result)
{
    char buf[128];
    sprintf(buf, "output from hook_function: syscall number %ld\n", syscall_number);
    syscall_no_intercept(SYS_write, 1, (uintptr_t)buf, strlen(buf));
    return 1;
}

int __hook_init(long placeholder __attribute__((unused)),
        void *no_intercept_ptr, void **out_hook_ptr)
{
    printf("output from __hook_init: we can do some init work here\n");

    syscall_no_intercept = (syscall_no_intercept_t)no_intercept_ptr;
    *out_hook_ptr = (void *)hook_function;
    return 0;
}