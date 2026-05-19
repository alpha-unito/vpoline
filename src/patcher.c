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

extern void gp_fault_handler(struct context *ctx);

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

    /* --- building trampoline to asm_syscall_hook --- */
    uint64_t dst = (uint64_t) asm_syscall_hook;
    uint32_t upper_32 = (dst & 0xffffffff00000000) >> 32;
    uint32_t lower_32 = (uint32_t) dst;

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
    trampoline[19] = 0x00028067; // jalr zero, t0, 0 -- jump to asm_syscall_hook

    /* --- storing context before calling gp_fault_handler --- */
    trampoline[20] = 0xe1010113; // addi sp, sp, -496
    trampoline[21] = 0x00113023; // sd ra, 0(sp)
    /*
     * at this point gp will hold the return address pointing right after the
     * replaced gp-relative memory accessing instruction from the executable
     */
    trampoline[22] = 0x00313423; // sd gp, 8(sp)
    trampoline[23] = 0x00413823; // sd tp, 16(sp)
    trampoline[24] = 0x00513c23; // sd t0, 24(sp)
    trampoline[25] = 0x02613023; // sd t1, 32(sp)
    trampoline[26] = 0x02713423; // sd t2, 40(sp)
    trampoline[27] = 0x03c13823; // sd t3, 48(sp)
    trampoline[28] = 0x03d13c23; // sd t4, 56(sp)
    trampoline[29] = 0x05e13023; // sd t5, 64(sp)
    trampoline[30] = 0x05f13423; // sd t6, 72(sp)
    trampoline[31] = 0x04a13823; // sd a0, 80(sp)
    trampoline[32] = 0x04b13c23; // sd a1, 88(sp)
    trampoline[33] = 0x06c13023; // sd a2, 96(sp)
    trampoline[34] = 0x06d13423; // sd a3, 104(sp)
    trampoline[35] = 0x06e13823; // sd a4, 112(sp)
    trampoline[36] = 0x06f13c23; // sd a5, 120(sp)
    trampoline[37] = 0x09013023; // sd a6, 128(sp)
    trampoline[38] = 0x09113423; // sd a7, 136(sp)
    trampoline[39] = 0x08813823; // sd s0, 144(sp)
    trampoline[40] = 0x08913c23; // sd s1, 152(sp)
    trampoline[41] = 0x0b213023; // sd s2, 160(sp)
    trampoline[42] = 0x0b313423; // sd s3, 168(sp)
    trampoline[43] = 0x0b413823; // sd s4, 176(sp)
    trampoline[44] = 0x0b513c23; // sd s5, 184(sp)
    trampoline[45] = 0x0d613023; // sd s6, 192(sp)
    trampoline[46] = 0x0d713423; // sd s7, 200(sp)
    trampoline[47] = 0x0d813823; // sd s8, 208(sp)
    trampoline[48] = 0x0d913c23; // sd s9, 216(sp)
    trampoline[49] = 0x0fa13023; // sd s10, 224(sp)
    trampoline[50] = 0x0fb13423; // sd s11, 232(sp)
    trampoline[51] = 0x0e013827; // fsd ft0, 240(sp)
    trampoline[52] = 0x0e113c27; // fsd ft1, 248(sp)
    trampoline[53] = 0x10213027; // fsd ft2, 256(sp)
    trampoline[54] = 0x10313427; // fsd ft3, 264(sp)
    trampoline[55] = 0x10413827; // fsd ft4, 272(sp)
    trampoline[56] = 0x10513c27; // fsd ft5, 280(sp)
    trampoline[57] = 0x12613027; // fsd ft6, 288(sp)
    trampoline[58] = 0x12713427; // fsd ft7, 296(sp)
    trampoline[59] = 0x13c13827; // fsd ft8, 304(sp)
    trampoline[60] = 0x13d13c27; // fsd ft9, 312(sp)
    trampoline[61] = 0x15e13027; // fsd ft10, 320(sp)
    trampoline[62] = 0x15f13427; // fsd ft11, 328(sp)
    trampoline[63] = 0x14813827; // fsd fs0, 336(sp)
    trampoline[64] = 0x14913c27; // fsd fs1, 344(sp)
    trampoline[65] = 0x17213027; // fsd fs2, 352(sp)
    trampoline[66] = 0x17313427; // fsd fs3, 360(sp)
    trampoline[67] = 0x17413827; // fsd fs4, 368(sp)
    trampoline[68] = 0x17513c27; // fsd fs5, 376(sp)
    trampoline[69] = 0x19613027; // fsd fs6, 384(sp)
    trampoline[70] = 0x19713427; // fsd fs7, 392(sp)
    trampoline[71] = 0x19813827; // fsd fs8, 400(sp)
    trampoline[72] = 0x19913c27; // fsd fs9, 408(sp)
    trampoline[73] = 0x1ba13027; // fsd fs10, 416(sp)
    trampoline[74] = 0x1bb13427; // fsd fs11, 424(sp)
    trampoline[75] = 0x1aa13827; // fsd fa0, 432(sp)
    trampoline[76] = 0x1ab13c27; // fsd fa1, 440(sp)
    trampoline[77] = 0x1cc13027; // fsd fa2, 448(sp)
    trampoline[78] = 0x1cd13427; // fsd fa3, 456(sp)
    trampoline[79] = 0x1ce13827; // fsd fa4, 464(sp)
    trampoline[80] = 0x1cf13c27; // fsd fa5, 472(sp)
    trampoline[81] = 0x1f013027; // fsd fa6, 480(sp)
    trampoline[82] = 0x1f113427; // fsd fa7, 488(sp)

    /* --- building trampoline to gp_fault_handler */
    dst = (uint64_t)gp_fault_handler;
    upper_32 = (dst & 0xffffffff00000000) >> 32;
    lower_32 = (uint32_t) dst;

    lui_imm_field = upper_32 & 0xfffff000;
    addi_imm_field = 0;
    if (upper_32 % 4096 != 0) {
        addi_imm_field = upper_32 - lui_imm_field;
        if (addi_imm_field > 2047) {
            lui_imm_field += 4096;
            addi_imm_field = -(4096 - addi_imm_field);
        }
    }
    trampoline[83] = 0x000002b7 | lui_imm_field; // lui t0, 0x.....
    trampoline[84] = 0x00028293 | (addi_imm_field << 20); // addi t0, t0, 0x...
    trampoline[85] = 0x02029293; // slli t0, t0, 32

    lui_imm_field = lower_32 & 0xfffff000;
    addi_imm_field = 0;
    if (lower_32 % 4096 != 0) {
        addi_imm_field = lower_32 - lui_imm_field;
        if (addi_imm_field > 2047) {
            lui_imm_field += 4096;
            addi_imm_field = -(4096 - addi_imm_field);
        }
    }
    trampoline[86] = 0x00000e37 | lui_imm_field; // lui t3, 0x.....
    trampoline[87] = 0x000e0e13 | (addi_imm_field << 20); // addi t3, t3, 0x...

    trampoline[88] = 0x7ffff337; // lui t1, 0x7ffff
    trampoline[89] = 0x7ff06393; // ori t2, zero, 0x7ff
    trampoline[90] = 0x00139393; // slli t2, t2, 1
    trampoline[91] = 0x0013e393; // ori t2, t2, 1
    trampoline[92] = 0x00736333; // or t1, t1, t2
    trampoline[93] = 0x00131313; // slli t1, t1, 1
    trampoline[94] = 0x00136313; // ori t1, t1, 1
    trampoline[95] = 0x006e7e33; // and t3, t3, t1
    trampoline[96] = 0x01c2e2b3; // or t0, t0, t3
    /*
     * before jumping we need to pass sp to gp_fault_handler which will cast
     * it as a pointer to struct context
     */
    trampoline[97] = 0x00010513; // addi a0, sp, 0
    trampoline[98] = 0x000280e7; // jalr ra, t0, 0 -- calling gp_fault_handler

    /* --- restoring context from the stack */
    /*
     * if a load instruction was emulated within gp_fault_handler, then the
     * loaded value has been already placed on the stack where the destination
     * register was saved before
     */
    trampoline[99] = 0x00013083; // ld ra, 0(sp)
    trampoline[100] = 0x00813183; // ld gp, 8(sp)
    trampoline[101] = 0x01013203; // ld tp, 16(sp)
    trampoline[102] = 0x01813283; // ld t0, 24(sp)
    trampoline[103] = 0x02013303; // ld t1, 32(sp)
    trampoline[104] = 0x02813383; // ld t2, 40(sp)
    trampoline[105] = 0x03013e03; // ld t3, 48(sp)
    trampoline[106] = 0x03813e83; // ld t4, 56(sp)
    trampoline[107] = 0x04013f03; // ld t5, 64(sp)
    trampoline[108] = 0x04813f83; // ld t6, 72(sp)
    trampoline[109] = 0x05013503; // ld a0, 80(sp)
    trampoline[110] = 0x05813583; // ld a1, 88(sp)
    trampoline[111] = 0x06013603; // ld a2, 96(sp)
    trampoline[112] = 0x06813683; // ld a3, 104(sp)
    trampoline[113] = 0x07013703; // ld a4, 112(sp)
    trampoline[114] = 0x07813783; // ld a5, 120(sp)
    trampoline[115] = 0x08013803; // ld a6, 128(sp)
    trampoline[116] = 0x08813883; // ld a7, 136(sp)
    trampoline[117] = 0x09013403; // ld s0, 144(sp)
    trampoline[118] = 0x09813483; // ld s1, 152(sp)
    trampoline[119] = 0x0a013903; // ld s2, 160(sp)
    trampoline[120] = 0x0a813983; // ld s3, 168(sp)
    trampoline[121] = 0x0b013a03; // ld s4, 176(sp)
    trampoline[122] = 0x0b813a83; // ld s5, 184(sp)
    trampoline[123] = 0x0c013b03; // ld s6, 192(sp)
    trampoline[124] = 0x0c813b83; // ld s7, 200(sp)
    trampoline[125] = 0x0d013c03; // ld s8, 208(sp)
    trampoline[126] = 0x0d813c83; // ld s9, 216(sp)
    trampoline[127] = 0x0e013d03; // ld s10, 224(sp)
    trampoline[128] = 0x0e813d83; // ld s11, 232(sp)
    trampoline[129] = 0x0f013007; // fld ft0, 240(sp)
    trampoline[130] = 0x0f813087; // fld ft1, 248(sp)
    trampoline[131] = 0x10013107; // fld ft2, 256(sp)
    trampoline[132] = 0x10813187; // fld ft3, 264(sp)
    trampoline[133] = 0x11013207; // fld ft4, 272(sp)
    trampoline[134] = 0x11813287; // fld ft5, 280(sp)
    trampoline[135] = 0x12013307; // fld ft6, 288(sp)
    trampoline[136] = 0x12813387; // fld ft7, 296(sp)
    trampoline[137] = 0x13013e07; // fld ft8, 304(sp)
    trampoline[138] = 0x13813e87; // fld ft9, 312(sp)
    trampoline[139] = 0x14013f07; // fld ft10, 320(sp)
    trampoline[140] = 0x14813f87; // fld ft11, 328(sp)
    trampoline[141] = 0x15013407; // fld fs0, 336(sp)
    trampoline[142] = 0x15813487; // fld fs1, 344(sp)
    trampoline[143] = 0x16013907; // fld fs2, 352(sp)
    trampoline[144] = 0x16813987; // fld fs3, 360(sp)
    trampoline[145] = 0x17013a07; // fld fs4, 368(sp)
    trampoline[146] = 0x17813a87; // fld fs5, 376(sp)
    trampoline[147] = 0x18013b07; // fld fs6, 384(sp)
    trampoline[148] = 0x18813b87; // fld fs7, 392(sp)
    trampoline[149] = 0x19013c07; // fld fs8, 400(sp)
    trampoline[150] = 0x19813c87; // fld fs9, 408(sp)
    trampoline[151] = 0x1a013d07; // fld fs10, 416(sp)
    trampoline[152] = 0x1a813d87; // fld fs11, 424(sp)
    trampoline[153] = 0x1b013507; // fld fa0, 432(sp)
    trampoline[154] = 0x1b813587; // fld fa1, 440(sp)
    trampoline[155] = 0x1c013607; // fld fa2, 448(sp)
    trampoline[156] = 0x1c813687; // fld fa3, 456(sp)
    trampoline[157] = 0x1d013707; // fld fa4, 464(sp)
    trampoline[158] = 0x1d813787; // fld fa5, 472(sp)
    trampoline[159] = 0x1e013807; // fld fa6, 480(sp)
    trampoline[160] = 0x1e813887; // fld fa7, 488(sp)
    trampoline[161] = 0x1f010113; // addi sp, sp, 496

    /* --- jump to relocated_gp - 4 (before_gp[13]) --- */
    trampoline[162] = 0xd75ff06f; // j before_gp[13] -- see setup_return()
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
