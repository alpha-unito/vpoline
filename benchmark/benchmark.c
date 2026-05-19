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

#define ITERATIONS N_ITERATIONS
#define WARMUP 10
#define JUNK_FILE "./sha256sum_benchmark_junk.bin"
#define LIBZPOLINE "/home/omonticelli/vpoline/build/libvpoline.so"
#define SUD "/home/omonticelli/vpoline/benchmark/libsud_custom.so"
#define SYSCALL_INTERCEPT "/home/omonticelli/gekko_syscall_intercept/build/libsyscall_intercept.so"
#define TESTED_EXE "/usr/bin/sha256sum"

typedef struct {
    double mean_ms;
    double stddev_ms;
} BenchResult;

/* executes command and measures run time */
double run_and_measure(const char *path, char *const argv[], int use_zpoline, int use_sud, int use_intercept) {
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
        } else if (use_intercept) {
            setenv("LD_PRELOAD", SYSCALL_INTERCEPT, 1);
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
BenchResult run_benchmark(const char *name, const char *path, char *const argv[], int use_zpoline, int use_sud, int use_intercept) {

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
        run_and_measure(path, argv, use_zpoline, use_sud, use_intercept);
    }

    printf("Measuring %s (%d iterations)...\n", name, iterations);
    for (int i = 0; i < iterations; i++) {
#ifdef DEBUG
        printf("Running iteration %d...\n", i);
#endif
        times[i] = run_and_measure(path, argv, use_zpoline, use_sud, use_intercept);
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
    char *gzip_argv[] = {"/usr/bin/gzip", "-k", "-f", "-5", LIBZPOLINE, NULL};
    char *sha_argv[] = {"/usr/bin/sha256sum", JUNK_FILE, NULL};
    char *dummy_argv[] = {"./dummy", NULL};
    char *mem_argv[] = {"./mem_stresser", NULL};
    char *strace_ls_argv[] = {"/usr/bin/strace", "/usr/bin/ls", "-l", ".", NULL};
    char *strace_dd_argv[] = {"/usr/bin/strace", "/usr/bin/dd", "if=/dev/zero", "of=/dev/null", "bs=4k", "count=100000", NULL};
    char *strace_gzip_argv[] = {"/usr/bin/strace", "/usr/bin/gzip", "-k", "-f", "-5", LIBZPOLINE, NULL};
    char *strace_sha_argv[] = {"/usr/bin/strace", "/usr/bin/sha256sum", JUNK_FILE, NULL};
    char *strace_mem_argv[] = {"/usr/bin/strace", "./mem_stresser", NULL};

    FILE* json_file = fopen("benchmark_results.json", "w");
    if (!json_file) {
        perror("Errore: impossibile creare il file JSON per i risultati");
        // Non usciamo con exit(1) così quantomeno stampa a schermo
    }

    if (strcmp(TESTED_EXE, "/usr/bin/sha256sum") == 0) {
        create_junk_file();
    }

    printf("=== RISC-V zpoline benchmark ===\n\n");

    /* ./dummy */
    BenchResult res_dummy_native = run_benchmark("untouched dummy", "./dummy", dummy_argv, 0, 0, 0);

    /* LD_PRELOAD=.../libzpoline.so ./dummy */
    BenchResult res_dummy_zpoline = run_benchmark("zpoline-intercepted dummy", "./dummy", dummy_argv, 1, 0, 0);

    /* LD_PRELOAD=.../libsyscall_intercept.so ./dummy */
    BenchResult res_dummy_intercept = run_benchmark("syscall_intercept-intercepted dummy", "./dummy", dummy_argv, 0, 0, 1);

    /* LD_PRELOAD=.../libsud_custom.so ./dummy */
    BenchResult res_dummy_sud = run_benchmark("sud-intercepted dummy", "./dummy", dummy_argv, 0, 1, 0);

    /* TESTED_EXE */
    BenchResult res_exe_native = run_benchmark("untouched exe", TESTED_EXE, sha_argv, 0, 0, 0);

    /* LD_PRELOAD=.../libzpoline.so TESTED_EXE */
    BenchResult res_exe_zpoline = run_benchmark("zpoline-intercepted exe", TESTED_EXE, sha_argv, 1, 0, 0);

    /* LD_PRELOAD=.../libsyscall_intercept.so TESTED_EXE */
    BenchResult res_exe_intercept = run_benchmark("syscall_intercept-intercepted exe", TESTED_EXE, sha_argv, 0, 0, 1);

    /* strace TESTED_EXE */
    BenchResult res_exe_strace = run_benchmark("strace-intercepted exe", "/usr/bin/strace", strace_sha_argv, 0, 0, 0);

    /* LD_PRELOAD=.../libsud_custom.so TESTED_EXE */
    BenchResult res_exe_sud = run_benchmark("sud-intercepted exe", TESTED_EXE, sha_argv, 0, 1, 0);

    double zpoline_init_overhead = res_dummy_zpoline.mean_ms - res_dummy_native.mean_ms;
    double zpoline_init_stddev = sqrt(pow(res_dummy_zpoline.stddev_ms, 2) + pow(res_dummy_native.stddev_ms, 2));
    double intercept_init_overhead = res_dummy_intercept.mean_ms - res_dummy_native.mean_ms;
    double intercept_init_stddev = sqrt(pow(res_dummy_intercept.stddev_ms, 2) + pow(res_dummy_native.stddev_ms, 2));
    double sud_init_overhead = res_dummy_sud.mean_ms - res_dummy_native.mean_ms;
    double sud_init_stddev = sqrt(pow(res_dummy_sud.stddev_ms, 2) + pow(res_dummy_native.stddev_ms, 2));
    double zpoline_exe_runtime = res_exe_zpoline.mean_ms - zpoline_init_overhead;
    double zpoline_exe_runtime_stddev = sqrt(pow(res_exe_zpoline.stddev_ms, 2) + pow(zpoline_init_stddev, 2));
    double intercept_exe_runtime = res_exe_intercept.mean_ms - intercept_init_overhead;
    double intercept_exe_runtime_stddev = sqrt(pow(res_exe_intercept.stddev_ms, 2) + pow(intercept_init_stddev, 2));
    double sud_exe_runtime = res_exe_sud.mean_ms - sud_init_overhead;
    double sud_exe_runtime_stddev = sqrt(pow(res_exe_sud.stddev_ms, 2) + pow(sud_init_stddev, 2));
    double percentage_overhead_zpoline = ((zpoline_exe_runtime - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;
    double percentage_overhead_intercept = ((res_exe_intercept.mean_ms - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;
    double percentage_overhead_strace = ((res_exe_strace.mean_ms - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;
    double percentage_overhead_sud = ((sud_exe_runtime - res_exe_native.mean_ms) / res_exe_native.mean_ms) * 100.0;


    printf("\n=== TIME MEASUREMENTS ===\n");
    printf("\nTesting %s\n\n", TESTED_EXE);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "untouched exe", res_exe_native.mean_ms, res_exe_native.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "zpoline-intercepted exe", res_exe_zpoline.mean_ms, res_exe_zpoline.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "syscall_intercept-intercepted exe", res_exe_intercept.mean_ms, res_exe_intercept.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "strace-intercepted exe", res_exe_strace.mean_ms, res_exe_strace.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "sud-intercepted exe", res_exe_sud.mean_ms, res_exe_sud.stddev_ms);

    printf("\n%-40s : %8.2f ms (std: %8.2f ms)\n", "dummy", res_dummy_native.mean_ms, res_dummy_native.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "zpoline-intercepted dummy", res_dummy_zpoline.mean_ms, res_dummy_zpoline.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "syscall_intercept-intercepted dummy", res_dummy_intercept.mean_ms, res_dummy_intercept.stddev_ms);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "sud-intercepted dummy", res_dummy_sud.mean_ms, res_dummy_sud.stddev_ms);

    printf("\n=== OVERHEAD ===\n");

    /* ZPOLINE */
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "zpoline init overhead", zpoline_init_overhead, zpoline_init_stddev);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "zpoline exe (without init)", zpoline_exe_runtime, zpoline_exe_runtime_stddev);
    printf("%-40s : %8.2f %%\n\n", "zpoline percentage overhead", percentage_overhead_zpoline);

    /* SYSCALL_INTERCEPT */
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "syscall_intercept init overhead", intercept_init_overhead, intercept_init_stddev);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "syscall_intercept exe (without init)", intercept_exe_runtime, intercept_exe_runtime_stddev);
    printf("%-40s : %8.2f %%\n\n", "syscall_intercept perc. overhead", percentage_overhead_intercept);

    /* STRACE */
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "strace exe", res_exe_strace.mean_ms, res_exe_strace.stddev_ms);
    printf("%-40s : %8.2f %%\n\n", "strace percentage overhead", percentage_overhead_strace);

    /* SUD */
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "sud init overhead", sud_init_overhead, sud_init_stddev);
    printf("%-40s : %8.2f ms (std: %8.2f ms)\n", "sud exe (without init)", sud_exe_runtime, sud_exe_runtime_stddev);
    printf("%-40s : %8.2f %%\n", "sud percentage overhead", percentage_overhead_sud);

    if (json_file) {
        fprintf(json_file, "{\n");

        /* Native */
        fprintf(json_file, "    \"native exe\": {\n");
        fprintf(json_file, "        \"mean\": %.2f,\n", res_exe_native.mean_ms);
        fprintf(json_file, "        \"stddev\": %.2f\n", res_exe_native.stddev_ms);
        fprintf(json_file, "    },\n");

        /* Zpoline (Without Init) */
        fprintf(json_file, "    \"zpoline exe (without init)\": {\n");
        fprintf(json_file, "        \"mean\": %.2f,\n", zpoline_exe_runtime);
        fprintf(json_file, "        \"stddev\": %.2f\n", zpoline_exe_runtime_stddev);
        fprintf(json_file, "    },\n");

        /* Syscall Intercept (Without Init) */
        fprintf(json_file, "    \"syscall_intercept exe (without init)\": {\n");
        fprintf(json_file, "        \"mean\": %.2f,\n", intercept_exe_runtime);
        fprintf(json_file, "        \"stddev\": %.2f\n", intercept_exe_runtime_stddev);
        fprintf(json_file, "    },\n");

        /* Strace */
        fprintf(json_file, "    \"strace exe\": {\n");
        fprintf(json_file, "        \"mean\": %.2f,\n", res_exe_strace.mean_ms);
        fprintf(json_file, "        \"stddev\": %.2f\n", res_exe_strace.stddev_ms);
        fprintf(json_file, "    },\n");

        /* SUD (Without Init) */
        fprintf(json_file, "    \"sud exe (without init)\": {\n");
        fprintf(json_file, "        \"mean\": %.2f,\n", sud_exe_runtime);
        fprintf(json_file, "        \"stddev\": %.2f\n", sud_exe_runtime_stddev);
        fprintf(json_file, "    }\n");

        fprintf(json_file, "}\n");
        fclose(json_file);

        printf("\n[INFO] Risultati JSON salvati con successo in 'benchmark_results.json'\n");
    }

    return 0;
}