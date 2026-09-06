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

#ifndef RISCVPOLINE_MAIN_H
#define RISCVPOLINE_MAIN_H

#include <stdint.h>
#include <stddef.h>

#define RET_SEQUENCE_SIZE (56) // 14 4-bytes long instructions

#define MAX_THREADS 1024

typedef int (*libc_start_main_t)(int (*main)(int, char **, char **),
                                 int argc, char **argv,
                                 void (*init)(void), void (*fini)(void),
                                 void (*rtld_fini)(void), void *stack_end);

typedef struct {

    /* where the 3 instructions return sequence starts */
    void *start_addr;

    /* last byte of the 3 instructions return sequence */
    void* end_addr;

    /* address of the memory page containing start_addr */
    void *start_addr_page;

    /* address of the memory page containing end_addr */
    void *end_addr_page;

    /*
     * difference between gp original value and page start address of the
     * page where original gp points to
     */
    uint64_t gp_offset;

    /*
     * relocated_gp is the value which gp will hold during the main elf
     * execution. This allows to write the return sequence in the 12 bytes
     * preceding relocated_gp in a eXecute-Only-Memory (XOM) page allocated
     * just for this purpose. Since we're not writing the return sequence before
     * the original gp, the original page pointed by gp will not be patched and
     * will not issue any segfault. The drawback of this approach is that we
     * irrevocably change the value of gp, thus needing any relocated_gp-relative
     * memory access to cause a segfault so that we can handle it and redirect
     * the access to the original page. To be able to access the correct data
     * we need the offset between relocated_gp and its page start address to be
     * the same as the offset between original gp and its page start address
     */
    uintptr_t relocated_gp;

    /* start_addr - start_addr_page */
    uint32_t start_offset;

    /*
     * how much space will undergo the mprotect execution. Practically speaking
     * this is either one or two pages depending on start_addr and end_addr
     * being in the same page or not. Its value will be 4096 or 8192
     */
    size_t protection_size;

    /*
     * this will reference the memory area allocated to hold the copy of the
     * memory pages which will be partially overwritten by the return sequence
     * and which will be set as eXecute-Only-Memory (XOM). Each read or write
     * access to that area will issue a segmentation fault which will be
     * handled so that such accesses will be redirected here to the backup copy.
     */
    void *ret_sequence_page_addr;

} ReturnSequenceInfo;

struct wrapper_ret {
    long a[2];
};

#endif //RISCVPOLINE_MAIN_H