/**
 * Copyright 2026 University of Turin
 * Copyright 2021 Kenichi Yasukata
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
 *
 * -----------------------------------------------------------------------------
 * MODIFICATION NOTICE:
 * This file contains heavily modified code originally from the zpoline project.
 * -----------------------------------------------------------------------------
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#define PACKAGE "1"
#define PACKAGE_VERSION "1"
#include <sched.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <capstone/capstone.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <stdatomic.h>

#include "main.h"
#include "utils.h"
#include "patcher.h"
#include "sigsegv_handler.h"

extern void setup_ecall(void);
extern long syscall_no_intercept(long syscall_number, ...);
extern long enter_syscall(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

uintptr_t glibc_ra;
uintptr_t virtual_global_pointer;
uintptr_t relocated_global_pointer;
extern struct sigaction user_sigsegv_act;
extern bool user_sigsegv_registered;

size_t page_size;

ReturnSequenceInfo rsi;

pid_t active_tids[MAX_THREADS];
atomic_flag tid_list_lock = ATOMIC_FLAG_INIT;

void ____asm_impl(void)
{

	asm volatile (
		".globl syscall_no_intercept \n\t"
		"syscall_no_intercept: \n\t"
		"mv a7, a0 \n\t"
		"mv a0, a1 \n\t"
		"mv a1, a2 \n\t"
		"mv a2, a3 \n\t"
		"mv a3, a4 \n\t"
		"mv a4, a5 \n\t"
		"mv a5, a6 \n\t"
		".globl setup_ecall \n\t"
		"setup_ecall: \n\t"
		"ecall \n\t"
		"ret \n\t"
	);

	asm volatile (
		".globl asm_syscall_hook \n\t"
		"asm_syscall_hook: \n\t"
        /*
         * t2 is momentarily clobbered by the trampoline, then we can use it
         * as a flag to check if we are executing the post-clone path or not.
         * If t2 == 0 then we are on the regular path -> syscall_hook
         * If t2 == 1 then we are on the post-clone path -> post_clone_hook
         */
		"li t2, 0 \n\t"
		"j bybass_post_clone_path \n\t"

		"post_clone_path: \n\t"
		"addi sp, sp, -304 \n\t"
		"sd t0, 0(sp) \n\t"
		"sd t1, 8(sp) \n\t"
		"sd t2, 16(sp) \n\t"
		"sd t3, 24(sp) \n\t"
		"li t2, 1 \n\t"

        "bybass_post_clone_path: \n\t"
		/*
		 * store &glibc_ra on the stack
		 */
		"sd gp, 288(sp) \n\t"
		"mv t1, gp \n\t" // needed for future check

        /*
         * restoring gp to relocated_global_pointer
         */
		"la t0, relocated_global_pointer \n\t"
		"ld gp, 0(t0) \n\t"

		/*
		 * if rt_sigreturn is intercepted, we don't try to handle it but we
		 * forward it directly to kernel
		 */
		"li t0, 139 \n\t" // __NR_rt_sigreturn
		"beq a7, t0, do_rt_sigreturn \n\t"


		/*
		 * store caller-saved registers on stack except a0, t0-t3 because it
		 * would be useless. a0 will never be restored as it will hold the first
		 * return value from syscall_hook. t0-t3 have been already saved in the
		 * trampoline, as for now they are clobbered
		 */

		"sd ra, 32(sp) \n\t" // original ra value prior to patched ecall
        "sd a1, 40(sp) \n\t"
        "sd a2, 48(sp) \n\t"
        "sd a3, 56(sp) \n\t"
        "sd a4, 64(sp) \n\t"
        "sd a5, 72(sp) \n\t"
        "sd a6, 80(sp) \n\t"
        "sd a7, 88(sp) \n\t"
        "sd t4, 96(sp) \n\t"
        "sd t5, 104(sp) \n\t"
        "sd t6, 112(sp) \n\t"
        "fsd fa0, 120(sp) \n\t"
        "fsd fa1, 128(sp) \n\t"
        "fsd fa2, 136(sp) \n\t"
        "fsd fa3, 144(sp) \n\t"
        "fsd fa4, 152(sp) \n\t"
        "fsd fa5, 160(sp) \n\t"
        "fsd fa6, 168(sp) \n\t"
        "fsd fa7, 176(sp) \n\t"
        "fsd ft0, 184(sp) \n\t"
        "fsd ft1, 192(sp) \n\t"
        "fsd ft2, 200(sp) \n\t"
        "fsd ft3, 208(sp) \n\t"
        "fsd ft4, 216(sp) \n\t"
        "fsd ft5, 224(sp) \n\t"
        "fsd ft6, 232(sp) \n\t"
        "fsd ft7, 240(sp) \n\t"
        "fsd ft8, 248(sp) \n\t"
        "fsd ft9, 256(sp) \n\t"
        "fsd ft10, 264(sp) \n\t"
        "fsd ft11, 272(sp) \n\t"

		/*
		 * since system calls expect up to 6 arguments (a0-a5) and a7 is used
		 * to hold system call number, we can use a6 to pass glibc_ra for
		 * security check
		 */
		"mv a6, t1 \n\t" // needed for future check

		"bnez t2, l0 \n\t"

		"call syscall_hook@plt \n\t"
        "j l1 \n\t"

		"l0: \n\t"
		"call post_clone_hook@plt \n\t"

		"l1: \n\t"
		/*
		 * Now a1 contains 0 or 1, and such value will be passed to t0 as it's
		 * already clobbered while a1 must be restored:
		 * 0: the system call must be forwarded to the kernel and a0 still holds
		 *    the first system call argument. ECALL will be executed before
		 *    returning to the caller.
		 * 1: the system call has been handled in the hook function and a0
		 *    already holds the return value which must be returned to the
		 *    caller. No more ECALL shall be executed before returning to the
		 *    caller.
		 */

		"mv t0, a1\n\t"

		/*
		 * restoring the tmp registers that were used to build the trampoline
		 * towards asm_syscall_hook
		 */
		"ld t1, 8(sp) \n\t"
		"ld t2, 16(sp) \n\t"
		"ld t3, 24(sp) \n\t"
		/* restore context from stack */
		"ld ra, 32(sp) \n\t"
		"ld a1, 40(sp) \n\t"
		"ld a2, 48(sp) \n\t"
		"ld a3, 56(sp) \n\t"
		"ld a4, 64(sp) \n\t"
		"ld a5, 72(sp) \n\t"
		"ld a6, 80(sp) \n\t"
		"ld a7, 88(sp) \n\t"
		"ld t4, 96(sp) \n\t"
		"ld t5, 104(sp) \n\t"
		"ld t6, 112(sp) \n\t"
		"fld fa0, 120(sp) \n\t"
		"fld fa1, 128(sp) \n\t"
		"fld fa2, 136(sp) \n\t"
		"fld fa3, 144(sp) \n\t"
		"fld fa4, 152(sp) \n\t"
		"fld fa5, 160(sp) \n\t"
		"fld fa6, 168(sp) \n\t"
		"fld fa7, 176(sp) \n\t"
		"fld ft0, 184(sp) \n\t"
		"fld ft1, 192(sp) \n\t"
		"fld ft2, 200(sp) \n\t"
		"fld ft3, 208(sp) \n\t"
		"fld ft4, 216(sp) \n\t"
		"fld ft5, 224(sp) \n\t"
		"fld ft6, 232(sp) \n\t"
		"fld ft7, 240(sp) \n\t"
		"fld ft8, 248(sp) \n\t"
		"fld ft9, 256(sp) \n\t"
		"fld ft10, 264(sp) \n\t"
		"fld ft11, 272(sp) \n\t"

        "beqz t0, return_prelude \n\t"
        "srli t0, t0, 1 \n\t"
        "beqz t0, handled_ecall \n\t"
        "srli t0, t0, 1 \n\t"
        "beqz t0, do_post_clone \n\t"
        "ebreak \n\t" // t0 should be 0, 1 or 2; any other value means error

		/*
		 * t0 will be restored in the return path encoded by setup_return() in
		 * patcher.c. gp will be automatically restored to
		 * relocated_global_pointer as the final jalr is placed exactly 4 bytes
		 * before of it.
		 * We need three separate return path as we need to actually issue the
		 * system call if the hook did not handle itself or avoid it if the hook
		 * already did it. Moreover, clone and fork system call need a full
		 * context restore before the execution, otherwise the child thread or
		 * process would start its execution with a compromised context.
		 * You can see the return paths layout in patcher.c:107-122.
		 */
        "handled_ecall: \n\t"
        "la t0, relocated_global_pointer \n\t"
        "ld t0, 0(t0) \n\t"
        "addi t0, t0, -56 \n\t"
        "jalr zero, t0, 0 \n\t"

		"return_prelude: \n\t"
        "li t0, 220 \n\t" // __NR_clone
        "beq a7, t0, do_clone \n\t"
        "li t0, 435 \n\t" // __NR_clone3
        "beq a7, t0, do_clone \n\t"
        "la t0, relocated_global_pointer \n\t"
        "ld t0, 0(t0) \n\t"
        "addi t0, t0, -40 \n\t"
        "jalr zero, t0, 0 \n\t"

        "do_clone: \n\t"
        "la t0, relocated_global_pointer \n\t"
        "ld t0, 0(t0) \n\t"
        "addi t0, t0, -20 \n\t"
        "jalr zero, t0, 0 \n\t"

        "do_post_clone: \n\t"
        "ld gp, 288(sp) \n\t"
        "ld t0, 0(sp) \n\t"
        "addi sp, sp, 304 \n\t"
        "ecall \n\t"
        "j post_clone_path \n\t"

		/*
		 * We do not want to intercept rt_sigreturn, then we just need to let
		 * the kernel normally handle this system call. However, we still need
		 * to restore t0-t3, sp, gp and jump at the correct returning path.
		 * t1-t3 store instructions are encoded in patcher.c:41-43.
		 */
		"do_rt_sigreturn: \n\t"
		"ld t1, 8(sp) \n\t"
		"ld t2, 16(sp) \n\t"
		"ld t3, 24(sp) \n\t"
        "j return_prelude \n\t"
	);

}

