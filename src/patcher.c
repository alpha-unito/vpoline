/**
 * Copyright 2026 University of Turin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "patcher.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>

extern void asm_syscall_hook(void);

/**
 * The trampoline consists of storing on the stack the values of t0-t3, loading
 * asm_syscall_hook address into t0 and jumping to it, where the actual
 * interception will be performed. The trampoline is placed right at
 * relocated_global_pointer, so that each JALR GP, GP, A7 replacing ECALL will
 * jump here.
 * -----------------------------------------------------------------------------
 * Here we encode directly in memory the instructions which will perform
 * the jump to asm_syscall_hook. Pseudo-instruction is used for readability, but
 * LA is manually emulated. We don't encode what a compiler would for an LA
 *
 * la t0, asm_syscall_hook
 * jr t0
 */

void setup_shadow_page_trampoline()
{
    uint32_t *trampoline = (uint32_t *)rsi.relocated_gp;
#ifdef DEBUG
    printf("writing trampoline to asm_syscall_hook at: 0x%" PRIXPTR "\n", (uintptr_t)trampoline);
#endif
    const uint64_t dst = (uint64_t) asm_syscall_hook;
    const uint32_t upper_32 = (dst & 0xffffffff00000000) >> 32;
    const uint32_t lower_32 = (uint32_t) dst;

    trampoline[0] = 0xed010113; // addi sp, sp, -304
    trampoline[1] = 0x00513023; // sd t0, 0(sp)
    trampoline[2] = 0x00613423; // sd t1, 8(sp)
    trampoline[3] = 0x00713823; // sd t2, 16(sp)
    trampoline[4] = 0x01c13c23; // sd t3, 24(sp)

    uint32_t lui_imm_field = upper_32 & 0xfffff000;
    uint32_t addi_imm_field = 0;
    if (upper_32 % 4096 != 0) {
        addi_imm_field = upper_32 - lui_imm_field;
        if (addi_imm_field > 2047) {
            lui_imm_field += 4096;
            addi_imm_field = -(4096 - addi_imm_field);
        }
    }
    trampoline[5] = 0x000002b7 | lui_imm_field; // lui t0, 0x.....
    trampoline[6] = 0x00028293 | (addi_imm_field << 20); // addi t0, t0, 0x...
    trampoline[7] = 0x02029293; // slli t0, t0, 32

    lui_imm_field = lower_32 & 0xfffff000;
    addi_imm_field = 0;
    if (lower_32 % 4096 != 0) {
        addi_imm_field = lower_32 - lui_imm_field;
        if (addi_imm_field > 2047) {
            lui_imm_field += 4096;
            addi_imm_field = -(4096 - addi_imm_field);
        }
    }
    trampoline[8] = 0x00000e37 | lui_imm_field; // lui t3, 0x.....
    trampoline[9] = 0x000e0e13 | (addi_imm_field << 20); // addi t3, t3, 0x...

    trampoline[10] = 0x7ffff337; // lui t1, 0x7ffff
    trampoline[11] = 0x7ff06393; // ori t2, zero, 0x7ff
    trampoline[12] = 0x00139393; // slli t2, t2, 1
    trampoline[13] = 0x0013e393; // ori t2, t2, 1
    trampoline[14] = 0x00736333; // or t1, t1, t2
    trampoline[15] = 0x00131313; // slli t1, t1, 1
    trampoline[16] = 0x00136313; // ori t1, t1, 1
    trampoline[17] = 0x006e7e33; // and t3, t3, t1
    trampoline[18] = 0x01c2e2b3; // or t0, t0, t3
    trampoline[19] = 0x00028067; // jalr zero, t0, 0
}

/**
 * Writes directly in memory three different return paths right all ending with
 * a JALR GP, GP, 0 instruction placed 4 bytes before relocated_global_pointer.
 * GP will hold the glibc_ra value thanks to LD, GP, 288(SP) matching the
 * SD GP, 288(SP) in main.c:183.
 * The first path is for executions where the hook function handled the system
 * call on its own.
 * The second path for executions where libvpoline must issue the system call if
 * hook function signaled to forward if to the kernel.
 * The third path is for clone and fork which need a full context restore before
 * being forwarded to kernel. Without it the memory space of the child thread or
 * process would be compromised.
 */
void setup_return(void)
{
    assert(!((uintptr_t)rsi.ret_sequence_page_addr % page_size));

    uint32_t *before_gp = rsi.start_addr;

#ifdef DEBUG
    printf("Writing return sequence at: 0x%" PRIXPTR " - 0x%" PRIXPTR "\n",
        (uintptr_t)before_gp, (uintptr_t)before_gp + RET_SEQUENCE_SIZE);
#endif

    /* path for already handled system calls */
    before_gp[0] = 0x12013183; // ld gp, 288(sp)
    before_gp[1] = 0x00013283; // ld t0, 0(sp)
    before_gp[2] = 0x13010113; // addi sp, sp, 304
    before_gp[3] = 0x0280006f; // j before_gp[13]
    /* normal system call path */
    before_gp[4] = 0x00000073; // ecall
    before_gp[5] = 0x12013183; // ld gp, 288(sp)
    before_gp[6] = 0x00013283; // ld t0, 0(sp)
    before_gp[7] = 0x13010113; // addi sp, sp, 304
    before_gp[8] = 0x0140006f; // j before_gp[13]
    /* clone/clone3 path */
    before_gp[9] = 0x12013183; // ld gp, 288(sp)
    before_gp[10] = 0x00013283; // ld t0, 0(sp)
    before_gp[11] = 0x13010113; // addi sp, sp, 304
    before_gp[12] = 0x00000073; // ecall
    before_gp[13] = 0x000181e7; // jalr gp, gp, 0

#ifdef DEBUG
    printf("Memory content of returning sequence:\n"
           "0x%" PRIXPTR ":\t0x%08x\n"
           "0x%" PRIXPTR ":\t0x%08x\n"
           "0x%" PRIXPTR ":\t0x%08x\n",
           (uintptr_t)(before_gp),*(before_gp),
           (uintptr_t)(before_gp+1),*(before_gp+1),
           (uintptr_t)(before_gp+2),*(before_gp+2));
#endif


    if (mprotect(rsi.ret_sequence_page_addr,RET_SEQUENCE_SIZE+rsi.start_offset,
        PROT_EXEC) == -1) {
        fprintf(stderr, "mprotect failed in setup_return\n");
        exit(1);
    }
}
