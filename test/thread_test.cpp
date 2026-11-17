#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <thread>
#include <unistd.h>

void thread_worker(const int *num) {
    // Verify argument passed to std::thread
    assert(*num == 12345);

    int fd = openat(AT_FDCWD, "testfile.txt", O_RDONLY);
    char buf[128];
    int n = read(fd, buf, sizeof(buf));
    buf[n] = '\0';
    n = atoi(buf);
    assert(n == getpid());
}

int main() {
    int *num = static_cast<int *>(malloc(sizeof(int)));
    *num = 12345;

    pid_t ppid = getpid();

    std::thread t(thread_worker, num);
    t.join();


    char buf[128];
    int fd = openat(AT_FDCWD, "testfile2.txt", O_RDONLY);
    int n = read(fd, buf, sizeof(buf));
    buf[n] = '\0';
    n = atoi(buf);
    assert(n == ppid);

    free(num);
    return 0;
}