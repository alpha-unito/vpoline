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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int main() {
    int fd = openat(AT_FDCWD, "testfile.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    int fd2 = openat(AT_FDCWD, "testfile2.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    int fd2_dup = fcntl(fd2, F_DUPFD, 0);
    char buf[128] = "writing to fd2_dup\n";
	write(fd2_dup, buf, strlen(buf));
    char dst_buf[128];
    int n = read(fd, dst_buf, sizeof(buf));
    dst_buf[n] = '\0';
    assert(strcmp(buf, dst_buf) == 0);
    write(1, "FCNTL TEST - OK\n",16);
	close(fd);
	close(fd2);
	close(fd2_dup);
	return 0;
}
