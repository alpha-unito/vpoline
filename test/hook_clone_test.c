/**
 * Copyright 2026 University of Turin
 * Copyright 2021 Kenichi Yasukata
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>

#include "../include/libvpoline.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

static syscall_no_intercept_t syscall_no_intercept = NULL;

static long hook_function(long syscall_number, long a0, long a1,
                    long a2, long a3, long a4, long a5, long *result)
{
    if (syscall_number == SYS_clone) {
        openat(AT_FDCWD, "testfile.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
        openat(AT_FDCWD, "testfile2.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
    }
    return 1;
}

static void post_clone_child()
{
    int fd = openat(AT_FDCWD, "testfile.txt", O_WRONLY);
    dprintf(fd, "%d\n", getpid());
}

static void post_clone_parent(long a0)
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
    *out_hook_ptr= (void *)hook_function;
    if (out_post_clone_child_ptr != NULL)
        *out_post_clone_child_ptr = (void *)post_clone_child;

    if (out_post_clone_parent_ptr != NULL)
        *out_post_clone_parent_ptr = (void *)post_clone_parent;
    return 0;
}
