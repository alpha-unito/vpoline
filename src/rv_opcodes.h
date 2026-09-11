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

#ifndef RISCVPOLINE_RV_OPCODES_H
#define RISCVPOLINE_RV_OPCODES_H

// TODO: add A extension instructions opcodes and funct3 fields

/*
 * Here opcode e funct3 fields for each existing RISC-V instruction accessing
 * memory both for reading and writing are listed accordingly to the RISC-V
 * Unprivileged ISA Specification
 */

#define LOAD_OPCODE     0x03

#define LB_FUNCT3   0x0
#define LH_FUNCT3   0x1
#define LW_FUNCT3   0x2
#define LD_FUNCT3   0x3
#define LBU_FUNCT3  0x4
#define LHU_FUNCT3  0x5
#define LWU_FUNCT3  0x6


#define FLOAD_OPCODE    0x07

#define FLW_FUNCT3  0x2
#define FLD_FUNCT3  0x3


#define STORE_OPCODE    0x23

#define SB_FUNCT3   0x0
#define SH_FUNCT3   0x1
#define SW_FUNCT3   0x2
#define SD_FUNCT3   0x3


#define FSTORE_OPCODE   0x27

#define FSW_FUNCT3  0x2
#define FSD_FUNCT3  0x3


#define C0_OPCODE   0x00

#define C_FLD_FUNCT3    0x1
#define C_LW_FUNCT3     0x2
#define C_LD_FUNCT3     0x3
#define C_FSD_FUNCT3    0x5
#define C_SW_FUNCT3     0x6
#define C_SD_FUNCT3     0x7


#define C2_OPCODE   0x02

#define C_FLDSP_FUNCT3  0x1
#define C_LWSP_FUNCT3   0x2
#define C_LDSP_FUNCT3   0x3
#define C_FSDSP_FUNCT3  0x5
#define C_SWSP_FUNCT3   0x6
#define C_SDSP_FUNCT3   0x7


#define AMO_OPCODE  0x2f

#define AMOADD_FUNCT5   0x00
#define AMOSWAP_FUNCT5  0x01
#define AMOLR_FUNCT5    0x02
#define AMOSCR_FUNCT5   0x03
#define AMOXOR_FUNCT5   0x04
#define AMOOR_FUNCT5    0x0a
#define AMOAND_FUNCT5   0x0c
#define AMOMIN_FUNCT5   0x10
#define AMOMAX_FUNCT5   0x14
#define AMOMINU_FUNCT5  0x18
#define AMOMAXU_FUNCT5  0x1c

/*
 * Non SP-relative compressed instructions (0x00 opcode) encode registers in 3
 * bits, shifting the referrable set to the "popular" x8-x15 instead of x0-x7.
 */
static const uint8_t rvc_reg_map[] = {
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15
};

#endif //RISCVPOLINE_RV_OPCODES_H