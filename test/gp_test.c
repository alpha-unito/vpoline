#include <stdio.h>
#include <stdint.h>

/* Variabili globali: il linker deciderà dove metterle, ma noi
 * calcoleremo dinamicamente il loro offset dal GP originale! */
int32_t global_var_32 = 42;
int64_t global_var_64 = 100;

/* Esponiamo il simbolo magico generato dal linker RISC-V */
extern char __global_pointer$[];

void test_32bit_gp_access() {
    int32_t val32 = 0;
    int64_t val64 = 0;

    /* Calcoliamo l'offset esatto delle variabili dal GP originale.
     * Questa sottrazione viene risolta tranquillamente a compile/link time! */
    intptr_t offset_32 = (char*)&global_var_32 - __global_pointer$;
    intptr_t offset_64 = (char*)&global_var_64 - __global_pointer$;

    printf("[*] Test istruzioni 32-bit su Shadow Page GP (XOM)...\n");
    for (int i = 0; i < 2; i++) {

        /* TEST LOAD 32-bit (lw) */
        __asm__ volatile (
            "add t6, gp, %1\n\t"  // Calcola indirizzo effettivo nella pagina XOM
            "lw %0, 0(t6)\n\t"    // BAM! Segfault gestito da vpoline
            : "=r" (val32)
            : "r" (offset_32)
            : "t6"
        );
        if (i == 0) {
            if (val32 != 42) printf("[FAIL] LW fallita. Letto %d\n", val32);
            else printf("[ OK ] LW emulata correttamente.\n");
        } else {
            if (val32 != 84) printf("[FAIL] LW fallita. Letto %d\n", val32);
            else printf("[ OK ] LW emulata correttamente.\n");
        }

        /* TEST STORE 32-bit (sw) */
        val32 = 84;
        __asm__ volatile (
            "add a5, gp, %0\n\t"
            "sw %1, 0(a5)\n\t"
            :
            : "r" (offset_32), "r" (val32)
            : "a5", "memory"
        );
        if (global_var_32 != 84) printf("[FAIL] SW fallita.\n");
        else printf("[ OK ] SW emulata correttamente.\n");

    }


    // /* TEST LOAD 64-bit (ld) */
    // __asm__ volatile (
    //     "add a5, gp, %1\n\t"
    //     "ld %0, 0(a5)\n\t"
    //     : "=r" (val64)
    //     : "r" (offset_64)
    //     : "a5"
    // );
    // if (val64 != 100) printf("[FAIL] LD fallita.\n");
    // else printf("[ OK ] LD emulata correttamente.\n");
    //
    // /* TEST STORE 64-bit (sd) */
    // val64 = 200;
    // __asm__ volatile (
    //     "add a5, gp, %0\n\t"
    //     "sd %1, 0(a5)\n\t"
    //     :
    //     : "r" (offset_64), "r" (val64)
    //     : "a5", "memory"
    // );
    // if (global_var_64 != 200) printf("[FAIL] SD fallita.\n");
    // else printf("[ OK ] SD emulata correttamente.\n");
}

void test_compressed_gp_access() {
    int32_t val32 = 0;
    intptr_t offset_32 = (char*)&global_var_32 - __global_pointer$;

    printf("[*] Test istruzioni Compresse (16-bit) su Shadow Page GP...\n");

    /* TEST LOAD COMPRESSA (c.lw)
     * Usiamo forzatamente a4 e a5 perché le istruzioni compresse
     * accettano solo registri nel range x8-x15. */
    __asm__ volatile (
        "add a5, gp, %1\n\t"
        "c.lw a4, 0(a5)\n\t"   // <-- Questo innescherà il ramo is_32bit == false!
        "mv %0, a4\n\t"
        : "=r" (val32)
        : "r" (offset_32)
        : "a4", "a5"
    );
    if (val32 != 84) printf("[FAIL] C.LW fallita. Letto %d\n", val32);
    else printf("[ OK ] C.LW emulata correttamente.\n");

    /* TEST STORE COMPRESSA (c.sw) */
    val32 = 999;
    __asm__ volatile (
        "mv a4, %1\n\t"
        "add a5, gp, %0\n\t"
        "c.sw a4, 0(a5)\n\t"
        :
        : "r" (offset_32), "r" (val32)
        : "a4", "a5", "memory"
    );
    if (global_var_32 != 999) printf("[FAIL] C.SW fallita.\n");
    else printf("[ OK ] C.SW emulata correttamente.\n");
}

int main() {
    printf("\n=== Inizio Test Accessi Global Pointer (GP) ===\n");
    test_32bit_gp_access();
    // test_compressed_gp_access();
    printf("=== Test Completati ===\n\n");
    return 0;
}