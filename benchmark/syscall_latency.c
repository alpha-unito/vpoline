#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <math.h>

#define ITERATIONS 1000

pid_t thread_id;

FILE* out_file;

typedef struct {
    double mean;
    double stddev;
} Stats;

Stats compute_stats(unsigned long long *data, size_t size) {
    Stats s = {0.0, 0.0};
    if (size <= 1) return s;

    // Calcolo della media
    double sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        sum += (double)data[i];
    }
    s.mean = sum / size;

    // Calcolo della deviazione standard campionaria
    double variance_sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        variance_sum += ((double)data[i] - s.mean) * ((double)data[i] - s.mean);
    }
    s.stddev = sqrt(variance_sum / (size - 1));

    return s;
}

void run_benchmark(const char *test_name, int repetitions, long syscallno, long arg0, long arg1, long arg2, long arg3, long arg4, long arg5) {
    struct timespec start, end;
    unsigned long long *measurements = malloc(repetitions * sizeof(unsigned long long));

    for (int i = 0; i < repetitions; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        syscall(syscallno, arg0, arg1, arg2, arg3, arg4, arg5);
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (syscall(SYS_gettid) != thread_id) {
            exit(EXIT_SUCCESS);
        }

        unsigned long long start_ns = (unsigned long long)start.tv_sec * 1000000000ULL + start.tv_nsec;
        unsigned long long end_ns   = (unsigned long long)end.tv_sec * 1000000000ULL + end.tv_nsec;
        measurements[i] = end_ns - start_ns;
    }

    Stats s = compute_stats(measurements, repetitions);

    fprintf(out_file,"  {\n");
    fprintf(out_file,"    \"name\": \"%s\",\n", test_name);
    fprintf(out_file,"    \"avg_ns\": %.2f,\n", s.mean);
    fprintf(out_file,"    \"stddev_ns\": %.2f\n", s.stddev);
    // fprintf(ou_file,"    \"measures\": [");
    // for (int i = 0; i < repetitions; i++) {
    //     fprintf(ou_file,"%llu%s", measurements[i], (i == repetitions - 1) ? "" : ", ");
    // }
    // fprintf(out_file,"]\n");
    fprintf(out_file,"  }");

    free(measurements);
}

int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        perror("You must provide a file as first argument to store benchmark results\n");
        return EXIT_FAILURE;
    }

    out_file = fopen(argv[1], "w");
    if (!out_file) {
        perror("Error opening output file\n");
        return EXIT_FAILURE;
    }

    setvbuf(out_file, NULL, _IONBF, 0);

    thread_id = syscall(SYS_gettid);

    fprintf(out_file,"[\n");

    run_benchmark("read", ITERATIONS, SYS_read, 0, 0, 0, 0, 0, 0);
    fprintf(out_file,",\n");
    run_benchmark("write", ITERATIONS, SYS_write, 0, 0, 0, 0, 0, 0);
    fprintf(out_file,",\n");
    run_benchmark("openat", ITERATIONS, SYS_openat, 0, 0, 0, 0, 0, 0);
    fprintf(out_file,",\n");
    run_benchmark("close", ITERATIONS, SYS_close, 0, 0, 0, 0, 0, 0);
    fprintf(out_file,",\n");
    run_benchmark("newfstatat", ITERATIONS, SYS_newfstatat, 0, 0, 0, 0, 0, 0);
    fprintf(out_file,",\n");
    run_benchmark("clone", ITERATIONS, SYS_clone, 0, 0, 0, 0, 0, 0);

    fprintf(out_file,"\n]\n");

    fclose(out_file);

    return 0;
}