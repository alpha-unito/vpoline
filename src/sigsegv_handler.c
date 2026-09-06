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
#include <stdatomic.h>

#include "patcher.h"
#include "utils.h"

#define KERNEL_SIGSETSIZE 8

struct sigaction user_sigsegv_act;
bool user_sigsegv_registered = false;

extern long syscall_no_intercept(long, ...);

extern pid_t active_tids[MAX_THREADS];
extern atomic_flag tid_list_lock;

static atomic_bool patch_in_progress = ATOMIC_VAR_INIT(false);
static atomic_int threads_parked = ATOMIC_VAR_INIT(0);

gp_instr_t *gp_instruction_map = NULL;
size_t map_capacity = 0;
size_t map_size = 0;

static atomic_flag patch_lock = ATOMIC_FLAG_INIT;

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
    char buf[160];
    snprintf(buf, sizeof(buf),
        "Segmentation fault at address: 0x%" PRIXPTR "\n", (uintptr_t)fault_addr);
    syscall_no_intercept(SYS_write, 2, buf, strlen(buf));
#endif

    ucontext_t *ctx = (ucontext_t *)context;

    /* retrieving PC value at the moment of segfault */
    const unsigned long pc = ctx->uc_mcontext.__gregs[REG_PC];


    /*
     * computing XOM boundaries; if segfault is issued there we need to forward
     * the read/write access to the backup memory region.
     */
    const uintptr_t xom_start = (uintptr_t)rsi.ret_sequence_page_addr;
    const uintptr_t xom_end = xom_start + rsi.protection_size - 1;

    /*
     * if segfault is issued anywhere else OR
     * faulting instruction's rs1 is different from gp (x3), then we forward
     * SIGSEGV to the user registered handler (if any) or to the default kernel
     * handler
     */
    // TODO: this works fine, but we need to reconsider how we treat compressed
    //  instructions for coherence reasons
    if ((fault_addr < xom_start || fault_addr > xom_end) ||
        ((*(uint16_t *)pc & 0x3) != 0x3) ||
        (((*(uint32_t *)pc >> 15) & 0x1F) != 0x3)) {
#ifdef DEBUG
        snprintf(buf, sizeof(buf),
            "Segfault at addr 0x%" PRIXPTR " outside XOM region or not caused by GP\n"
            "Forwarding SIGSEGV handling to old handler or default handler\n", fault_addr);
        syscall_no_intercept(SYS_write, 2, buf, strlen(buf));
#endif
        if (user_sigsegv_registered &&
            user_sigsegv_act.sa_handler != SIG_DFL &&
            user_sigsegv_act.sa_handler != SIG_IGN ) {
            if (user_sigsegv_act.sa_flags & SA_SIGINFO) {
                user_sigsegv_act.sa_sigaction(sig, si, context);
            } else {
                user_sigsegv_act.sa_handler(sig);
            }
            } else {
                struct sigaction dfl_act = {0};
                dfl_act.sa_handler = SIG_DFL;
                /*
                 * we're bypassing glibc implementation of sigaction, then we need
                 * to manually hardcode the last parameter of rt_sigaction
                 *
                 * check disassembly of __libc_sigaction as example (last
                 * instruction before ecall is c.li a3, 8)
                 */
                syscall_no_intercept(SYS_rt_sigaction,SIGSEGV,&dfl_act,NULL,KERNEL_SIGSETSIZE);
            }
        return;
    }

    /* computing correspondant address in the backup area */
    const uintptr_t offset = fault_addr - xom_start;
    const uintptr_t backup_addr = (uintptr_t)rsi.start_addr_page + offset;

    while (atomic_flag_test_and_set_explicit(&patch_lock, memory_order_acquire)) {
        /* spin until lock is released */
    }

    if (*(uint32_t *)pc == 0x050181e7) {
        atomic_flag_clear_explicit(&patch_lock, memory_order_release);
        /*
         * by not incrementing PC we ensure that the thread resumes execution
         * at the patched instruction, jumping directly to the user space
         * handling
         */
        return;
    }

    bool is_unaligned = (pc & 0x3) != 0;
    bool require_stw = is_unaligned || !ziccif_supported;

    pid_t my_tid = (pid_t)syscall_no_intercept(SYS_gettid);
    pid_t my_tgid = (pid_t)syscall_no_intercept(SYS_getpid);
    int signaled_threads = 0;

    if (require_stw) {
        atomic_store_explicit(&patch_in_progress, true, memory_order_release);

        while (atomic_flag_test_and_set_explicit(&tid_list_lock, memory_order_acquire));
        for (int i = 0; i < MAX_THREADS; i++) {
            pid_t target_tid = active_tids[i];
            if (target_tid != 0 && target_tid != my_tid) {
                long ret = syscall_no_intercept(SYS_tgkill, my_tgid, target_tid, SIGUSR1);
                if (ret == -3) { // -ESRCH (thread is terminated)
                    active_tids[i] = 0;
                } else if (ret == 0) {
                    signaled_threads++;
                }
            }
        }
        atomic_flag_clear_explicit(&tid_list_lock, memory_order_release);

        /* wait until all signaled threads enter in stw_signal_handler */
        while (atomic_load_explicit(&threads_parked, memory_order_acquire) < signaled_threads) {
            __asm__ volatile (".word 0x0100000F" ::: "memory");
        }
    }

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

#ifndef __NR_riscv_flush_icache
#define __NR_riscv_flush_icache 259
#endif
        uintptr_t patch_addr = ctx->uc_mcontext.__gregs[REG_PC];
        syscall_no_intercept(__NR_riscv_flush_icache, patch_addr, patch_addr + pc_step, 0);

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

    if (require_stw) {
        atomic_store_explicit(&patch_in_progress, false, memory_order_release);
        while (atomic_load_explicit(&threads_parked, memory_order_acquire) > 0) {
            __asm__ volatile (".word 0x0100000F" ::: "memory");
        }
    }

    atomic_flag_clear_explicit(&patch_lock, memory_order_release);
}

void stw_signal_handler(int sig, siginfo_t *si, void *context) {
#ifdef DEBUG
    const char msg_enter[] = "[STW-DEBUG] Signal SIGUSR1 received: pausing thread...\n";
    syscall_no_intercept(SYS_write, 2, msg_enter, sizeof(msg_enter) - 1);
#endif
    atomic_fetch_add_explicit(&threads_parked, 1, memory_order_acq_rel);
    while (atomic_load_explicit(&patch_in_progress, memory_order_acquire)) {
        __asm__ volatile (".word 0x0100000F" ::: "memory");
    }
    atomic_fetch_sub_explicit(&threads_parked, 1, memory_order_acq_rel);
#ifdef DEBUG
    const char msg_exit[] = "[STW-DEBUG] Patching completed: thread restarting\n";
    syscall_no_intercept(SYS_write, 2, msg_exit, sizeof(msg_exit) - 1);
#endif
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

    struct sigaction sa_stw;
    memset(&sa_stw, 0, sizeof(sa_stw));
    sa_stw.sa_flags = SA_SIGINFO | SA_RESTART;
    sa_stw.sa_sigaction = stw_signal_handler;
    sigaction(SIGUSR1, &sa_stw, NULL);
}
