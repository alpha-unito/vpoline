#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 5000

FILE *csv_file = NULL;

volatile int8_t   g_i8   = -42;          // LB/SB
volatile int16_t  g_i16  = -1234;        // LH/SH
volatile int32_t  g_i32  = -56789;       // LW/SW
volatile int64_t  g_i64  = -987654321LL; // LD/SD

volatile uint8_t  g_u8   = 0xAA;         // LBU
volatile uint16_t g_u16  = 0xBBCC;       // LHU
volatile uint32_t g_u32  = 0xDEADBEEF;   // LWU

volatile float    g_f32  = 3.14159f;     // FLW/FSW
volatile double   g_f64  = 2.7182818;    // FLD/FSD

void compute_and_print_stats(const char* insn, struct timespec t_start, struct timespec t_after_slow) {
    struct timespec t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double slow_path = (double)(t_after_slow.tv_sec - t_start.tv_sec) * 1e9 +
                       (double)(t_after_slow.tv_nsec - t_start.tv_nsec);

    double fast_mean = ((double)(t_end.tv_sec - t_after_slow.tv_sec) * 1e9 +
                        (double)(t_end.tv_nsec - t_after_slow.tv_nsec)) / (ITERATIONS - 1);

    if (csv_file) {
        fprintf(csv_file, "%s,%f,%f\n", insn, slow_path, fast_mean);
    }
    printf("%s,%f,%f\n", insn, slow_path, fast_mean);
}

void test_32bit_integer_instructions() {
    struct timespec t_start, t_after_slow;
    int64_t val64;
    int32_t val32;
    int16_t val16;
    int8_t val8;
    uint32_t val_u32;
    uint16_t val_u16;
    uint8_t val_u8;

    /* === LB === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val8 = g_i8;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LB", t_start, t_after_slow);
    if (val8 != -42) exit(EXIT_FAILURE);

    /* === LH === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val16 = g_i16;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LH", t_start, t_after_slow);
    if (val16 != -1234) exit(EXIT_FAILURE);

    /* === LW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val32 = g_i32;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LW", t_start, t_after_slow);
    if (val32 != -56789) exit(EXIT_FAILURE);

    /* === LD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val64 = g_i64;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LD", t_start, t_after_slow);
    if (val64 != -987654321LL) exit(EXIT_FAILURE);

    /* === LBU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val_u8 = g_u8;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LBU", t_start, t_after_slow);
    if (val_u8 != 0xAA) exit(EXIT_FAILURE);

    /* === LHU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val_u16 = g_u16;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LHU", t_start, t_after_slow);
    if (val_u16 != 0xBBCC) exit(EXIT_FAILURE);

    /* === LWU === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val_u32 = g_u32;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("LWU", t_start, t_after_slow);
    if (val_u32 != 0xDEADBEEF) exit(EXIT_FAILURE);

    /* === SB === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_i8 = 11;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("SB", t_start, t_after_slow);
    if (g_i8 != 11) exit(EXIT_FAILURE);

    /* === SH === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_i16 = 22;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("SH", t_start, t_after_slow);
    if (g_i16 != 22) exit(EXIT_FAILURE);

    /* === SW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_i32 = 33;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("SW", t_start, t_after_slow);
    if (g_i32 != 33) exit(EXIT_FAILURE);

    /* === SD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_i64 = 44;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("SD", t_start, t_after_slow);
    if (g_i64 != 44) exit(EXIT_FAILURE);
}

void test_32bit_float_instructions() {
    struct timespec t_start, t_after_slow;
    double val_f64;
    float val_f32;

    /* === FLW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val_f32 = g_f32;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("FLW", t_start, t_after_slow);
    if (val_f32 != 3.14159f) exit(EXIT_FAILURE);

    /* === FLD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        val_f64 = g_f64;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("FLD", t_start, t_after_slow);
    if (val_f64 != 2.7182818) exit(EXIT_FAILURE);

    /* === FSW === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_f32 = 1.23f;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("FSW", t_start, t_after_slow);
    if (g_f32 != 1.23f) exit(EXIT_FAILURE);

    /* === FSD === */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        g_f64 = 4.56;
        if (i == 0) clock_gettime(CLOCK_MONOTONIC, &t_after_slow);
    }
    compute_and_print_stats("FSD", t_start, t_after_slow);
    if (g_f64 != 4.56) exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        csv_file = fopen(argv[1], "w");
    }

    struct timespec dummy;
    clock_gettime(CLOCK_MONOTONIC, &dummy);

    test_32bit_integer_instructions();
    test_32bit_float_instructions();

    if (csv_file) {
        fclose(csv_file);
    }

    return 0;
}
