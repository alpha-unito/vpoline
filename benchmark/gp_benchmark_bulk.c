#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define ITERATIONS 5000

FILE *csv_file = NULL;

struct timespec t_start, t_after_slow, t_end;
double slow_path, fast_mean;

#define COMPUTE_STATS \
    clock_gettime(CLOCK_MONOTONIC, &t_end); \
    slow_path = (double)(t_after_slow.tv_sec - t_start.tv_sec) * 1e9 + \
                   (double)(t_after_slow.tv_nsec - t_start.tv_nsec); \
    fast_mean = ((double)(t_end.tv_sec - t_after_slow.tv_sec) * 1e9 + \
                        (double)(t_end.tv_nsec - t_after_slow.tv_nsec)) / (ITERATIONS - 1);

#define PRINT_STATS(insn) \
    printf("[ OK ] %-20s | Slow: %9.2f ns | Fast Mean: %8.2f ns\n", \
    insn, slow_path, fast_mean); \
    if (csv_file) fprintf(csv_file, "%s,%.2f,%.2f\n", insn, slow_path, fast_mean);

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

    /* === LB === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lb %0, 0(t6)"  : "=r"(val) : "r"(off_i8) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_i8) {
        printf("[FAIL] LB. Read: %ld\n", val);
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LB")

    /* === LH === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lh %0, 0(t6)"  : "=r"(val) : "r"(off_i16) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_i16) {
        printf("[FAIL] LH. Read: %ld\n", val);
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LH")

    /* === LW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lw %0, 0(t6)"  : "=r"(val) : "r"(off_i32) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_i32) {
        printf("[FAIL] LW\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LW")

    /* === LD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t ld %0, 0(t6)"  : "=r"(val) : "r"(off_i64) : "t6");
        if (b_i == 0) {
                clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != g_i64) {
        printf("[FAIL] LD\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LD")

    /* === LBU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lbu %0, 0(t6)" : "=r"(val) : "r"(off_u8) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_u8) {
        printf("[FAIL] LBU. Read: %ld\n", val);
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LBU")

    /* === LHU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lhu %0, 0(t6)" : "=r"(val) : "r"(off_u16) : "t6");
        if (b_i == 0) {
                clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_u16) {
        printf("[FAIL] LHU\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LHU")

    /* === LWU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t lwu %0, 0(t6)" : "=r"(val) : "r"(off_u32) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (val != (int64_t)g_u32) {
        printf("[FAIL] LWU\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("LWU")

    /* === SB === */
    val = 11;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t sb %1, 0(t6)" : : "r"(off_i8), "r"(val) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_i8 != 11) {
        printf("[FAIL] SB\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("SB")

    /* === SH === */
    val = 22;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t sh %1, 0(t6)" : : "r"(off_i16), "r"(val) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_i16 != 22) {
        printf("[FAIL] SH\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("SH")

    /* === SW === */
    val = 33;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t sw %1, 0(t6)" : : "r"(off_i32), "r"(val) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_i32 != 33) {
        printf("[FAIL] SW\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("SW")

    /* === SD === */
    val = 44;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t sd %1, 0(t6)" : : "r"(off_i64), "r"(val) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_i64 != 44) {
        printf("[FAIL] SD\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("SD")
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

    /* === FLW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t flw %0, 0(t6)" : "=f"(fval) : "r"(off_f32) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (fval != 3.14159f) {
        printf("[FAIL] FLW\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("FLW")

    /* === FLD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %1\n\t fld %0, 0(t6)" : "=f"(dval) : "r"(off_f64) : "t6");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (dval != 2.7182818) {
        printf("[FAIL] FLD\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("FLD")


    /* === FSW === */
    fval = 1.23f;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t fsw %1, 0(t6)" : : "r"(off_f32), "f"(fval) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_f32 != 1.23f) {
        printf("[FAIL] FSW\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("FSW")

    /* === FSD === */
    dval = 4.56;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int b_i = 0; b_i < ITERATIONS; b_i++) {
        asm volatile ("add t6, gp, %0\n\t fsd %1, 0(t6)" : : "r"(off_f64), "f"(dval) : "t6", "memory");
        if (b_i == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
        }
    }
    COMPUTE_STATS
    if (g_f64 != 4.56) {
        printf("[FAIL] FSD\n");
        exit(EXIT_FAILURE);
    }
    PRINT_STATS("FSD")
}

int main(int argc, char *argv[]) {
    printf("=== TESTING GP-RELATIVE MEMORY ACCESSES ===\n");

    /* warm-up call */
    struct timespec dummy_time;
    clock_gettime(CLOCK_MONOTONIC, &dummy_time);

    if (argc < 2) {
        fprintf(stderr, "[!] No output file specified\n");
        fprintf(stderr, "    Retry with: %s <nome_file_output.csv>\n", argv[0]);
        fprintf(stderr, "    Benchmark results will be printed only on screen\n\n");
        csv_file = NULL;
    } else {
        csv_file = fopen(argv[1], "w");
        if (csv_file) {
            fprintf(csv_file, "Instruction,SlowPath_ns,FastMean_ns\n");
        } else {
            perror("[!] Failed to create .csv file. Results will be printed only on screen\n");
        }
    }

    test_32bit_integer_instructions();
    test_32bit_float_instructions();

    if (csv_file) {
        fclose(csv_file);
        printf("\n[INFO] Time measurements saved in '%s'\n", argv[1]);
    }

    return 0;
}