typedef int (*hook_fn_t)(long syscall_numer, long a0, long a1, long a2, long a3,
    long a4, long a5, long *result);
typedef void (*post_clone_hook_fn_child_t)(void);
typedef void (*post_clone_hook_fn_parent_t)(long a0);

static hook_fn_t hook_fn = NULL;
static post_clone_hook_fn_child_t post_clone_hook_fn_child = NULL;
static post_clone_hook_fn_parent_t post_clone_hook_fn_parent = NULL;

struct wrapper_ret post_clone_hook(int64_t a0)
{
    if (a0 == 0) {
        if (post_clone_hook_fn_child != NULL)
            post_clone_hook_fn_child();
    } else {
    	while (atomic_flag_test_and_set_explicit(&tid_list_lock, memory_order_acquire));
    	for (int i = 0; i < MAX_THREADS; i++) {
    		if (active_tids[i] == 0) {
    			active_tids[i] = (pid_t)a0;
    			break;
    		}
    	}
    	atomic_flag_clear_explicit(&tid_list_lock, memory_order_release);
        if (post_clone_hook_fn_parent != NULL)
            post_clone_hook_fn_parent(a0);
    }
    return (struct wrapper_ret) { .a[0] = a0, .a[1] = 1 };
}

struct wrapper_ret syscall_hook(int64_t a0, int64_t a1,
		  int64_t a2, int64_t a3,
		  int64_t a4, int64_t a5,
		  int64_t a6_ra, // a6 contains return address to jump after patched ecall
		  int64_t a7)
{

    /*
     * If the targeted executable is trying to register a custom SIGSEGV
     * handler which would replace the VPOLINE handler, then we store the
     * pointer to the user-defined handler aside and we trick the executable
     * into thinking that the registration was successful. In this way, we can
     * keep the gp-caused fault handling active and we can still forward the
     * signal to the user-defined handler whenever gp is not the cause.
     */
    if (a7 == SYS_rt_sigaction && a0 == SIGSEGV) {
        const struct sigaction *act = (const struct sigaction *)a1;
        struct sigaction *oldact = (struct sigaction *)a2;

        if (oldact != NULL) {
            if (user_sigsegv_registered) {
                *oldact = user_sigsegv_act;
            } else {
                memset(oldact, 0, sizeof(*oldact));
                oldact->sa_handler = SIG_DFL;
            }
        }

        if (act != NULL) {
            user_sigsegv_act = *act;
            user_sigsegv_registered = true;
        }

        return (struct wrapper_ret) { .a[0] = 0, .a[1] = 1 };
    }

    long result;
    if (hook_fn == NULL) {
        return (struct wrapper_ret) {
            .a[0] = a0, .a[1] = 0
        };
    }
    const bool forward_to_kernel = hook_fn(a7, a0, a1, a2, a3, a4, a5, &result);
    if (forward_to_kernel) {

        if (a7 == SYS_clone && a1 != 0) {
            return (struct wrapper_ret) { .a[0] = a0, .a[1] = 2 };
        }
#ifdef SYS_clone3
        else if (a7 == SYS_clone3 && ((struct clone_args *)a0)->stack != 0) {
            return (struct wrapper_ret) { .a[0] = a0, .a[1] = 2 };
        }
#endif
        return (struct wrapper_ret) { .a[0] = a0, .a[1] = 0 };
    } else {
        return (struct wrapper_ret) { .a[0] = result, .a[1] = 1 };
    }
}

