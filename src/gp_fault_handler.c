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

#include <stdio.h>
#include <syscall.h>

#include "sigsegv_handler.h"
#include "rv_opcodes.h"

extern long syscall_no_intercept(long, ...);

extern uintptr_t relocated_global_pointer;

extern gp_instr_t *gp_instruction_map;
extern size_t map_capacity;
extern size_t map_size;


static inline uint64_t get_reg(const struct context *ctx, uint8_t reg) {
    switch(reg) {
        case 0: return 0;
        case 1: return ctx->ra;
        case 2: return (long)ctx + 496; // original SP before trampoline allocation
        case 3: return (long)relocated_global_pointer; // original GP value
        case 4: return ctx->tp;
        case 5: case 6: case 7:
            return ctx->t[reg - 5];     // t0-t2
        case 8: case 9:
            return ctx->s[reg - 8];     // s0-s1
        case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
            return ctx->a[reg - 10];    // a0-a7
        case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27:
            return ctx->s[reg - 16];    // s2-s11
        case 28: case 29: case 30: case 31:
            return ctx->t[reg - 25];    // t3-t6
        default: {
                char buf[80];
                int len = sprintf(buf, "Error: gp_fault_handler - unrecognized register in get_reg %d\n", reg);
                syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
                __builtin_trap();
#endif
                syscall_no_intercept(SYS_exit_group, 1);
                return 0;
        }
    }
}

static inline uint64_t get_freg(const struct context *ctx, uint8_t reg) {
    if (reg <= 7) return ctx->ft[reg];                  // ft0-ft7
    if (reg <= 9) return ctx->fs[reg - 8];              // fs0-fs1
    if (reg <= 17) return ctx->fa[reg - 10];            // fa0-fa7
    if (reg <= 27) return ctx->fs[reg - 16];            // fs2-fs11
    if (reg <= 31) return ctx->ft[reg - 20];            // ft8-ft11
    char buf[80];
    int len = sprintf(buf, "Error: gp_fault_handler - unrecognized register in get_freg %d\n", reg);
    syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
    __builtin_trap();
#endif
    syscall_no_intercept(SYS_exit_group, 1);
    return 0;
}

static inline void set_reg(struct context *ctx, uint8_t reg, uint64_t val) {
    char buf[100];
    int len;
    switch(reg) {
        case 0: return; // zero register ignores any write
        case 1: ctx->ra = val; break;
        case 2:
            len = sprintf(buf, "Error: gp_fault_handler:set_reg - not supporting instructions which access GP to write SP\n");
            syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
            break;  // SP non dovrebbe essere bersaglio qui
        case 3:
            len = sprintf(buf, "Error: gp_fault_handler:set_reg - psABI violation: attempt to write to GP register in set_reg\n");
            syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
            break;  // GP viene ripristinato automaticamente
        case 4: ctx->tp = val; break;
        case 5: case 6: case 7:
            ctx->t[reg - 5] = val; break;
        case 8: case 9:
            ctx->s[reg - 8] = val; break;
        case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
            ctx->a[reg - 10] = val; break;
        case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27:
            ctx->s[reg - 16] = val; break;
        case 28: case 29: case 30: case 31:
            ctx->t[reg - 25] = val; break;
        default:
            len = sprintf(buf, "Error: gp_fault_handler - unrecognized register in set_reg %d\n", reg);
            syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
            __builtin_trap();
#endif
            syscall_no_intercept(SYS_exit_group, 1);
    }
}

static inline void set_freg(struct context *ctx, uint8_t reg, uint64_t val) {
    if (reg <= 7) ctx->ft[reg] = val;
    else if (reg <= 9) ctx->fs[reg - 8] = val;
    else if (reg <= 17) ctx->fa[reg - 10] = val;
    else if (reg <= 27) ctx->fs[reg - 16] = val;
    else if (reg <= 31) ctx->ft[reg - 20] = val;
    else {
        char buf[80];
        int len = sprintf(buf, "Error: gp_fault_handler - unrecognized register in set_freg %d\n", reg);
        syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
        __builtin_trap();
#endif
        syscall_no_intercept(SYS_exit_group, 1);
    }
}

uint32_t lookup_instruction(uintptr_t addr) {
    for (size_t i = 0; i < map_size; i++) {
        if (gp_instruction_map[i].address == addr) {
            return gp_instruction_map[i].encoding;
        }
    }
    return 0;
}

