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

#include <libvpoline.h>

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/syscall.h>

static syscall_no_intercept_t syscall_no_intercept = NULL;

static int hook_function(long syscall_number, long a0, long a1,
                    long a2, long a3, long a4, long a5, long *result)
{
    if (syscall_number == SYS_openat) {
        const char non_existing[] = "non_existing.txt";
        const char *tmp = non_existing;
        if (strcmp((char *)a1, tmp) == 0) {
            const char testfile[] = "testfile.txt";
            long flags = O_WRONLY;
            *result = syscall_no_intercept(SYS_openat, a0, (uintptr_t)testfile,
                                           flags, a3, a4, a5);
            return 0;
        }
    }
    return 1;
}

int __hook_init(long placeholder __attribute__((unused)),
        void *no_intercept_ptr,
        void **out_hook_ptr,
        void **out_post_clone_child_ptr,
        void **out_post_clone_parent_ptr)
{

    syscall_no_intercept = (syscall_no_intercept_t)no_intercept_ptr;
    *out_hook_ptr= (void *)hook_function;
    return 0;
}