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

#include "utils.h"

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <sys/auxv.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <syscall.h>

extern long syscall_no_intercept(long, ...);

bool ziccif_supported = false;

/**
 * Defines all fields of the global structure holding useful information about
 * addresses and memory pages involved in the writing of the return sequence
 * prior to __global_pointer$.
 */
void define_ret_sequence_info()
{
    page_size = sysconf(_SC_PAGESIZE);
    const uintptr_t page_mask = ~(page_size - 1);

    rsi.gp_offset = virtual_global_pointer % page_size;

    rsi.start_addr = (void *)virtual_global_pointer - RET_SEQUENCE_SIZE;
    rsi.end_addr = (void *)virtual_global_pointer - 1;

    rsi.start_addr_page = (void *)((uintptr_t)rsi.start_addr & page_mask);
    rsi.end_addr_page = (void *)((uintptr_t)rsi.end_addr & page_mask);

    rsi.protection_size = rsi.end_addr_page - rsi.start_addr_page + page_size;

#ifdef DEBUG
    char buf[200];
    snprintf(buf, sizeof(buf),
        "Read-Write memory page pointed by original GP:\n"
        "0x%" PRIXPTR " - 0x%" PRIXPTR "\n",
        (uintptr_t)rsi.start_addr_page,
        (uintptr_t)rsi.start_addr_page + rsi.protection_size);
    write(2, buf, strlen(buf));
#endif
}

/**
 * Allocates the memory page which will contain the return sequence and will
 * be pointed to by relocated_gp. This page will be set as eXecute-Only-Memory
 * (XOM) once the return sequence is written in it.
 */
void allocate_ret_sequence_page()
{
    rsi.ret_sequence_page_addr = mmap(NULL, rsi.protection_size,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0);

    if (rsi.ret_sequence_page_addr == MAP_FAILED) {
        fprintf(stderr, "mmap failed in allocate_ret_sequence_page\n");
        exit(1);
    }

#ifdef DEBUG
    char buf[200];
    snprintf(buf, sizeof(buf),
        "Memory page allocated for return sequence:\n"
        "0x%" PRIXPTR " - 0x%" PRIXPTR "\n",
        (uintptr_t)rsi.ret_sequence_page_addr,
        (uintptr_t)rsi.ret_sequence_page_addr + rsi.protection_size);
    write(2, buf, strlen(buf));
#endif

    rsi.relocated_gp = (uintptr_t)rsi.ret_sequence_page_addr + rsi.gp_offset;
#ifdef DEBUG
    printf("surrogate_global_pointer: %lx\n", rsi.relocated_gp);
#endif

    relocated_global_pointer = rsi.relocated_gp;
    rsi.start_addr = (void *)rsi.relocated_gp - RET_SEQUENCE_SIZE;
    rsi.end_addr = (void *)rsi.relocated_gp - 1;
    rsi.start_offset = (uintptr_t)rsi.start_addr % page_size;
}

/**
 * Sets ziccif_supported to true is the CPU supports such ISA extension. This
 * is needed to know if the CPU guarantees atomic fetching of naturally aligned
 * instructions.
 */
void check_ziccif_support(void)
{
    struct riscv_hwprobe probe = { .key = RISCV_HWPROBE_KEY_IMA_EXT_1, .value = 0 };

    long ret = syscall_no_intercept(SYS_riscv_hwprobe, &probe, 1, 0, NULL, 0);

    if (ret == 0) {
        if (probe.value & RISCV_HWPROBE_IMA_ZICCIF) {
            ziccif_supported = true;
#ifdef DEBUG
            const char msg[] = "[vpoline] HWPROBE: Ziccif extension SUPPORTED by hardware.\n";
            syscall_no_intercept(SYS_write, 2, msg, sizeof(msg) - 1);
#endif
        } else {
#ifdef DEBUG
            const char msg[] = "[vpoline] HWPROBE: Ziccif extension NOT supported.\n";
            syscall_no_intercept(SYS_write, 2, msg, sizeof(msg) - 1);
#endif
        }
    } else {
#ifdef DEBUG
        char buf[120];
        int len = sprintf(buf, "[vpoline] HWPROBE: riscv_hwprobe syscall failed or missing (ret = %ld)\n", ret);
        syscall_no_intercept(SYS_write, 2, buf, len);
#endif
    }
}
