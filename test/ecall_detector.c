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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <capstone/capstone.h>

static int nop_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    (void)str;
    (void)size;
    (void)format;
    (void)ap;
    return 0;
}

static size_t count_ecalls_in_mapping(const unsigned char *code, size_t code_size)
{
    csh handle;
    cs_insn *insn;
    size_t ecall_count = 0;

    if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV64 | CS_MODE_RISCVC, &handle) != CS_ERR_OK) {
        fprintf(stderr, "cs_open failed in count_ecalls_in_mapping\n");
        exit(EXIT_FAILURE);
    }

    if (cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON) != CS_ERR_OK) {
        fprintf(stderr, "cs_option(CS_OPT_DETAIL) failed\n");
        exit(EXIT_FAILURE);
    }

    cs_opt_mem mem_opt = {
        .malloc = malloc,
        .free = free,
        .calloc = calloc,
        .realloc = realloc,
        .vsnprintf = nop_vsnprintf
    };
    if (cs_option(handle, CS_OPT_MEM, (size_t)&mem_opt) != CS_ERR_OK) {
        fprintf(stderr, "cs_option(CS_OPT_MEM) failed\n");
        exit(EXIT_FAILURE);
    }

    insn = cs_malloc(handle);
    if (insn == NULL) {
        fprintf(stderr, "cs_malloc failed\n");
        exit(EXIT_FAILURE);
    }

    const unsigned char *end = code + code_size - 1;
    const unsigned char *cur = code;

    while (cur <= end) {
        const unsigned char *start = cur;
        size_t size = (size_t)(end - cur + 1);
        uint64_t address = (uint64_t)cur;

        if (!cs_disasm_iter(handle, &start, &size, &address, insn)) {
            ++cur;
            continue;
        }

        if (insn->size == 0) {
            fprintf(stderr, "cs_disasm_iter returned instruction of size 0\n");
            exit(EXIT_FAILURE);
        }

        if (insn->id == RISCV_INS_ECALL) {
            fprintf(stderr, "[!] Unpatched ECALL found at %p\n", (const void *)cur);
            ecall_count++;
        }

        cur += insn->size;
    }

    cs_free(insn, 1);
    cs_close(&handle);

    return ecall_count;
}

int main(void)
{
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open /proc/self/maps: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    size_t total_ecalls = 0;
    size_t scanned_mappings = 0;
    char buf[4096];

    while (fgets(buf, sizeof(buf), fp) != NULL) {
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
                        for (size_t j = 0; j < strlen(c); j++) {
                            if (c[j] == 'r')
                                mem_prot |= PROT_READ;
                            if (c[j] == 'w')
                                mem_prot |= PROT_WRITE;
                            if (c[j] == 'x')
                                mem_prot |= PROT_EXEC;
                        }

                        if (mem_prot & PROT_EXEC) {
                            size_t k;
                            for (k = 0; k < strlen(addr); k++) {
                                if (addr[k] == '-') {
                                    addr[k] = '\0';
                                    break;
                                }
                            }

                            int64_t from = strtol(&addr[0], NULL, 16);
                            if (from == 0) {
                                break;
                            }
                            int64_t to = strtol(&addr[k + 1], NULL, 16);

                            total_ecalls += count_ecalls_in_mapping(
                                (const unsigned char *)from,
                                (size_t)(to - from)
                            );
                            scanned_mappings++;
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

    fclose(fp);

    if (scanned_mappings == 0) {
        fprintf(stderr, "Error: no executable mappings found for libc.so.6\n");
        return EXIT_FAILURE;
    }

    if (total_ecalls != 0) {
        fprintf(stderr, "Test FAILED: found %zu unpatched ECALL instruction(s) in libc.so.6\n", total_ecalls);
        return EXIT_FAILURE;
    }

    printf("Test PASSED: 0 ECALL instructions found across %zu executable mapping(s) of libc.so.6\n", scanned_mappings);
    return EXIT_SUCCESS;
}