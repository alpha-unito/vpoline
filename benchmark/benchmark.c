#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>

#ifdef DEBUG
    #define N_ITERATIONS 10
#else
    #define N_ITERATIONS 1000
#endif

#define     ITERATIONS N_ITERATIONS
#define WARMUP 10
#define JUNK_FILE "./sha256sum_benchmark_junk.bin"
#define LIBZPOLINE "/home/omonticelli/riscvpoline/libzpoline.so"
#define LIBZPHOOK "/home/omonticelli/riscvpoline/apps/basic/libzphook_basic.so"
#define SUD "/home/omonticelli/riscvpoline/benchmark/libsud_custom.so"
#define LIBZPOLINE_PATH "/home/omonticelli/riscvpoline/libzpoline.so"
#define TESTED_EXE "./mem_stresser"


typedef struct {
    double mean_ms;
    double stddev_ms;
} BenchResult;

/* executes command and measures run time */
double run_and_measure(const char *path, char *const argv[], int use_zpoline, int use_sud) {
    struct timespec start, end;
    pid_t pid;
    int status;

    /* start time */
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid = fork();
    if (pid == 0) {

        /* avoid prints on stdout to save time */
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        if (use_zpoline) {
            setenv("LD_PRELOAD", LIBZPOLINE, 1);
        } else if (use_sud) {
            setenv("LD_PRELOAD", SUD, 1);
        }

        execv(path, argv);

        perror("failed execv");
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
    } else {
        perror("failed fork");
        exit(1);
    }

    /* end time */
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    return elapsed_ns / 1e6;
}

/* cache warmup, executes ITERATIONS runs and computes stats */
BenchResult run_benchmark(const char *name, const char *path, char *const argv[], int use_zpoline, int use_sud) {

    int iterations = ITERATIONS;
    int warmup = WARMUP;
    if (strcmp(path, "/usr/bin/strace") == 0 &&
        strcmp(TESTED_EXE, "/usr/bin/dd") == 0) {
        /* strace is much slower, reduce to a reasonable number of iterations */
        iterations = ITERATIONS / 10;
        warmup = WARMUP / 10;
    }

    double *times = malloc(sizeof(double) * iterations);
    double sum = 0.0;

    printf("Cache warmup for %s...\n", name);
    for (int i = 0; i < warmup; i++) {
#ifdef DEBUG
        printf("Running warmup iteration %d...\n", i);
#endif
        run_and_measure(path, argv, use_zpoline, use_sud);
    }

    printf("Measuring %s (%d iterations)...\n", name, iterations);
    for (int i = 0; i < iterations; i++) {
#ifdef DEBUG
        printf("Running iteration %d...\n", i);
#endif
        times[i] = run_and_measure(path, argv, use_zpoline, use_sud);
        sum += times[i];
    }

    double mean = sum / iterations;

    double variance_sum = 0.0;
    for (int i = 0; i < iterations; i++) {
        variance_sum += (times[i] - mean) * (times[i] - mean);
    }
    double stddev = sqrt(variance_sum / iterations);

    free(times);
    BenchResult res = {mean, stddev};
    return res;
}

void create_junk_file() {
    printf("Creating 100 MB junk file...\n");

    int fd = open(JUNK_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error: junk file creation failed");
        exit(1);
    }

    size_t buf_size = 1024 * 1024; // 1 Megabyte
    char *buffer = malloc(buf_size);
    if (!buffer) {
        perror("Error: buffer allocation failed");
        close(fd);
        exit(1);
    }

    memset(buffer, 0xAB, buf_size);

    for (int i = 0; i < 100; i++) {
        if (write(fd, buffer, buf_size) != buf_size) {
            perror("Error: writing to junk file failed");
            free(buffer);
            close(fd);
            exit(1);
        }
    }

    free(buffer);
    close(fd);
    printf("Successfully generated file. Start benchmark...\n\n");
}

