#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * The current test program must be compiled explicitly with as a non PIE.
 * Not doing so would make the test useless since the linker would not
 * translate the assignment statements before each check into GP-relative
 * memory access instructions.
 *
 * This gets automatically correctly compiled by using cmake, however the
 * command is reported here for reference:
 *
 * gcc -o gp_test gp_test.c -no-pie -no-pie -msmall-data-limit=8
 */

/*
 * the following global variables locations will be chosen by the linker,
 * but we'll compute their offset from __global_pointer$
 */
volatile int8_t   global_var_8  = -10;
volatile int16_t  global_var_16 = -200;
volatile int32_t  global_var_32 = 42;
volatile int64_t  global_var_64 = 100;

volatile uint8_t  global_var_u8  = 0xAA;
volatile uint16_t global_var_u16 = 0x8AAA;
volatile uint32_t global_var_u32 = 0x8AAAAAAA;

volatile float    global_var_f32 = 3.14f;
volatile double   global_var_f64 = 2.718;

int exit_status = EXIT_SUCCESS;

void test_32bit_gp_access() {

    /* --- TEST LD/SD --- */
    printf("[*] Testing LD/SD with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int64_t expected_load = (i == 0) ? 100 : 200;
        int64_t store_val     = (i == 0) ? 200 : 300;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        /* linker will translate this assignment into ld reg, offset(gp) */
        int64_t val64 = global_var_64;

        if (val64 != expected_load) {
            printf("[FAIL] Failed LD emulation in %s. Read: %ld, Expected: %ld\n", path_name, val64, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LD emulation in %s\n", path_name);
        }

        /* linker will translate this assignment sd reg, offset(gp) */
        global_var_64 = store_val;

        if (global_var_64 != store_val) {
            printf("[FAIL] Failed SD in %s. In memory: %ld, Expected: %ld\n", path_name, global_var_64, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SD emulation in %s\n", path_name);
        }
    }

    /* --- TEST LW/SW --- */
    printf("\n[*] Testing LW/SW with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int32_t expected_load = (i == 0) ? 42 : 84;
        int32_t store_val     = (i == 0) ? 84 : 126;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        /* linker will translate this assignment into lw reg, offset(gp) */
        int32_t val32 = global_var_32;

        if (val32 != expected_load) {
            printf("[FAIL] Failed LW emulation in %s. Read: %d, Expected: %d\n", path_name, val32, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LW emulation in %s\n", path_name);
        }

        global_var_32 = store_val;

        if (global_var_32 != store_val) {
            printf("[FAIL] Failed SW in %s. In memory: %d, Expected: %d\n", path_name, global_var_32, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SW emulation in %s\n", path_name);
        }
    }

    /* --- TEST LH/SH --- */
    printf("\n[*] Testing LH/SH (16-bit signed) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int16_t expected_load = (i == 0) ? -200 : -400;
        int16_t store_val     = (i == 0) ? -400 : -800;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        int16_t val16 = global_var_16;

        if (val16 != expected_load) {
            printf("[FAIL] Failed LH emulation in %s. Read: %d, Expected: %d\n", path_name, val16, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LH emulation in %s\n", path_name);
        }

        global_var_16 = store_val;

        if (global_var_16 != store_val) {
            printf("[FAIL] Failed SH in %s. In memory: %d, Expected: %d\n", path_name, global_var_16, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SH emulation in %s\n", path_name);
        }
    }

    /* --- TEST LB/SB --- */
    printf("\n[*] Testing LB/SB (8-bit signed) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int8_t expected_load = (i == 0) ? -10 : -50;
        int8_t store_val     = (i == 0) ? -50 : -100;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        int8_t val8 = global_var_8;

        if (val8 != expected_load) {
            printf("[FAIL] Failed LB emulation in %s. Read: %d, Expected: %d\n", path_name, val8, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LB emulation in %s\n", path_name);
        }

        global_var_8 = store_val;

        if (global_var_8 != store_val) {
            printf("[FAIL] Failed SB in %s. In memory: %d, Expected: %d\n", path_name, global_var_8, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SB emulation in %s\n", path_name);
        }
    }

    /* --- TEST LWU/SW --- */
    printf("\n[*] Testing LWU/SW (32-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint32_t expected_load = (i == 0) ? 0x8AAAAAAA : 0x9BBBBBBB;
        uint32_t store_val     = (i == 0) ? 0x9BBBBBBB : 0xACCCCCCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        uint32_t val_u32 = global_var_u32;

        if (val_u32 != expected_load) {
            printf("[FAIL] Failed LWU emulation in %s. Read: 0x%X, Expected: 0x%X\n", path_name, val_u32, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LWU emulation in %s\n", path_name);
        }

        global_var_u32 = store_val;

        if (global_var_u32 != store_val) {
            printf("[FAIL] Failed SW in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u32, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SW emulation in %s\n", path_name);
        }
    }

    /* --- TEST LHU/SH --- */
    printf("\n[*] Testing LHU/SH (16-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint16_t expected_load = (i == 0) ? 0x8AAA : 0x9BBB;
        uint16_t store_val     = (i == 0) ? 0x9BBB : 0xACCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        uint16_t val_u16 = global_var_u16;

        if (val_u16 != expected_load) {
            printf("[FAIL] Failed LHU emulation in %s. Read: 0x%X, Expected: 0x%X\n", path_name, val_u16, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LHU emulation in %s\n", path_name);
        }

        global_var_u16 = store_val;

        if (global_var_u16 != store_val) {
            printf("[FAIL] Failed SH in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u16, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SH emulation in %s\n", path_name);
        }
    }

    /* --- TEST LBU/SB --- */
    printf("\n[*] Testing LBU/SB (8-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint8_t expected_load = (i == 0) ? 0xAA : 0xBB;
        uint8_t store_val     = (i == 0) ? 0xBB : 0xCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        uint8_t val_u8 = global_var_u8;

        if (val_u8 != expected_load) {
            printf("[FAIL] Failed LBU emulation in %s. Read: 0x%X, Expected: 0x%X\n", path_name, val_u8, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct LBU emulation in %s\n", path_name);
        }

        global_var_u8 = store_val;

        if (global_var_u8 != store_val) {
            printf("[FAIL] Failed SB in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u8, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct SB emulation in %s\n", path_name);
        }
    }

    /* --- TEST FLD/FSD --- */
    printf("\n[*] Testing FLD/FSD (64-bit floating) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        double expected_load = (i == 0) ? 2.718 : 5.436;
        double store_val     = (i == 0) ? 5.436 : 8.154;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        double val_f64 = global_var_f64;

        if (val_f64 != expected_load) {
            printf("[FAIL] Failed FLD emulation in %s. Read: %lf, Expected: %lf\n", path_name, val_f64, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct FLD emulation in %s\n", path_name);
        }

        global_var_f64 = store_val;

        if (global_var_f64 != store_val) {
            printf("[FAIL] Failed FSD in %s. In memory: %lf, Expected: %lf\n", path_name, global_var_f64, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct FSD emulation in %s\n", path_name);
        }
    }

    /* --- TEST FLW/FSW --- */
    printf("\n[*] Testing FLW/FSW (32-bit floating) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        float expected_load = (i == 0) ? 3.14f : 6.28f;
        float store_val     = (i == 0) ? 6.28f : 9.42f;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        float val_f32 = global_var_f32;

        if (val_f32 != expected_load) {
            printf("[FAIL] Failed FLW emulation in %s. Read: %f, Expected: %f\n", path_name, val_f32, expected_load);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct FLW emulation in %s\n", path_name);
        }

        global_var_f32 = store_val;

        if (global_var_f32 != store_val) {
            printf("[FAIL] Failed FSW in %s. In memory: %f, Expected: %f\n", path_name, global_var_f32, store_val);
            exit_status = EXIT_FAILURE;
        } else {
            printf("[ OK ] Correct FSW emulation in %s\n", path_name);
        }
    }

}

int main() {
    printf("\n=== Inizio Test Accessi Global Pointer (GP) ===\n");
    test_32bit_gp_access();
    printf("=== Test Completati ===\n\n");
    return exit_status;
}