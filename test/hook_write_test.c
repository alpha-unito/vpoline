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

#include <stdio.h>
#include <sys/syscall.h>

static syscall_no_intercept_t syscall_no_intercept = NULL;

static long hook_function(long syscall_number, long a0, long a1,
                    long a2, long a3, long a4, long a5, long *result)
{
    (void) a0;
    (void) a2;
    (void) a3;
    (void) a4;
    (void) a5;

    if (syscall_number == SYS_write) {
        const char interc[] = "intercepted_";
        const char *src = interc;

        /* write(fd, buf, len) */
        size_t len = (size_t)a2;
        char *buf = (char *)a1;

        if (len > sizeof(interc)) {
            while (*src != '\0')
                *buf++ = *src++;
        }
        *result = syscall_no_intercept(syscall_number, a0, a1, a2);
        return 0;
    }
    return 1;
}

int __hook_init(long placeholder __attribute__((unused)),
        void *no_intercept_ptr, void **out_hook_ptr)
{
    printf("output from __hook_init: we can do some init work here\n");

    syscall_no_intercept = (syscall_no_intercept_t)no_intercept_ptr;
    *out_hook_ptr= (void *)hook_function;
    return 0;
}