void gp_fault_handler(struct context *ctx) {
    uintptr_t ins_addr = ctx->gp - 4;

    uint32_t encoding = lookup_instruction(ins_addr);
    if (!encoding) {
        const char err_msg[] = "Error: gp_fault_handler unable to find encoding for provided address\n";
        syscall_no_intercept(SYS_write, 2, err_msg, sizeof(err_msg) - 1);
#ifdef DEBUG
        __builtin_trap();
#endif
        syscall_no_intercept(SYS_exit_group, 1);
    }

    uint8_t opcode = encoding & 0x7F;
    uint8_t rd     = (encoding >> 7) & 0x1F;
    uint8_t funct3 = (encoding >> 12) & 0x7;
    uint8_t rs1    = (encoding >> 15) & 0x1F;
    uint8_t rs2    = (encoding >> 20) & 0x1F;

    int32_t imm_i = ((int32_t)encoding) >> 20;
    int32_t imm_s = (((int32_t)encoding) >> 25) << 5;
    imm_s |= ((encoding >> 7) & 0x1F);

    uintptr_t base_addr = get_reg(ctx, rs1);

    uint64_t val = 0;
    if (opcode == LOAD_OPCODE || opcode == FLOAD_OPCODE) {

        uintptr_t fault_addr = base_addr + imm_i;
        uintptr_t offset = fault_addr - (uintptr_t)rsi.ret_sequence_page_addr;
        uintptr_t backup_addr = (uintptr_t)rsi.start_addr_page + offset;

        if (opcode == LOAD_OPCODE) {
            switch(funct3) {
                case LB_FUNCT3:  val = (int64_t)(*(int8_t *)backup_addr); break;
                case LH_FUNCT3:  val = (int64_t)(*(int16_t *)backup_addr); break;
                case LW_FUNCT3:  val = (int64_t)(*(int32_t *)backup_addr); break;
                case LD_FUNCT3:  val = *(int64_t *)backup_addr; break;
                case LBU_FUNCT3: val = *(uint8_t *)backup_addr; break;
                case LHU_FUNCT3: val = *(uint16_t *)backup_addr; break;
                case LWU_FUNCT3: val = *(uint32_t *)backup_addr; break;
                default: {
                        char buf[80];
                        int len = sprintf(buf, "Error: unrecognized func3 field in LOAD_OPCODE disassembly %x\n",funct3);
                        syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
                        __builtin_trap();
#endif
                        syscall_no_intercept(SYS_exit_group, 1);
                }
            }
            set_reg(ctx, rd, val);
        }
        else { // FLOAD_OPCODE
            switch(funct3) {
                case FLW_FUNCT3:
                    // NaN boxing for 32-bit float hardware emulation
                    val = 0xFFFFFFFF00000000ULL | *(uint32_t*)backup_addr;
                    set_freg(ctx, rd, val);
                    break;
                case FLD_FUNCT3:
                    set_freg(ctx, rd, *(uint64_t *)backup_addr);
                    break;
                default: {
                    char buf[80];
                    int len = sprintf(buf, "Error: unrecognized func3 field in FLOAD_OPCODE disassembly %x\n",funct3);
                    syscall_no_intercept(SYS_write, 2, buf, len);
                }
#ifdef DEBUG
                    __builtin_trap();
#endif
                    syscall_no_intercept(SYS_exit_group, 1);
            }
        }
    }
    else if (opcode == STORE_OPCODE || opcode == FSTORE_OPCODE) {

        uintptr_t fault_addr = base_addr + imm_s;
        uintptr_t offset = fault_addr - (uintptr_t)rsi.ret_sequence_page_addr;
        uintptr_t backup_addr = (uintptr_t)rsi.start_addr_page + offset;

        if (opcode == STORE_OPCODE) {
            val = get_reg(ctx, rs2);
            switch(funct3) {
                case SB_FUNCT3: *(int8_t *)backup_addr  = (int8_t)val; break;
                case SH_FUNCT3: *(int16_t *)backup_addr = (int16_t)val; break;
                case SW_FUNCT3: *(int32_t *)backup_addr = (int32_t)val; break;
                case SD_FUNCT3: *(int64_t *)backup_addr = (int64_t)val; break;
                default: {
                    char buf[80];
                    int len = sprintf(buf, "Error: unrecognized func3 field in STORE_OPCODE disassembly %x\n",funct3);
                    syscall_no_intercept(SYS_write, 2, buf, len);
                }
#ifdef DEBUG
                    __builtin_trap();
#endif
                    syscall_no_intercept(SYS_exit_group, 1);
            }
        }
        else { // FSTORE_OPCODE
            val = get_freg(ctx, rs2);
            switch(funct3) {
                case FSW_FUNCT3:
                    *(uint32_t *)backup_addr = val;
                    break;
                case FSD_FUNCT3:
                    *(uint64_t *)backup_addr = val;
                    break;
                default: {
                    char buf[80];
                    int len = sprintf(buf, "Error: unrecognized func3 field in FSTORE_OPCODE disassembly %x\n",funct3);
                    syscall_no_intercept(SYS_write, 2, buf, len);
                }
#ifdef DEBUG
                    __builtin_trap();
#endif
                    syscall_no_intercept(SYS_exit_group, 1);
            }
        }
    } else {
        char buf[80];
        int len = sprintf(buf, "Error: unrecognized opcode in user-space gp_fault_handler %x\n", opcode);
        syscall_no_intercept(SYS_write, 2, buf, len);
#ifdef DEBUG
        __builtin_trap();
#endif
        syscall_no_intercept(SYS_exit_group, 1);
    }

}