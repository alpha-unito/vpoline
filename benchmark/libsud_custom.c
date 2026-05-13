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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <sys/prctl.h>

#if defined(__x86_64__) || defined(__riscv)
void* (syscall_dispatcher_start)(void);
void* (syscall_dispatcher_end)(void);
#else
unsigned long syscall_dispatcher_start = 0;
unsigned long syscall_dispatcher_end = 0;
#endif

char selector;

void intercept_hook_point (int syscall_number,
                      long arg0, long arg1,
                      long arg2, long arg3,
                      long arg4, long arg5,
                      long *result) {
    register long r_a7 asm("a7") = syscall_number;
    register long r_a0 asm("a0") = arg0;
    register long r_a1 asm("a1") = arg1;
    register long r_a2 asm("a2") = arg2;
    register long r_a3 asm("a3") = arg3;
    register long r_a4 asm("a4") = arg4;
    register long r_a5 asm("a5") = arg5;

    asm volatile (
        "ecall"
        : "+r" (r_a0)
        : "r" (r_a1), "r" (r_a2), "r" (r_a3), "r" (r_a4), "r" (r_a5), "r" (r_a7)
        : "memory"
    );
    *result = r_a0;
}

static void handle_sigsys(int sig, siginfo_t *info, void *ucontext) {
	selector = SYSCALL_DISPATCH_FILTER_ALLOW;
	mcontext_t *context = &((ucontext_t *) ucontext)->uc_mcontext;
    long result;
#if defined(__x86_64__)
	intercept_hook_point(
            info->si_syscall, context->gregs[8],
            context->gregs[9], context->gregs[12],
            context->gregs[2], context->gregs[0],
            context->gregs[1], &result);
    context->gregs[13] = result;
#elif defined(__riscv)
	intercept_hook_point(
			info->si_syscall, context->__gregs[10],
			context->__gregs[11], context->__gregs[12],
			context->__gregs[13], context->__gregs[14],
			context->__gregs[15], &result);
	context->__gregs[10] = result;
#endif
	selector = SYSCALL_DISPATCH_FILTER_BLOCK;

#if defined(__x86_64__)
	__asm__ volatile("movq %0, %%rax" : : "I" (SYS_rt_sigreturn));
	__asm__ volatile("leaveq");
	__asm__ volatile("add $0x8, %rsp");
	__asm__ volatile("syscall_dispatcher_start:");
	__asm__ volatile("syscall");
	__asm__ volatile("nop");
	__asm__ volatile("syscall_dispatcher_end:");
#elif defined(__riscv)
	__asm__ volatile("li a7, %0" : : "I" (SYS_rt_sigreturn));
	__asm__ volatile("ld ra, 72(sp)");
	__asm__ volatile("ld s0, 64(sp)");
	__asm__ volatile("addi sp, sp, 80");
	__asm__ volatile("syscall_dispatcher_start:");
	__asm__ volatile("ecall");
	__asm__ volatile("syscall_dispatcher_end:");
#endif
}

static __attribute__((constructor)) void enable_sud(void) {
	struct sigaction act;
	sigset_t mask;

	memset(&act, 0, sizeof(struct sigaction));
	sigemptyset(&mask);

	act.sa_sigaction = handle_sigsys;
	act.sa_flags = SA_SIGINFO;
	act.sa_mask = mask;

	if (sigaction(SIGSYS, &act, NULL)) {
		perror("Error sigaction:");
		exit(EXIT_FAILURE);
	}

	if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
			syscall_dispatcher_start, syscall_dispatcher_end, &selector)) {
		perror("prctl failed");
		exit(EXIT_FAILURE);
	}

	selector = SYSCALL_DISPATCH_FILTER_BLOCK;
}