struct disassembly_state {
	char *code;
	size_t off;
};

struct intercept_disasm_context {
	csh handle;
	cs_insn *insn;
	const unsigned char *begin;
	const unsigned char *end;
};

struct intercept_disasm_result {
	const unsigned char *address;

	bool is_syscall;

	/* Length in bytes, zero if disasm was not successful. */
	unsigned length;

#ifndef NDEBUG
	const char *mnemonic;
#endif
};

void
intercept_disasm_destroy(struct intercept_disasm_context *context)
{
	cs_free(context->insn, 1);
	cs_close(&context->handle);
	munmap(context,sizeof(*context));
}

static int
nop_vsnprintf()
{
	return 0;
}

struct intercept_disasm_context *
intercept_disasm_init(const unsigned char *begin, const unsigned char *end)
{
	struct intercept_disasm_context *context;

	context = mmap(NULL,sizeof(*context), PROT_READ | PROT_WRITE, MAP_PRIVATE |	MAP_ANON, -1, (off_t)0);
	context->begin = begin;
	context->end = end;

	/*
	 * Initialize the disassembler.
	 * The handle here must be passed to capstone each time it is used.
	 */
	if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV64 | CS_MODE_RISCVC, &context->handle) != CS_ERR_OK) {
		fprintf(stderr, "cs_open failed in intercept_disasm_init\n");
		exit(EXIT_FAILURE);
    }

	/*
	 * Kindly ask capstone to return some details about the instruction.
	 * Without this, it only prints the instruction, and we would need
	 * to parse the resulting string.
	 */
	if (cs_option(context->handle, CS_OPT_DETAIL, CS_OPT_ON) != 0) {
		fprintf(stderr, "cs_option failed in intercept_disasm_init\n");
		exit(EXIT_FAILURE);
    }

	/*
	 * Overriding the printing routine used by capstone,
	 * see comments above about nop_vsnprintf.
	 */
	cs_opt_mem x = {
		.malloc = malloc,
		.free = free,
		.calloc = calloc,
		.realloc = realloc,
		.vsnprintf = nop_vsnprintf};
	if (cs_option(context->handle, CS_OPT_MEM, (size_t)&x) != 0) {
		fprintf(stderr, "cs_option failed in intercept_disasm_init\n");
		exit(EXIT_FAILURE);
    }

	if ((context->insn = cs_malloc(context->handle)) == NULL) {
		fprintf(stderr, "cs_malloc failed in intercept_disasm_init\n");
		exit(EXIT_FAILURE);
    }

	return context;
}

