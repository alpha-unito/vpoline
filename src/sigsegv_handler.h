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

#ifndef RISCVPOLINE_SIGSEGV_HANDLER_H
#define RISCVPOLINE_SIGSEGV_HANDLER_H

#include <bits/types/siginfo_t.h>

#include "main.h"

extern ReturnSequenceInfo rsi;

// void segfault_handler(int sig, siginfo_t *si, void *context);

/*
 * the layout of this struct is dependent on which order registers get saved
 * on the stack in patcher.c:104-169
 */
struct context {
    uint64_t ra;
    uint64_t gp;
    uint64_t tp;
    uint64_t t[7];
    uint64_t a[8];
    uint64_t s[12];
    uint64_t ft[12];
    uint64_t fs[12];
    uint64_t fa[8];
};

typedef struct {
    uintptr_t address;
    uint32_t encoding;
} gp_instr_t;

#define INITIAL_CAPACITY 64

/**
 * Sets up segfault_handler() as the handling function for SIGSEGV signals. In
 * other words, segfault_handler() will be executed at each segmentation fault.
 */
void init_trap_handler(void);

#endif //RISCVPOLINE_SIGSEGV_HANDLER_H