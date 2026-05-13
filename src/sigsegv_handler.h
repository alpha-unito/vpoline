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

/**
 * Sets up segfault_handler() as the handling function for SIGSEGV signals. In
 * other words, segfault_handler() will be executed at each segmentation fault.
 */
void init_trap_handler(void);

#endif //RISCVPOLINE_SIGSEGV_HANDLER_H