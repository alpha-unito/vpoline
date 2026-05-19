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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "sigsegv_handler.h"
#include "rv_opcodes.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <bits/sigaction.h>
#include <inttypes.h>
#include <syscall.h>
#include <sys/mman.h>

#include "patcher.h"

extern long syscall_no_intercept(long, ...);

gp_instr_t *gp_instruction_map = NULL;
size_t map_capacity = 0;
size_t map_size = 0;

static inline uint8_t get_field(const uint32_t instr, const uint32_t start,
                         const uint32_t len)
{
    return (instr >> start) & ((1 << len) - 1);
}

/* TODO: RV64A Atomic Instructions */

void emulate_load_instruction(ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t rd, const uintptr_t backup_addr)
{
    /*
     * if rd is 0 any write to it would be ignored by hardware design but since
     * __gregs[0] actually represents the program counter we need to explicitly
     * avoid any write to PC
     */
    if (rd == 0) return;
    switch (funct3) {
        /* RV64I BASE INTEGER INSTRUCTIONS */
        case LB_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (int64_t)(*(int8_t *)backup_addr);
            break;
        case LH_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (int64_t)(*(int16_t *)backup_addr);
            break;
        case LW_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (int64_t)(*(int32_t *)backup_addr);
            break;
        case LD_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = *(int64_t *)backup_addr;
            break;
        case LBU_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (*(uint8_t *)backup_addr);
            break;
        case LHU_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (*(uint16_t *)backup_addr);
            break;
        case LWU_FUNCT3:
            ctx->uc_mcontext.__gregs[rd] = (*(uint32_t *)backup_addr);
            break;
        default:
            fprintf(stderr,
            "Error: Unrecognized funct3 field for LOAD instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void emulate_fload_instruction(ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t rd, const uintptr_t backup_addr)
{
    switch (funct3) {
        /* FLW performs NaN boxing in hardware, se here it's emulated too */
        case FLW_FUNCT3:
            ctx->uc_mcontext.__fpregs.__d.__f[rd] = 0xFFFFFFFF00000000ULL | *(uint32_t *)backup_addr;
            break;
        case FLD_FUNCT3:
            ctx->uc_mcontext.__fpregs.__d.__f[rd] = *(uint64_t *)backup_addr;
            break;
        default:
            fprintf(stderr,
            "Error: Unrecognized funct3 field for FLOAD instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void emulate_store_instruction(const ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t rs2, const uintptr_t backup_addr)
{
    switch (funct3) {
        /* RV64I BASE INTEGER INSTRUCTIONS */
        case SB_FUNCT3:
            *(int8_t *)backup_addr = rs2 == 0 ? 0 : (int8_t)(ctx->uc_mcontext.__gregs[rs2]);
            break;
        case SH_FUNCT3:
            *(int16_t *)backup_addr = rs2 == 0 ? 0 : (int16_t)(ctx->uc_mcontext.__gregs[rs2]);
            break;
        case SW_FUNCT3:
            *(int32_t *)backup_addr = rs2 == 0 ? 0 : (int32_t)(ctx->uc_mcontext.__gregs[rs2]);
            break;
        case SD_FUNCT3:
            *(int64_t *)backup_addr = rs2 == 0 ? 0 : (int64_t)ctx->uc_mcontext.__gregs[rs2];
            break;
        default:
            fprintf(stderr,
                "Error: Unrecognized funct3 field for STORE instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void emulate_fstore_instruction(const ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t rs2, const uintptr_t backup_addr)
{
    switch (funct3) {
        case FSW_FUNCT3:
            *(uint32_t *)backup_addr = (uint32_t)(ctx->uc_mcontext.__fpregs.__d.__f[rs2]);
            break;
        case FSD_FUNCT3:
            *(uint64_t *)backup_addr = (uint64_t)ctx->uc_mcontext.__fpregs.__d.__f[rs2];
            break;
        default:
            fprintf(stderr,
            "Error: Unrecognized funct3 field for FSTORE instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void emulate_c_load_store_instruction(ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t reg, const uintptr_t backup_addr)
{
    /* RV64C Extension for Compressed Instructions --- non-SP relative */
    switch (funct3) {
        case C_FLD_FUNCT3:
            ctx->uc_mcontext.__fpregs.__d.__f[reg] = *(uint64_t *)backup_addr;
            break;
        case C_LW_FUNCT3:
            ctx->uc_mcontext.__gregs[reg] = (int64_t)(*(int32_t *)backup_addr);
            break;
        case C_LD_FUNCT3:
            ctx->uc_mcontext.__gregs[reg] = *(int64_t *)backup_addr;
            break;
        case C_FSD_FUNCT3:
            *(uint64_t *)backup_addr = ctx->uc_mcontext.__fpregs.__d.__f[reg];
            break;
        case C_SW_FUNCT3:
            *(int32_t *)backup_addr = (int32_t)(ctx->uc_mcontext.__gregs[reg]);
            break;
        case C_SD_FUNCT3:
            *(int64_t *)backup_addr = (int64_t)ctx->uc_mcontext.__gregs[reg];
            break;
        default:
            fprintf(stderr,
            "Error: Unrecognized funct3 field for non-SP relative compressed instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void emulate_sp_rel_c_load_store_instruction(ucontext_t *ctx, const uint8_t funct3,
                                const uint8_t rd, const uint8_t rs2,
                                const uintptr_t backup_addr)
{
    /* RV64C Extension for Compressed Instructions --- SP relative */
    switch (funct3) {
        case C_FLDSP_FUNCT3:
            ctx->uc_mcontext.__fpregs.__d.__f[rd] = *(uint64_t *)backup_addr;
            break;
        case C_LWSP_FUNCT3:
            if (rd != 0) {
                ctx->uc_mcontext.__gregs[rd] = (int64_t)(*(int32_t *)backup_addr);
            }
            break;
        case C_LDSP_FUNCT3:
            if (rd != 0) {
                ctx->uc_mcontext.__gregs[rd] = *(int64_t *)backup_addr;
            }
            break;
        case C_FSDSP_FUNCT3:
            *(uint64_t *)backup_addr = ctx->uc_mcontext.__fpregs.__d.__f[rs2];
            break;
        case C_SWSP_FUNCT3:
            *(int32_t *)backup_addr = rs2 == 0 ? 0 : (int32_t)(ctx->uc_mcontext.__gregs[rs2]);
            break;
        case C_SDSP_FUNCT3:
            *(int64_t *)backup_addr = rs2 == 0 ? 0 : (int64_t)ctx->uc_mcontext.__gregs[rs2];
            break;
        default:
            fprintf(stderr,
            "Error: Unrecognized funct3 field for SP relative compressed instruction\n");
#ifdef DEBUG
            __builtin_trap();
#endif
            exit(1);
    }
}

void register_instruction(uintptr_t addr, uint32_t encoding) {

    if (gp_instruction_map == NULL) {

        map_capacity = INITIAL_CAPACITY;
        long ret = syscall_no_intercept(SYS_mmap, 0, map_capacity * sizeof(gp_instr_t),
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((unsigned long)ret > -4096UL) {
            const char err_msg[] = "Error: Failed to allocate memory for GP-relative instruction map\n";
            syscall_no_intercept(SYS_write, 2, err_msg, sizeof(err_msg) - 1);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
        }
        gp_instruction_map = (gp_instr_t *)ret;

    } else if (map_size >= map_capacity) {

        size_t old_size = map_capacity * sizeof(gp_instr_t);
        map_capacity *= 2;
        size_t new_size = map_capacity * sizeof(gp_instr_t);
        long ret = syscall_no_intercept(SYS_mremap, gp_instruction_map,
                                        old_size, new_size, MREMAP_MAYMOVE);
        if ((unsigned long)ret > -4096UL) {
            const char err_msg[] = "Error: Failed to reallocate memory for GP-relative instruction map\n";
            syscall_no_intercept(SYS_write, 2, err_msg, sizeof(err_msg) - 1);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
        }

        gp_instruction_map = (gp_instr_t *)ret;
    }

    gp_instruction_map[map_size].address = addr;
    gp_instruction_map[map_size].encoding = encoding;
    map_size++;
}

void segfault_handler(int sig, siginfo_t *si, void *context)
{

    /*
     * si_addr holds the address which caused the segfault; not the instruction
     * address but the memory address being accessed
     */
    const uintptr_t fault_addr = (uintptr_t)si->si_addr;

#ifdef DEBUG
    char buf[80];
    snprintf(buf, sizeof(buf),
        "Segmentation fault at address: 0x%" PRIXPTR "\n", (uintptr_t)fault_addr);
    syscall_no_intercept(SYS_write, 2, buf, strlen(buf));
#endif


    /*
     * computing XOM boundaries; if segfault is issued there we need to forward
     * the read/write access to the backup memory region.
     */
    const uintptr_t xom_start = (uintptr_t)rsi.ret_sequence_page_addr;
    const uintptr_t xom_end = xom_start + rsi.protection_size - 1;

    /* if segfault is issued anywhere else, let it be */
    if (fault_addr < xom_start || fault_addr > xom_end) {
#ifdef DEBUG
        snprintf(buf, sizeof(buf),
            "Segfault at addr 0x%" PRIXPTR " outside XOM region\n", fault_addr);
        syscall_no_intercept(SYS_write, 2, buf, strlen(buf));
        __builtin_trap();
        // abort();
#else
        signal(sig, SIG_DFL); // TODO: replace with sigaction
        raise(sig);
#endif
        return;
    }

    /* computing correspondant address in the backup area */
    const uintptr_t offset = fault_addr - xom_start;
    const uintptr_t backup_addr = (uintptr_t)rsi.start_addr_page + offset;

    ucontext_t *ctx = (ucontext_t *)context;

    /* retrieving PC value at the moment of segfault */
    const unsigned long pc = ctx->uc_mcontext.__gregs[REG_PC];

    /*
     * we assume it's a compressed instruction to avoid the remote possibility
     * to cause a segfault if such C instruction it's the last of a page
     */
    const uint16_t c_instr = *(uint16_t *)pc;
    const bool is_32bit = (c_instr & 0x3) == 0x3;


    uint8_t opcode, funct3;
    uint8_t rd, rs2;
    // TODO: add support for all instructions writing or reading in memory from
    //  all ISA extensions. Currently this functions supports I, C and F (need
    //  A too). To check currently supported instructions check rv_opcodes.h
    if (is_32bit) {

        uint32_t instr = *(uint32_t *) pc;

        opcode = get_field(instr, 0, 7);
        funct3 = get_field(instr, 12, 3);

        switch (opcode) {
            case LOAD_OPCODE:
                rd = get_field(instr, 7, 5);
                emulate_load_instruction(ctx, funct3, rd, backup_addr);
                break;
            case FLOAD_OPCODE:
                rd = get_field(instr, 7, 5);
                emulate_fload_instruction(ctx, funct3, rd, backup_addr);
                break;
            case STORE_OPCODE:
                rs2 = get_field(instr, 20, 5);
                emulate_store_instruction(ctx, funct3, rs2, backup_addr);
                break;
            case FSTORE_OPCODE:
                rs2 = get_field(instr, 20, 5);
                emulate_fstore_instruction(ctx, funct3, rs2, backup_addr);
                break;
            default:
                fprintf(stderr,
                    "Error: Unimplemented 32-bit instruction causing fault at XOM region. Opcode: %x\n",opcode);
#ifdef DEBUG
                __builtin_trap();
#endif
                exit(1);
        }

    } else {

        opcode = get_field(c_instr, 0, 2);
        funct3 = get_field(c_instr, 13, 3);
        uint8_t rvc_reg_idx, reg;

        switch (opcode) {
            /* simple compressed load/store instructions */
            case C0_OPCODE:
                /*
                 * at this moment we don't know if a load or a store operation
                 * should be performed as, for example, both C.LW and C.SW
                 * share 0b00 as opcode. On the other hand we know for sure
                 * that both rd' and rs2' are encoded in bits [4:2] of the
                 * instruction, so we can extract the register number and pass
                 * it to the emulation function which will treat it as
                 * destination or source register depending on the funct3 field
                 */
                rvc_reg_idx = get_field(c_instr, 2, 3);
                reg = rvc_reg_map[rvc_reg_idx];
                emulate_c_load_store_instruction(ctx, funct3, reg, backup_addr);
                break;
            /* SP-relative compressed load/store instructions */
            case C2_OPCODE:
                /*
                 * like in the previous case we cannot determine yet if we're
                 * dealing with a load or a store operation. Since C
                 * instructions with opcode 0b10 have rd and rs2 encoded in
                 * different positions, we extract them both from the
                 * instruction and forward them to the emulation function which
                 * will use the appropriate one depending on the funct3 field
                 */
                rd = get_field(c_instr, 7, 5);
                rs2 = get_field(c_instr, 2, 5);
                emulate_sp_rel_c_load_store_instruction(ctx, funct3, rd, rs2, backup_addr);
                break;
            default:
                fprintf(stderr,
                    "Error: Unimplemented 16-bit instruction causing fault at XOM region. Opcode: %x\n",opcode);
                exit(1);
                break;
        }

    }

    const int pc_step = is_32bit ? 4 : 2;

    if (is_32bit) {

        const uintptr_t page_start = pc & ~(page_size -1);
        const size_t prot_len = ((pc + 4 - page_start) <= page_size) ? page_size : page_size*2;

        long ret_prot1 = syscall_no_intercept(SYS_mprotect, page_start, prot_len,
                                              PROT_READ | PROT_WRITE | PROT_EXEC);
        if ((unsigned long)ret_prot1 > -4096UL) {
            const char err_msg[] = "Error: mprotect failed to unlock text page\n";
            syscall_no_intercept(SYS_write, 2, err_msg, sizeof(err_msg) - 1);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
        }

        /*
         * this occurrence of a GP-related memory accessing instruction has been
         * handled. We can now save address and encoding of such instruction and
         * rewrite it with jalr gp, gp, 80
         */
        register_instruction(ctx->uc_mcontext.__gregs[REG_PC],
            *(uint32_t *)ctx->uc_mcontext.__gregs[REG_PC]);
        *(uint32_t *)ctx->uc_mcontext.__gregs[REG_PC] = 0x050181e7; // jalr gp, gp, 80

        long ret_prot2 = syscall_no_intercept(SYS_mprotect, page_start, prot_len,
                                              PROT_READ | PROT_EXEC);
        if ((unsigned long)ret_prot2 > -4096UL) {
            const char err_msg[] = "Error: mprotect failed to lock text page\n";
            syscall_no_intercept(SYS_write, 2, err_msg, sizeof(err_msg) - 1);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
        }
    }

    /*
     * incrementing PC to resume execution at first instruction after the one
     * which caused the segfault
     */
    ctx->uc_mcontext.__gregs[REG_PC] += pc_step;
}

/**
 * Sets up segfault_handler() as the handling function for SIGSEGV signals. In
 * other words, segfault_handler() will be executed at each segmentation fault.
 */
void init_trap_handler(void) {
    struct sigaction sa;
    struct sigaction old_sa;
    memset(&sa, 0, sizeof(sa));

    /*
     * SA_SIGINFO: this flag indicates that we use sa_sigaction instead of
     *             sa_handler, thus having access to siginfo_t and ucontext_t
     * SA_NODEFER: this flag prevents the automatic blocking of SIGSEGV while
     *             the handler is executing
     */
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sa.sa_sigaction = segfault_handler;

    if (sigaction(SIGSEGV, &sa, &old_sa) == -1) {
        perror("Failed to register SIGSEGV handler");
        exit(1);
    }
}
