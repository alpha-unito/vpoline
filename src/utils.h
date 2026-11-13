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

#ifndef RISCVPOLINE_UTILS_H
#define RISCVPOLINE_UTILS_H

#include "main.h"

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <signal.h>
#include <stdbool.h>


extern uintptr_t virtual_global_pointer;
extern uintptr_t relocated_global_pointer;
extern size_t page_size;
extern ReturnSequenceInfo rsi;

/**
 * Defines all fields of the global structure holding useful information about
 * addresses and memory pages involved in the writing of the return sequence
 * prior to __global_pointer$.
 */
void define_ret_sequence_info(void);

/**
 * Allocates the memory page which will contain the return sequence and will
 * be pointed to by relocated_gp. This page will be set as eXecute-Only-Memory
 * (XOM) once the return sequence is written in it.
 */
void allocate_ret_sequence_page(void);

#ifndef SYS_riscv_hwprobe
#define SYS_riscv_hwprobe 258
#endif

#define RISCV_HWPROBE_KEY_IMA_EXT_0 4
#define RISCV_HWPROBE_KEY_IMA_EXT_1 16
#define RISCV_HWPROBE_IMA_ZICCIF (1ULL << 3)

struct riscv_hwprobe {
    int64_t key;
    uint64_t value;
};

extern bool ziccif_supported;
/**
 * Sets ziccif_supported to true is the CPU supports such ISA extension. This
 * is needed to know if the CPU guarantees atomic fetching of naturally aligned
 * instructions.
 */
void check_ziccif_support(void);

#endif // RISCVPOLINE_UTILS_H
