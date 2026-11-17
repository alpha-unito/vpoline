#include <libvpoline.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

static syscall_no_intercept_t syscall_no_intercept = NULL;

static long hook_function(long syscall_number, long a0, long a1,
                          long a2, long a3, long a4, long a5, long *result)
{
    if (syscall_number == SYS_clone || syscall_number == SYS_clone3) {
        openat(AT_FDCWD, "testfile.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
        openat(AT_FDCWD, "testfile2.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
    }
    return 1;
}

static void post_clone_child(void)
{
    int fd = openat(AT_FDCWD, "testfile.txt", O_WRONLY);
    dprintf(fd, "%d\n", getpid());
}

static void post_clone_parent(long child_tid)
{
    int fd = openat(AT_FDCWD, "testfile2.txt", O_WRONLY);
    dprintf(fd, "%d\n", getpid());
}

int __hook_init(long placeholder __attribute__((unused)),
                void *no_intercept_ptr,
                void **out_hook_ptr,
                void **out_post_clone_child_ptr,
                void **out_post_clone_parent_ptr)
{
    syscall_no_intercept = (syscall_no_intercept_t)no_intercept_ptr;
    *out_hook_ptr = (void *)hook_function;
    if (out_post_clone_child_ptr != NULL)
        *out_post_clone_child_ptr = (void *)post_clone_child;
    if (out_post_clone_parent_ptr != NULL)
        *out_post_clone_parent_ptr = (void *)post_clone_parent;
    return 0;
}