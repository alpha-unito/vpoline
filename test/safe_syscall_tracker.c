#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);

static syscall_fn_t syscall_no_intercept = NULL;

/**
 * User can define the hook_function according to their needs. Here system call
 * number is printed on stdout as a demo. However, user needs to be aware of
 * how the hook_function return value is used by libvpoline. 1 means that the
 * system call execution is delegated to libvpoline, while 0 means making
 * libvpoline free to assume that the system call has been issued here in
 * hook_function by using syscall_intercept.
 * Clone must not be issued here in hook_function since it is a common C coded
 * function and compiler-generated epilogue is likely to cause segmentation
 * fault.
 *
 * @return always 1, as for now.
 * TODO: 1 to make libvpoline to execute the original system call,
 *  0 to communicate to libvpoline that the original system call has been
 *  already handled by user defined hook_function
 */

static void my_print_syscall(long syscall_number, long a3, long a4, long a5) {
    char buf[64];
    char *p = buf + sizeof(buf) - 1;
    *p = '\n';

    long n = syscall_number;
    if (n == 0) {
        *--p = '0';
    } else {
        while (n > 0) {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }

    const char *prefix = "output from hook_function: syscall number ";
    const int prefix_len = 42;
    p -= prefix_len;
    for (int i = 0; i < prefix_len; i++) {
        p[i] = prefix[i];
    }

    syscall_no_intercept(SYS_write, 1, (uintptr_t)p, (buf + sizeof(buf)) - p, a3, a4, a5);
}

/**
 * CAREFUL! THE SYSTEM CALL IS CURRENTLY EXECUTED BY DEFAULT BY LIBVPOLINE!
 * THERE IS NO WAY TO TELL LIBVPOLINE THAT THE SYSTEM CALL HAS BEEN ALREADY
 * EXECUTED HERE BY HOOK FUNCTION
 */
static long hook_function(long syscall_number, long a0, long a1,
              long a2, long a3, long a4, long a5)
{
    my_print_syscall(syscall_number, a3, a4, a5);
    return 1;
}

int __hook_init(long placeholder __attribute__((unused)),
        void *sys_call_hook_ptr)
{
    printf("output from __hook_init: we can do some init work here\n");

    syscall_no_intercept = *((syscall_fn_t *) sys_call_hook_ptr);
    *((syscall_fn_t *) sys_call_hook_ptr) = hook_function;
    return 0;
}