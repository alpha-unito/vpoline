#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * the following global variables locations will be chosen by the linker,
 * but we'll compute their offset from __global_pointer$
 */
int8_t   global_var_8  = -10;
int16_t  global_var_16 = -200;
int32_t  global_var_32 = 42;
int64_t  global_var_64 = 100;

uint8_t  global_var_u8  = 0xAA;
uint16_t global_var_u16 = 0x8AAA;
uint32_t global_var_u32 = 0x8AAAAAAA;

float    global_var_f32 = 3.14f;
double   global_var_f64 = 2.718;

extern char __global_pointer$[];

int exit_status = EXIT_SUCCESS;

void test_32bit_gp_access() {

    /* computing offsets from __global_pointer$ */
    intptr_t offset_8   = (char*)&global_var_8   - __global_pointer$;
    intptr_t offset_16  = (char*)&global_var_16  - __global_pointer$;
    intptr_t offset_32  = (char*)&global_var_32  - __global_pointer$;
    intptr_t offset_64  = (char*)&global_var_64  - __global_pointer$;

    intptr_t offset_u8  = (char*)&global_var_u8  - __global_pointer$;
    intptr_t offset_u16 = (char*)&global_var_u16 - __global_pointer$;
    intptr_t offset_u32 = (char*)&global_var_u32 - __global_pointer$;

    intptr_t offset_f32 = (char*)&global_var_f32 - __global_pointer$;
    intptr_t offset_f64 = (char*)&global_var_f64 - __global_pointer$;

    /* --- TEST LD/SD --- */
    printf("[*] Testing LD/SD with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int64_t val64 = 0;
        int64_t expected_load = (i == 0) ? 100 : 200;
        int64_t store_val     = (i == 0) ? 200 : 300;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "ld %0, 0(t6)\n\t"
            : "=r" (val64)
            : "r" (offset_64)
            : "t6");
        if (val64 != expected_load) {
            printf("[FAIL] Failed LD emulation in %s. Read: %ld, Expected: %ld\n", path_name, val64, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LD emulation in %s\n", path_name);

        __asm__ volatile (
            "add t6, gp, %0\n\t"
            "sd %1, 0(t6)\n\t"
            :
            : "r" (offset_64), "r" (store_val)
            : "t6", "memory");
        if (global_var_64 != store_val) {
            printf("[FAIL] Failed SD in %s. In memory: %ld, Expected: %ld\n", path_name, global_var_64, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SD emulation in %s\n", path_name);
    }

    /* --- TEST LD/SW --- */
    printf("\n[*] Testing LW/SW with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int32_t val32 = 0;
        int32_t expected_load = (i == 0) ? 42 : 84;
        int32_t store_val     = (i == 0) ? 84 : 126;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        /* TEST LOAD 32-bit (lw) */
        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "lw %0, 0(t6)\n\t"
            : "=r" (val32)
            : "r" (offset_32)
            : "t6"
        );
        if (val32 != expected_load) {
            printf("[FAIL] Failed LW emulation in %s. Read: %d, Expected: %d\n", path_name, val32, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LW emulation in %s\n", path_name);

        /* TEST STORE 32-bit (sw) */
        __asm__ volatile (
            "add t6, gp, %0\n\t"
            "sw %1, 0(t6)\n\t"
            :
            : "r" (offset_32), "r" (store_val)
            : "t6", "memory"
        );
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
        int64_t val16 = 0;
        int64_t expected_load = (i == 0) ? -200 : -400;
        int16_t store_val     = (i == 0) ? -400 : -800;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "lh %0, 0(t6)\n\t"
            : "=r" (val16)
            : "r" (offset_16)
            : "t6");
        if (val16 != expected_load) {
            printf("[FAIL] Failed LH emulation in %s. Read: %ld, Expected: %ld\n", path_name, val16, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LH emulation in %s\n", path_name);

        __asm__ volatile (
            "add t6, gp, %0\n\t"
            "sh %1, 0(t6)\n\t"
            :
            : "r" (offset_16), "r" (store_val)
            : "t6", "memory");
        if (global_var_16 != store_val) {
            printf("[FAIL] Failed SH in %s. In memory: %d, Expected: %d\n", path_name, global_var_16, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SH emulation in %s\n", path_name);
    }

    /* --- TEST LB/SB --- */
    printf("\n[*] Testing LB/SB (8-bit signed) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        int64_t val8 = 0;
        int64_t expected_load = (i == 0) ? -10 : -50;
        int8_t store_val      = (i == 0) ? -50 : -100;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "lb %0, 0(t6)\n\t"
            : "=r" (val8)
            : "r" (offset_8)
            : "t6");
        if (val8 != expected_load) {
            printf("[FAIL] Failed LB emulation in %s. Read: %ld, Expected: %ld\n", path_name, val8, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LB emulation in %s\n", path_name);

        __asm__ volatile ("add t6, gp, %0\n\t"
            "sb %1, 0(t6)\n\t"
            :
            : "r" (offset_8), "r" (store_val)
            : "t6", "memory");
        if (global_var_8 != store_val) {
            printf("[FAIL] Failed SB in %s. In memory: %d, Expected: %d\n", path_name, global_var_8, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SB emulation in %s\n", path_name);
    }

    /* --- TEST LWU/SW --- */
    printf("\n[*] Testing LWU/SW (32-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint64_t val_u32 = 0; // 64-bit receiving register to verify zero-extension
        uint64_t expected_load = (i == 0) ? 0x8AAAAAAA : 0x9BBBBBBB;
        uint32_t store_val     = (i == 0) ? 0x9BBBBBBB : 0xACCCCCCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "lwu %0, 0(t6)\n\t"
            : "=r" (val_u32)
            : "r" (offset_u32)
            : "t6");
        if (val_u32 != expected_load) {
            printf("[FAIL] Failed LWU emulation in %s. Read: 0x%lX, Expected: 0x%lX\n", path_name, val_u32, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LWU emulation in %s (Zero-extended)\n", path_name);

        __asm__ volatile ("add t6, gp, %0\n\t"
            "sw %1, 0(t6)\n\t"
            :
            : "r" (offset_u32), "r" (store_val)
            : "t6", "memory");
        if (global_var_u32 != store_val) {
            printf("[FAIL] Failed SW for unsigned test in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u32, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SW emulation in %s\n", path_name);
    }

    /* --- TEST LHU/SH --- */
    printf("\n[*] Testing LHU/SH (16-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint64_t val_u16 = 0;
        uint64_t expected_load = (i == 0) ? 0x8AAA : 0x9BBB;
        uint16_t store_val     = (i == 0) ? 0x9BBB : 0xACCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile ("add t6, gp, %1\n\t"
            "lhu %0, 0(t6)\n\t"
            : "=r" (val_u16)
            : "r" (offset_u16)
            : "t6");
        if (val_u16 != expected_load) {
            printf("[FAIL] Failed LHU emulation in %s. Read: 0x%lX, Expected: 0x%lX\n", path_name, val_u16, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LHU emulation in %s (Zero-extended)\n", path_name);

        __asm__ volatile ("add t6, gp, %0\n\t"
            "sh %1, 0(t6)\n\t"
            :
            : "r" (offset_u16), "r" (store_val)
            : "t6", "memory");
        if (global_var_u16 != store_val) {
            printf("[FAIL] Failed SH for unsigned test in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u16, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SH emulation in %s\n", path_name);
    }

    /* --- TEST LBU/SB */
    printf("\n[*] Testing LBU/SB (8-bit unsigned) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        uint64_t val_u8 = 0;
        uint64_t expected_load = (i == 0) ? 0xAA : 0xBB;
        uint8_t store_val      = (i == 0) ? 0xBB : 0xCC;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile ("add t6, gp, %1\n\t"
            "lbu %0, 0(t6)\n\t"
            : "=r" (val_u8)
            : "r" (offset_u8)
            : "t6");
        if (val_u8 != expected_load) {
            printf("[FAIL] Failed LBU emulation in %s. Read: 0x%lX, Expected: 0x%lX\n", path_name, val_u8, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct LBU emulation in %s (Zero-extended)\n", path_name);

        __asm__ volatile ("add t6, gp, %0\n\t"
            "sb %1, 0(t6)\n\t"
            :
            : "r" (offset_u8), "r" (store_val)
            : "t6", "memory");
        if (global_var_u8 != store_val) {
            printf("[FAIL] Failed SB for unsigned test in %s. In memory: 0x%X, Expected: 0x%X\n", path_name, global_var_u8, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct SB emulation in %s\n", path_name);
    }

    /* --- TEST FLW/FSW --- */
    printf("\n[*] Testing FLW/FSW (32-bit Float) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        float val_f32 = 0.0f;
        float expected_load = (i == 0) ? 3.14f : 6.28f;
        float store_val     = (i == 0) ? 6.28f : 9.42f;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "flw %0, 0(t6)\n\t"
            : "=f" (val_f32)
            : "r" (offset_f32)
            : "t6");
        // Using strict equality is safe here because we are directly loading/storing exactly written values
        if (val_f32 != expected_load) {
            printf("[FAIL] Failed FLW emulation in %s. Read: %f, Expected: %f\n", path_name, val_f32, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct FLW emulation in %s\n", path_name);

        __asm__ volatile (
            "add t6, gp, %0\n\t"
            "fsw %1, 0(t6)\n\t"
            :
            : "r" (offset_f32), "f" (store_val)
            : "t6", "memory");
        if (global_var_f32 != store_val) {
            printf("[FAIL] Failed FSW in %s. In memory: %f, Expected: %f\n", path_name, global_var_f32, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct FSW emulation in %s\n", path_name);
    }

    /* --- TEST FLD/FSD --- */
    printf("\n[*] Testing FLD/FSD (64-bit Double) with GP-relative memory access...\n");
    for (int i = 0; i < 2; i++) {
        double val_f64 = 0.0;
        double expected_load = (i == 0) ? 2.718 : 5.436;
        double store_val     = (i == 0) ? 5.436 : 8.154;
        const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";

        __asm__ volatile (
            "add t6, gp, %1\n\t"
            "fld %0, 0(t6)\n\t"
            : "=f" (val_f64)
            : "r" (offset_f64)
            : "t6");
        if (val_f64 != expected_load) {
            printf("[FAIL] Failed FLD emulation in %s. Read: %f, Expected: %f\n", path_name, val_f64, expected_load);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct FLD emulation in %s\n", path_name);

        __asm__ volatile (
            "add t6, gp, %0\n\t"
            "fsd %1, 0(t6)\n\t"
            :
            : "r" (offset_f64), "f" (store_val)
            : "t6", "memory");
        if (global_var_f64 != store_val) {
            printf("[FAIL] Failed FSD in %s. In memory: %f, Expected: %f\n", path_name, global_var_f64, store_val);
            exit_status = EXIT_FAILURE;
        } else printf("[ OK ] Correct FSD emulation in %s\n", path_name);
    }

}

int main() {
    printf("\n=== Inizio Test Accessi Global Pointer (GP) ===\n");
    test_32bit_gp_access();
    printf("=== Test Completati ===\n\n");
    return exit_status;
}