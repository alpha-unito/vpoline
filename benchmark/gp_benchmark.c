#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define TEST_NR 27
#define ITERATIONS 100

FILE *csv_file = NULL;

struct result {
    double mean;
    double std_dev;
};

struct result overall_stats[TEST_NR];

int test_idx = 0;

#define START_BENCHMARK \
    { \
        double bench_times[ITERATIONS]; \
        for (int b_i = 0; b_i < ITERATIONS; b_i++) {

#define END_BENCHMARK(csv_tag) \
        } \
        double sum = 0.0, mean = 0.0, variance = 0.0, std_dev = 0.0; \
        for (int j = 0; j < ITERATIONS; j++) sum += bench_times[j]; \
        mean = sum / ITERATIONS; \
        for (int j = 0; j < ITERATIONS; j++) variance += pow(bench_times[j] - mean, 2); \
        variance /= (ITERATIONS-1); \
        std_dev = sqrt(variance); \
        if (test_idx < TEST_NR) { \
            overall_stats[test_idx].mean = mean ; \
            overall_stats[test_idx].std_dev = std_dev; \
            test_idx++; \
        } else { \
            fprintf(stderr, "[!] Enhance TEST_NR\n"); \
        }\
        if (csv_file) fprintf(csv_file, "%s,%.2f,%.2f\n", csv_tag, mean, std_dev); \
        printf("[ OK ] %-20s (Mean: %8.2f ns | StdDev: %8.2f ns)\n", csv_tag, mean, std_dev); \
    }

#define START_TIME \
    struct timespec start, end; \
    clock_gettime(CLOCK_MONOTONIC, &start);

#define STOP_TIME \
    clock_gettime(CLOCK_MONOTONIC, &end); \
    bench_times[b_i] = (double)(end.tv_sec - start.tv_sec) * 1e9 + \
    (double)(end.tv_nsec - start.tv_nsec);


/* global variables needed for tests */
int8_t   g_i8   = -42;          // LB/SB
int16_t  g_i16  = -1234;        // LH/SH
int32_t  g_i32  = -56789;       // LW/SW/C.LW/C.SW/C.LWSP/C.SWSP
int64_t  g_i64  = -987654321LL; // LD/SD/C.LD/C.SD/C.LDSP/C.SDSP

uint8_t  g_u8   = 0xAA;         // LBU
uint16_t g_u16  = 0xBBCC;       // LHU
uint32_t g_u32  = 0xDEADBEEF;   // LWU

float    g_f32  = 3.14159f;     // FLW/FSW
double   g_f64  = 2.7182818;    // FLD/FSD/C.FLD/C.FSD/C.FLDSP/C.FSDSP

/*
 * RISC-V linker symbol. libvpoline.so just overwrites GP content but
 * __global_pointer$ keeps pointing to the original GP value
 */
extern char __global_pointer$[];

/*
 * compressed SP-relative instructions (C.FLDSP, C.LWSP, C.LDSP, C.FSDSP,
 * C.SWSP, C.SDSP) are hardcoded to use SP value to compute memory addresses,
 * then we need to set up an alternative stack to test them without risk of
 * corrupting the area pointed by SP when SIGSEGV will be issued, as we'll use
 * SP as a holder of relocated_GP+offset (pointing in the XOM shadow page).
 */
void setup_alt_stack() {
    static char alt_stack[SIGSTKSZ];
    stack_t ss = { .ss_sp = alt_stack, .ss_size = SIGSTKSZ, .ss_flags = 0 };
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack fallita");
        exit(1);
    }

    struct sigaction sa;
    sigaction(SIGSEGV, NULL, &sa);
    sa.sa_flags |= SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
}

