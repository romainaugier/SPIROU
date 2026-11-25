/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/function.h"

#define SPIROU_OP_INFO(name, mnemonic, ins, outs, flags, payload) \
    { mnemonic, (flags), (uint16_t)sizeof(payload), (ins), (outs) },

const SpirouOpInfo spirou_op_infos[SpirouOp_Count] = {
    SPIROU_OPS(SPIROU_OP_INFO)
};

#define SPIROU_CMP_MNEMONIC(name, mnemonic) mnemonic,

const char* spirou_cmp_mnemonics[SpirouCmp_Count] = {
    SPIROU_CMPS(SPIROU_CMP_MNEMONIC)
};
