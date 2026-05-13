#include <unistd.h>

int main() {
    char buf[64] = "Hello, World!\n";
    write(1, buf, sizeof(buf));
    return 0;
}