int main() {

    char *ls_argv[] = {"/usr/bin/ls", "-l", ".", NULL};
    char *dd_argv[] = {"/usr/bin/dd", "if=/dev/zero", "of=/dev/null", "bs=4k", "count=100000", NULL};
    char *gzip_argv[] = {"/usr/bin/gzip", "-k", "-f", "-5", LIBZPOLINE_PATH, NULL};
    char *sha_argv[] = {"/usr/bin/sha256sum", JUNK_FILE, NULL};
    char *dummy_argv[] = {"./dummy", NULL};
    char *mem_argv[] = {"./mem_stresser", NULL};
    char *strace_ls_argv[] = {"/usr/bin/strace", "/usr/bin/ls", "-l", ".", NULL};
    char *strace_dd_argv[] = {"/usr/bin/strace", "/usr/bin/dd", "if=/dev/zero", "of=/dev/null", "bs=4k", "count=100000", NULL};
    char *strace_gzip_argv[] = {"/usr/bin/strace", "/usr/bin/gzip", "-k", "-f", "-5", LIBZPOLINE_PATH, NULL};
    char *strace_sha_argv[] = {"/usr/bin/strace", "/usr/bin/sha256sum", JUNK_FILE, NULL};
    char *strace_mem_argv[] = {"/usr/bin/strace", "./mem_stresser", NULL};

    if (strcmp(TESTED_EXE, "/usr/bin/sha256sum") == 0) {
        create_junk_file();
    }

    printf("=== RISC-V zpoline benchmark ===\n\n");

    /* ./dummy */
    BenchResult res_dummy_native = run_benchmark("untouched dummy", "./dummy", dummy_argv, 0, 0);

    /* LD_PRELOAD=.../libzpoline.so ./dummy */
    BenchResult res_dummy_zpoline = run_benchmark("zpoline-intercepted dummy", "./dummy", dummy_argv, 1, 0);

    /* LD_PRELOAD=.../libsud_custom.so ./dummy */
    BenchResult res_dummy_sud = run_benchmark("sud-intercepted dummy", "./dummy", dummy_argv, 0, 1);

    /* TESTED_EXE */
    BenchResult res_exe_native = run_benchmark("untouched exe", TESTED_EXE, mem_argv, 0, 0);

    /* LD_PRELOAD=.../libzpoline.so TESTED_EXE */
    BenchResult res_exe_zpoline = run_benchmark("zpoline-intercepted exe", TESTED_EXE, mem_argv, 1, 0);

    /* strace TESTED_EXE */
    BenchResult res_exe_strace = run_benchmark("strace-intercepted exe", "/usr/bin/strace", strace_mem_argv, 0, 0);

    /* LD_PRELOAD=.../libsud_custom.so TESTED_EXE */
    BenchResult res_exe_sud = run_benchmark("sud-intercepted exe", TESTED_EXE, mem_argv, 0, 1);

    double zpoline_init_overhead = res_dummy_zpoline.mean_ms - res_dummy_native.mean_ms;
    double sud_init_overhead = res_dummy_sud.mean_ms - res_dummy_native.mean_ms;
    double zpoline_exe_runtime = res_exe_zpoline.mean_ms - zpoline_init_overhead;
    double sud_exe_runtime = res_exe_sud.mean_ms - sud_init_overhead;
    double percentage_overhead_zpoline = ((zpoline_exe_runtime - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;
    double percentage_overhead_strace = ((res_exe_strace.mean_ms - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;
    double percentage_overhead_sud = ((sud_exe_runtime - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;

    printf("\n=== TIME MEASUREMENTS ===\n");
    printf("\nTesting %s\n\n", TESTED_EXE);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "untouched exe", res_exe_native.mean_ms, res_exe_native.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "zpoline-intercepted exe", res_exe_zpoline.mean_ms, res_exe_zpoline.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "strace-intercepted exe", res_exe_strace.mean_ms, res_exe_strace.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "sud-intercepted exe", res_exe_sud.mean_ms, res_exe_sud.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "dummy", res_dummy_native.mean_ms, res_dummy_native.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "zpoline-intercepted dummy", res_dummy_zpoline.mean_ms, res_dummy_zpoline.stddev_ms);
    printf("%-30s : %.2f ms (std: %.2f ms)\n", "sud-intercepted dummy", res_dummy_sud.mean_ms, res_dummy_sud.stddev_ms);

    printf("\n=== OVERHEAD ===\n");
    printf("zpoline init overhead           : %.2f ms\n", zpoline_init_overhead);
    printf("zpoline-intercepted exe         : %.2f ms (without init)\n", zpoline_exe_runtime);
    printf("zpoline percentage overhead     : %.2f %%\n", percentage_overhead_zpoline);
    printf("strace-intercepted exe          : %.2f ms\n", res_exe_strace.mean_ms);
    printf("strace percentage overhead      : %.2f %%\n", percentage_overhead_strace);
    printf("sud init overhead               : %.2f ms\n", sud_init_overhead);
    printf("sud-intercepted exe             : %.2f ms (without init)\n", sud_exe_runtime);
    printf("sud percentage overhead         : %.2f %%\n", percentage_overhead_sud);

    return 0;
}