struct intercept_disasm_result
intercept_disasm_next_instruction(struct intercept_disasm_context *context,
					const unsigned char *code) {
	struct intercept_disasm_result result = {.address = code, 0, };
	const unsigned char *start = code;
	size_t size = (size_t)(context->end - code + 1);
	uint64_t address = (uint64_t)code;

	if (!cs_disasm_iter(context->handle, &start, &size,
		&address, context->insn)) {
		return result;
	}

	result.length = context->insn->size;

    if (result.length == 0) {
        fprintf(stderr, "cs_disasm_iter failed in intercept_disasm_next_instruction\n");
        exit(EXIT_FAILURE);
    }

	result.is_syscall = (context->insn->id == RISCV_INS_ECALL);
#ifndef NDEBUG
	result.mnemonic = context->insn->mnemonic;
#endif

	return result;
}

/**
 * Use capstone to detect all ECALL occurrences in the referenced memory section
 * and overwrite them with JALR GP, GP, A0
 *
 */
static void disassemble_and_rewrite(char *code, size_t code_size, int mem_prot)
{
#ifdef DEBUG
	write(1,"Disassembling the previous mapping...\n",38);
#endif
	/* add PROT_WRITE to rewrite the code */
    if (mprotect(code, code_size, PROT_WRITE | PROT_READ | PROT_EXEC)) {
        fprintf(stderr, "mprotect failed in disassemble_and_rewrite (before patching): %s\n",
            strerror(errno));
        exit(EXIT_FAILURE);
    }

	char *end = code + code_size - 1;
	char *code_cpy = code;
	struct intercept_disasm_context *context =
		intercept_disasm_init((const unsigned char *)code, (const unsigned char *)end);
	while (code <= end) {
		struct intercept_disasm_result result;

		result = intercept_disasm_next_instruction(context, (const unsigned char *)code);

		if (result.length == 0) {
			++code;
			continue;
		}

		if (result.is_syscall) {
			*(uint32_t *)(result.address) = 0x000181e7; // jalr gp, gp, 0
		}
#ifdef SUPPLEMENTAL__REWRITTEN_ADDR_CHECK
		record_replaced_instruction_addr((uintptr_t) result.address);
#endif
		code += result.length;
	}

	intercept_disasm_destroy(context);
    if (mprotect(code_cpy, code_size, mem_prot)) {
        fprintf(stderr, "mprotect failed in disassemble_and_rewrite (after patching): %s\n",
            strerror(errno));
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
	write(1,"Successfully patched the previous mapping\n\n",43);
#endif
}

/**
 * We browse the process memory mappings to detect the executable section of
 * glibc, then we forward the memory addresses to capstone so that the binary
 * can be disassembled and analyzed through capstone API
 */
static void rewrite_code(void)
{
#ifdef DEBUG
	write(1,"Opening /proc/self/maps - OK\n\n",30);
#endif
	FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open /proc/self/maps: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
	/* get memory mapping information from procfs */
	{
		char buf[4096];
		while (fgets(buf, sizeof(buf), fp) != NULL) {
#ifdef DEBUG
			write(1,buf,strlen(buf));
#endif
			/* we do not touch stack and vsyscall memory */
			if (strstr(buf, "libc.so.6") != NULL) {
				int i = 0;
				char addr[65] = { 0 };
				char *c = strtok(buf, " ");
				while (c != NULL) {
					switch (i) {
					case 0:
						strncpy(addr, c, sizeof(addr) - 1);
						break;
					case 1:
						{
							int mem_prot = 0;
							{
								size_t j;
								for (j = 0; j < strlen(c); j++) {
									if (c[j] == 'r')
										mem_prot |= PROT_READ;
									if (c[j] == 'w')
										mem_prot |= PROT_WRITE;
									if (c[j] == 'x')
										mem_prot |= PROT_EXEC;
								}
							}
							/* rewrite code if the memory is executable */
							if (mem_prot & PROT_EXEC) {
								size_t k;
								for (k = 0; k < strlen(addr); k++) {
									if (addr[k] == '-') {
										addr[k] = '\0';
										break;
									}
								}
								{
									int64_t from, to;
									from = strtol(&addr[0], NULL, 16);
									if (from == 0) {
										/*
										 * this is trampoline code.
										 * so skip it.
										 */
										break;
									}
									to = strtol(&addr[k + 1], NULL, 16);
									disassemble_and_rewrite((char *) from,
											(size_t) to - from,
											mem_prot);
								}
							}
						}
						break;
					}
					if (i == 1)
						break;
					c = strtok(NULL, " ");
					i++;
				}
			}
		}
	}
	fclose(fp);
#ifdef DEBUG
	write(1, "\nFinished replacing ECALL occurrences with JALR GP, A7, 0\n", 58);
#endif
}

/**
 * Here we basically check the hook library specified in the LIBVPHOOK env
 * variable, then we call its __hook_init to assign the actual hook function to
 * the hook_fn function pointer, which will be used sy syscall_hook everytime
 * a system call gets intercepted
 */
static void load_hook_lib(void)
{
	void *handle;
	{
		const char *filename;
		filename = getenv("LIBVPHOOK");
		if (!filename) {
			fprintf(stderr, "env LIBVPHOOK is empty, so skip to load a hook library\n");
			return;
		}

		handle = dlmopen(LM_ID_NEWLM, filename, RTLD_NOW | RTLD_LOCAL);
		if (!handle) {
			fprintf(stderr, "dlmopen failed: %s\n\n", dlerror());
			fprintf(stderr, "NOTE: this may occur when the compilation of your hook function library misses some specifications in LDFLAGS. or if you are using a C++ compiler, dlmopen may fail to find a symbol, and adding 'extern \"C\"' to the definition may resolve the issue.\n");
			exit(1);
		}
	}
	{
		int (*hook_init)(long, void *, void **, void **, void **);
		hook_init = dlsym(handle, "__hook_init");
		if (hook_init == NULL) {
			fprintf(stderr, "dlsym failed: %s\n\n", dlerror());
			exit(EXIT_FAILURE);
		}

	    void *user_hook_ptr = NULL;
	    void *user_post_clone_hook_child_ptr = NULL;
        void *user_post_clone_hook_parent_ptr = NULL;
		int init_state = -1;
		init_state = hook_init(0, (void *)syscall_no_intercept, &user_hook_ptr, &user_post_clone_hook_child_ptr, &user_post_clone_hook_parent_ptr);
		if (init_state != 0) {
			fprintf(stderr, "Error: hook_init failed initialization: return value = %d", init_state);
			exit(EXIT_FAILURE);
		}
	    hook_fn = (hook_fn_t) user_hook_ptr;
	    post_clone_hook_fn_child = (post_clone_hook_fn_child_t) user_post_clone_hook_child_ptr;
        post_clone_hook_fn_parent = (post_clone_hook_fn_parent_t) user_post_clone_hook_parent_ptr;
	}
}

__attribute__((constructor(0xffff))) static void __vpoline_init(void)
{
    /*
     * Here GP register has already been set to __global_pointer$ which is
     * .data + 0x800. __global_pointer$ is a symbol whose scope is limited to
     * the main executable, then it must be saved it in a global variable which
     * will be a __global_pointer$ symbol surrogate to use after the
     * interception in the context of this shared library.
     */
    asm volatile
    (
        "mv %0, gp\n\t"
        : "=r"(virtual_global_pointer)
    );

#ifdef DEBUG
    printf("virtual_global_pointer: %lx\n", virtual_global_pointer);
#endif


    /*
     * The following code is here for debug purposes only. Running cmake with
     * -DCMAKE_BUILD_TYPE=Debug will define DEBUG macro and enable this check.
     * The purpose is to make sure the zpoline initialization is actually
     * performed only before the execution of the target executable and not
     * before an intermediate program so that unwanted and disruptive behaviour
     * is avoided when running under an intermediate program, i.e. gdb.
     */
#ifdef DEBUG
    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) return; /* cannot determine executable -> be conservative */
    path[n] = '\0';
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    /*
     * VPOLINE_TARGET can be used to explicitly specify the target executable
     * otherwise the execution command will be used to determine it.
     */
    const char *expected = getenv("VPOLINE_TARGET");
    char expected_buf[PATH_MAX];
    if (!expected) {
        FILE *f = fopen("/proc/self/cmdline", "r");
        if (f) {
            size_t r = fread(expected_buf, 1, sizeof(expected_buf) - 1, f);
            fclose(f);
            if (r > 0) {
                expected_buf[r] = '\0';
                /* cmdline entries are NUL-separated; first entry is argv[0] */
                const char *first = expected_buf;
                const char *b = strrchr(first, '/');
                expected = b ? b + 1 : first;
            }
        }
    }
    /*
     * If expected is still NULL, then use target the default executable used
     * for testing and debugging. Will probably change in the future.
     */
    if (!expected) expected = "dummy_exec";

    if (strcmp(base, expected) != 0) {
        /* running inside an intermediate program (e.g. bash) -> skip heavy init */
        return;
    }
#endif

    define_ret_sequence_info();

	check_ziccif_support();

	/*
	 * we register the current thread ID as the first one of the possible
	 * concurrent threads which will execute in the same memory space
	 */
	memset(active_tids, 0, sizeof(active_tids));
	active_tids[0] = (pid_t)syscall_no_intercept(SYS_gettid);

    init_trap_handler();

    allocate_ret_sequence_page();

#ifdef DEBUG
    printf("Allocated shadow page\n");
#endif

    setup_shadow_page_trampoline();

#ifdef DEBUG
    printf("Wrote trampoline to asm_syscall_hook at relocated_gp\n");
#endif

    setup_return();

    __asm__ volatile
    (
        "mv gp, %0 \n\t"
        :
        : "r"(rsi.relocated_gp)
    );

#ifdef DEBUG
    printf("Return sequence wrote before relocated_gp\n");
#endif

    rewrite_code();

#ifdef DEBUG
	printf("glibc executable section has been patched\n"
	       "each ECALL occurrence has been replaced with JALR GP, A7, 0\n\n");
#endif

    load_hook_lib();
}

int __libc_start_main(int (*main)(int, char **, char **),
                      int argc, char **argv,
                      void (*init)(void), void (*fini)(void),
                      void (*rtld_fini)(void), void *stack_end)
{
    /* save the true __libc_start_main function pointer */
    libc_start_main_t orig_libc_start_main = (libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");

    /*
     * we store in the gp register the relocated_gp value so that any
     * gp-relative access will be able to issue a segfault which will be
     * handled by redirecting the access to the main elf .data page
     */
    __asm__ volatile
    (
        "mv gp, %0 \n\t"
        :
        : "r"(rsi.relocated_gp)
    );

    /*
     * call true __libc_start_marin which will call the main of the target
     * executable without reloading gp with the proper value
     */
    return orig_libc_start_main(main, argc, argv, init, fini, rtld_fini, stack_end);
}

