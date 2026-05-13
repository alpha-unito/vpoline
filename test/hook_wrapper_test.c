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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <errno.h>

#include "wrapper_test.h"

static syscall_no_intercept_t syscall_no_intercept = NULL;

static long hook_function(long syscall_number, long a0, long a1,
              long a2, long a3, long a4, long a5, long *result)
{
    /* we're printing the final report */
    if (syscall_number == SYS_write && a0 == 1) return 1;
    /* we don't want to intercept the test executable exit */
    if (syscall_number == SYS_exit_group || syscall_number == SYS_exit) {
        return 1;
    }
    /*
     * our coverage test must simply signal that the ECALL occurrences within
     * glibc wrappers are expectedly diverted to this hook_function; it returns
     * -ENOSYS so that the glibc doesn't get tricked into thinking the system
     * call was actually executed by kernel as this would surely cause segfaults
     * into the test executables
     */
    if (inside_test) {
        char buf[128];
        sprintf(buf, "output from hook_function: syscall number %ld\n", syscall_number);
        syscall_no_intercept(SYS_write, 1, (uintptr_t)buf, strlen(buf));
        if (current_test_idx < MAX_TESTS) {
            if (tracker[current_test_idx].matched == 0) {
                if (syscall_number == tracker[current_test_idx].expected_sys_nr) {
                    tracker[current_test_idx].matched = 1;
                }
            }
        }
        *result = -ENOSYS;
        return 0;
    }
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