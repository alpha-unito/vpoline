/**
 * Copyright 2026 University of Turin
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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

int main() {
    int test_fd = openat(AT_FDCWD, "testfile.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (test_fd == -1) {
        printf("intercepted_openat error nr: %d\n", errno);
        return 1;
    }
    char buf[128] = "original_syscall\n";
    write(test_fd, buf, strlen(buf));
    lseek(test_fd, 0, SEEK_SET);
    char test_buf[128];
    int n = read(test_fd, test_buf, strlen(buf));
    test_buf[n] = '\0';
    assert(strcmp(test_buf, "intercepted_call\n") == 0);
    char test_ok[128] = "garbagecharsWRITE TEST - OK\n";
    write(1, test_ok, strlen(test_ok));
    return 0;
}