/* =================================== */
/*  32-bit integer instructions tests  */
/* =================================== */
void test_32bit_integer_instructions() {
    printf("\n=== Testing 32-bit integers instructions...\n");

    intptr_t off_i8  = (char*)&g_i8  - __global_pointer$;
    intptr_t off_i16 = (char*)&g_i16 - __global_pointer$;
    intptr_t off_i32 = (char*)&g_i32 - __global_pointer$;
    intptr_t off_i64 = (char*)&g_i64 - __global_pointer$;
    intptr_t off_u8  = (char*)&g_u8  - __global_pointer$;
    intptr_t off_u16 = (char*)&g_u16 - __global_pointer$;
    intptr_t off_u32 = (char*)&g_u32 - __global_pointer$;

    int64_t val = 0;

    /* LOADS */
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lb %0, 0(a5)"  : "=r"(val) : "r"(off_i8) : "a5");
    STOP_TIME
    if (val != (int64_t)g_i8) {
        printf("[FAIL] LB. Read: %ld\n", val);
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LB (Sign-Extended)")


    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lh %0, 0(a5)"  : "=r"(val) : "r"(off_i16) : "a5");
    STOP_TIME
    if (val != (int64_t)g_i16) {
        printf("[FAIL] LH\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LH")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lw %0, 0(a5)"  : "=r"(val) : "r"(off_i32) : "a5");
    STOP_TIME
    if (val != (int64_t)g_i32) {
        printf("[FAIL] LW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LW")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t ld %0, 0(a5)"  : "=r"(val) : "r"(off_i64) : "a5");
    STOP_TIME
    if (val != g_i64) {
        printf("[FAIL] LD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LD")

    /* UNSIGNED LOADS */
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lbu %0, 0(a5)" : "=r"(val) : "r"(off_u8) : "a5");
    STOP_TIME
    if (val != (int64_t)g_u8) {
        printf("[FAIL] LBU. Read: %ld\n", val);
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LBU (Zero-Extended)")


    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lhu %0, 0(a5)" : "=r"(val) : "r"(off_u16) : "a5");
    STOP_TIME
    if (val != (int64_t)g_u16) {
        printf("[FAIL] LHU\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LHU")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t lwu %0, 0(a5)" : "=r"(val) : "r"(off_u32) : "a5");
    STOP_TIME
    if (val != (int64_t)g_u32) {
        printf("[FAIL] LWU\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("LWU")

    /* STORES */
    val = 11;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t sb %1, 0(a5)" : : "r"(off_i8), "r"(val) : "a5", "memory");
    STOP_TIME
    if (g_i8 != 11) {
        printf("[FAIL] SB\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("SB")

    val = 22;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t sh %1, 0(a5)" : : "r"(off_i16), "r"(val) : "a5", "memory");
    STOP_TIME
    if (g_i16 != 22) {
        printf("[FAIL] SH\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("SH")

    val = 33;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t sw %1, 0(a5)" : : "r"(off_i32), "r"(val) : "a5", "memory");
    STOP_TIME
    if (g_i32 != 33) {
        printf("[FAIL] SW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("SW")

    val = 44;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t sd %1, 0(a5)" : : "r"(off_i64), "r"(val) : "a5", "memory");
    STOP_TIME
    if (g_i64 != 44) {
        printf("[FAIL] SD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("SD")
}

/* ==================================== */
/*  32-bit floating instructions tests  */
/* ==================================== */
void test_32bit_float_instructions() {
    printf("\n=== Testing 32-bit floating instructions...\n");

    intptr_t off_f32 = (char*)&g_f32 - __global_pointer$;
    intptr_t off_f64 = (char*)&g_f64 - __global_pointer$;

    float fval = 0.0f;
    double dval = 0.0;

    /* LOADS */
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t flw %0, 0(a5)" : "=f"(fval) : "r"(off_f32) : "a5");
    STOP_TIME
    if (fval != 3.14159f) {
        printf("[FAIL] FLW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("FLW")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t fld %0, 0(a5)" : "=f"(dval) : "r"(off_f64) : "a5");
    STOP_TIME
    if (dval != 2.7182818) {
        printf("[FAIL] FLD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("FLD")

    /* STORES */
    fval = 1.23f;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t fsw %1, 0(a5)" : : "r"(off_f32), "f"(fval) : "a5", "memory");
    STOP_TIME
    if (g_f32 != 1.23f) {
        printf("[FAIL] FSW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("FSW")

    dval = 4.56;
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %0\n\t fsd %1, 0(a5)" : : "r"(off_f64), "f"(dval) : "a5", "memory");
    STOP_TIME
    if (g_f64 != 4.56) {
        printf("[FAIL] FSD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("FSD")
}

/* ============================================== */
/*  non SP-relative compressed instruction tests  */
/* ============================================== */
void test_compressed_c0_instructions() {
    printf("\n=== Testing 16-bit non SP-relative instructions...\n");

    intptr_t off_i32 = (char*)&g_i32 - __global_pointer$;
    intptr_t off_i64 = (char*)&g_i64 - __global_pointer$;
    intptr_t off_f64 = (char*)&g_f64 - __global_pointer$;

    int64_t val = 0;
    double dval = 0.0;

    /* LOADS */
    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t c.lw a4, 0(a5)\n\t mv %0, a4" : "=r"(val) : "r"(off_i32) : "a4", "a5");
    STOP_TIME
    if (val != 33) {
        printf("[FAIL] C.LW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.LW")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t c.ld a4, 0(a5)\n\t mv %0, a4" : "=r"(val) : "r"(off_i64) : "a4", "a5");
    STOP_TIME
    if (val != 44) {
        printf("[FAIL] C.LD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.LD")

    START_BENCHMARK
    START_TIME
    asm volatile ("add a5, gp, %1\n\t c.fld fa5, 0(a5)\n\t fmv.d %0, fa5" : "=f"(dval) : "r"(off_f64) : "a5", "fa5");
    STOP_TIME
    if (dval != 4.56) {
        printf("[FAIL] C.FLD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.FLD")

    /* STORES */
    val = 55;
    START_BENCHMARK
    START_TIME
    asm volatile ("mv a4, %1\n\t add a5, gp, %0\n\t c.sw a4, 0(a5)" : : "r"(off_i32), "r"(val) : "a4", "a5", "memory");
    STOP_TIME
    if (g_i32 != 55) {
        printf("[FAIL] C.SW\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.SW")

    val = 66;
    START_BENCHMARK
    START_TIME
    asm volatile ("mv a4, %1\n\t add a5, gp, %0\n\t c.sd a4, 0(a5)" : : "r"(off_i64), "r"(val) : "a4", "a5", "memory");
    STOP_TIME
    if (g_i64 != 66) {
        printf("[FAIL] C.SD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.SD")

    dval = 7.89;
    START_BENCHMARK
    START_TIME
    asm volatile ("fmv.d fa5, %1\n\t add a5, gp, %0\n\t c.fsd fa5, 0(a5)" : : "r"(off_f64), "f"(dval) : "a5", "fa5", "memory");
    STOP_TIME
    if (g_f64 != 7.89) {
        printf("[FAIL] C.FSD\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.FSD")
}

/* ========================================== */
/*  SP-relative compressed instruction tests  */
/* ========================================== */
void test_compressed_c2_sp_relative() {
    printf("\n=== Testing 16-bit SP-relative instructions...\n");

    intptr_t off_i32 = (char*)&g_i32 - __global_pointer$;
    intptr_t off_i64 = (char*)&g_i64 - __global_pointer$;
    intptr_t off_f64 = (char*)&g_f64 - __global_pointer$;

    int64_t val = 0;
    double dval = 0.0;

    /* LOADS */
    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %1\n\t c.lwsp %0, 0(sp)\n\t mv sp, t0" : "=r"(val) : "r"(off_i32) : "t0");
    STOP_TIME
    if (val != 55) {
        printf("[FAIL] C.LWSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.LWSP")

    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %1\n\t c.ldsp %0, 0(sp)\n\t mv sp, t0" : "=r"(val) : "r"(off_i64) : "t0");
    STOP_TIME
    if (val != 66) {
        printf("[FAIL] C.LDSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.LDSP")

    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %1\n\t c.fldsp %0, 0(sp)\n\t mv sp, t0" : "=f"(dval) : "r"(off_f64) : "t0");
    STOP_TIME
    if (dval != 7.89) {
        printf("[FAIL] C.FLDSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.FLDSP")

    /* STORES */
    val = 77;
    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %0\n\t c.swsp %1, 0(sp)\n\t mv sp, t0" : : "r"(off_i32), "r"(val) : "t0", "memory");
    STOP_TIME
    if (g_i32 != 77) {
        printf("[FAIL] C.SWSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.SWSP")

    val = 88;
    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %0\n\t c.sdsp %1, 0(sp)\n\t mv sp, t0" : : "r"(off_i64), "r"(val) : "t0", "memory");
    STOP_TIME
    if (g_i64 != 88) {
        printf("[FAIL] C.SDSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.SDSP")

    dval = 9.99;
    START_BENCHMARK
    START_TIME
    asm volatile ("mv t0, sp\n\t add sp, gp, %0\n\t c.fsdsp %1, 0(sp)\n\t mv sp, t0" : : "r"(off_f64), "f"(dval) : "t0", "memory");
    STOP_TIME
    if (g_f64 != 9.99) {
        printf("[FAIL] C.FSDSP\n");
        exit(EXIT_FAILURE);
    }
    END_BENCHMARK("C.FSDSP")
}

void print_global_stats() {
    if (test_idx == 0) return;

    double sum_of_means = 0.0;

    for (int i = 0; i < test_idx; i++) {
        sum_of_means += overall_stats[i].mean;
    }
    double global_mean = sum_of_means / test_idx;

    double variance_of_means = 0.0;
    for (int i = 0; i < test_idx; i++) {
        variance_of_means += pow(overall_stats[i].mean - global_mean, 2);
    }
    variance_of_means /= (test_idx-1);
    double global_std_dev = sqrt(variance_of_means);

    printf("\n[INFO] Global Mean across instructions : %8.2f ns\n", global_mean);
    printf("[INFO] Global StdDev across instructions : %8.2f ns\n", global_std_dev);

    /* 4. Stampa su CSV (se il file è aperto) */
    if (csv_file) {
        fprintf(csv_file, "GLOBAL_AVERAGE,%.2f,%.2f\n", global_mean, global_std_dev);
    }
}

int main(int argc, char *argv[]) {
    printf("=== TESTING GP-RELATIVE MEMORY ACCESSES ===\n");

    if (argc < 2) {
        fprintf(stderr, "[!] No output file specified\n");
        fprintf(stderr, "    Retry with: %s <nome_file_output.csv>\n", argv[0]);
        fprintf(stderr, "    Benchmark results will be printed only on screen\n\n");
        csv_file = NULL;
    } else {
        csv_file = fopen(argv[1], "w");
        if (csv_file) {
            fprintf(csv_file, "Instruction,Mean_ns,StdDev_ns\n");
        } else {
            perror("[!] Failed to create .csv file. Results will be printed only on screen\n");
        }
    }

    setup_alt_stack();

    test_32bit_integer_instructions();
    test_32bit_float_instructions();
    test_compressed_c0_instructions();
    test_compressed_c2_sp_relative();

    print_global_stats();

    if (csv_file) {
        fclose(csv_file);
        printf("\n[INFO] Time measurements saved in '%s'\n",argv[1]);
    }

    return 0;
}