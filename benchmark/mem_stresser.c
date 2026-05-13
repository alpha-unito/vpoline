#include <stdlib.h>
#include <string.h>

/* more MiB than the cache size of our testing board */
#define ARRAY_SIZE (32 * 1024 * 1024)

#define STEP 64
#define ITERATIONS 100

int main() {

    volatile char *buffer = malloc(ARRAY_SIZE);
    if (!buffer) return 1;

    memset((void*)buffer, 0, ARRAY_SIZE);

    /* each iteration is supposed to cause a cache miss */
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < ARRAY_SIZE; i += STEP) {
            buffer[i] += 1;
        }
    }

    free((void*)buffer);
    return 0;
}