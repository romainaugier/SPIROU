/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_OP)
#define __SPIROU_OP

#include "spirou/common.h"

SPIROU_CPP_ENTER

typedef enum SpirouOpFlags
{
    SpirouOpFlags_Pure = 0x1,
    SpirouOpFlags_Commutative = 0x2,
    SpirouOpFlags_Structural = 0x4,
    SpirouOpFlags_Memory = 0x8,
    SpirouOpFlags_Effect = 0x10,
    SpirouOpFlags_MayTrap = 0x20,
    SpirouOpFlags_Binary = 0x40,
    SpirouOpFlags_Unary = 0x80,
    SpirouOpFlags_Cast = 0x100,
    SpirouOpFlags_IntOp = 0x200,
    SpirouOpFlags_FloatOp = 0x400,
} SpirouOpFlags;

#define SPIROU_VARIADIC 0xFF

#define SPIROU_OP_PURE_INT2   (SpirouOpFlags_Pure | SpirouOpFlags_Binary | SpirouOpFlags_IntOp)
#define SPIROU_OP_PURE_INT2C  (SPIROU_OP_PURE_INT2 | SpirouOpFlags_Commutative)
#define SPIROU_OP_TRAP_INT2   (SPIROU_OP_PURE_INT2 | SpirouOpFlags_MayTrap)
#define SPIROU_OP_PURE_INT1   (SpirouOpFlags_Pure | SpirouOpFlags_Unary | SpirouOpFlags_IntOp)
#define SPIROU_OP_PURE_FLT2   (SpirouOpFlags_Pure | SpirouOpFlags_Binary | SpirouOpFlags_FloatOp)
#define SPIROU_OP_PURE_FLT2C  (SPIROU_OP_PURE_FLT2 | SpirouOpFlags_Commutative)
#define SPIROU_OP_PURE_FLT1   (SpirouOpFlags_Pure | SpirouOpFlags_Unary | SpirouOpFlags_FloatOp)
#define SPIROU_OP_CAST        (SpirouOpFlags_Pure | SpirouOpFlags_Cast)

/*
 * X(name, mnemonic, num_inputs, num_outputs, flags, payload struct)
 * Inputs and outputs counts of SPIROU_VARIADIC are set by the builder
 */
#define SPIROU_OPS(X)                                                                         \
    X(Undef,      "undef",      0, 1, SpirouOpFlags_Pure, SpirouNode)                          \
    X(IConst,     "iconst",     0, 1, SpirouOpFlags_Pure, SpirouNodeIConst)                    \
    X(FConst,     "fconst",     0, 1, SpirouOpFlags_Pure, SpirouNodeFConst)                    \
    X(Add,        "add",        2, 1, SPIROU_OP_PURE_INT2C, SpirouNode)                        \
    X(Sub,        "sub",        2, 1, SPIROU_OP_PURE_INT2, SpirouNode)                         \
    X(Mul,        "mul",        2, 1, SPIROU_OP_PURE_INT2C, SpirouNode)                        \
    X(SDiv,       "sdiv",       2, 1, SPIROU_OP_TRAP_INT2, SpirouNode)                         \
    X(UDiv,       "udiv",       2, 1, SPIROU_OP_TRAP_INT2, SpirouNode)                         \
    X(SRem,       "srem",       2, 1, SPIROU_OP_TRAP_INT2, SpirouNode)                         \
    X(URem,       "urem",       2, 1, SPIROU_OP_TRAP_INT2, SpirouNode)                         \
    X(And,        "and",        2, 1, SPIROU_OP_PURE_INT2C, SpirouNode)                        \
    X(Or,         "or",         2, 1, SPIROU_OP_PURE_INT2C, SpirouNode)                        \
    X(Xor,        "xor",        2, 1, SPIROU_OP_PURE_INT2C, SpirouNode)                        \
    X(Shl,        "shl",        2, 1, SPIROU_OP_PURE_INT2, SpirouNode)                         \
    X(LShr,       "lshr",       2, 1, SPIROU_OP_PURE_INT2, SpirouNode)                         \
    X(AShr,       "ashr",       2, 1, SPIROU_OP_PURE_INT2, SpirouNode)                         \
    X(Neg,        "neg",        1, 1, SPIROU_OP_PURE_INT1, SpirouNode)                         \
    X(Not,        "not",        1, 1, SPIROU_OP_PURE_INT1, SpirouNode)                         \
    X(FAdd,       "fadd",       2, 1, SPIROU_OP_PURE_FLT2C, SpirouNode)                        \
    X(FSub,       "fsub",       2, 1, SPIROU_OP_PURE_FLT2, SpirouNode)                         \
    X(FMul,       "fmul",       2, 1, SPIROU_OP_PURE_FLT2C, SpirouNode)                        \
    X(FDiv,       "fdiv",       2, 1, SPIROU_OP_PURE_FLT2, SpirouNode)                         \
    X(FMin,       "fmin",       2, 1, SPIROU_OP_PURE_FLT2C, SpirouNode)                        \
    X(FMax,       "fmax",       2, 1, SPIROU_OP_PURE_FLT2C, SpirouNode)                        \
    X(FNeg,       "fneg",       1, 1, SPIROU_OP_PURE_FLT1, SpirouNode)                         \
    X(FAbs,       "fabs",       1, 1, SPIROU_OP_PURE_FLT1, SpirouNode)                         \
    X(FSqrt,      "fsqrt",      1, 1, SPIROU_OP_PURE_FLT1, SpirouNode)                         \
    X(FMA,        "fma",        3, 1, SpirouOpFlags_Pure | SpirouOpFlags_FloatOp, SpirouNode)  \
    X(ICmp,       "icmp",       2, 1, SpirouOpFlags_Pure | SpirouOpFlags_IntOp, SpirouNodeCmp) \
    X(FCmp,       "fcmp",       2, 1, SpirouOpFlags_Pure | SpirouOpFlags_FloatOp, SpirouNodeCmp) \
    X(Select,     "select",     3, 1, SpirouOpFlags_Pure, SpirouNode)                          \
    X(ZExt,       "zext",       1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(SExt,       "sext",       1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(Trunc,      "trunc",      1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(FPExt,      "fpext",      1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(FPTrunc,    "fptrunc",    1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(SIToFP,     "sitofp",     1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(UIToFP,     "uitofp",     1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(FPToSI,     "fptosi",     1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(FPToUI,     "fptoui",     1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(Bitcast,    "bitcast",    1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(PtrToInt,   "ptrtoint",   1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(IntToPtr,   "inttoptr",   1, 1, SPIROU_OP_CAST, SpirouNode)                              \
    X(PtrAdd,     "ptradd",     2, 1, SpirouOpFlags_Pure, SpirouNode)                          \
    X(SymbolAddr, "addr",       0, 1, SpirouOpFlags_Pure, SpirouNodeSymbolAddr)                \
    X(StackSlot,  "stackslot",  0, 1, SpirouOpFlags_Effect, SpirouNodeStackSlot)               \
    X(Load,       "load",       2, 2, SpirouOpFlags_Memory, SpirouNodeMemory)                  \
    X(Store,      "store",      3, 1, SpirouOpFlags_Memory, SpirouNodeMemory)                  \
    X(Call,       "call",       SPIROU_VARIADIC, SPIROU_VARIADIC, SpirouOpFlags_Effect, SpirouNodeCall) \
    X(Gamma,      "gamma",      SPIROU_VARIADIC, SPIROU_VARIADIC, SpirouOpFlags_Structural, SpirouNodeGamma) \
    X(Theta,      "theta",      SPIROU_VARIADIC, SPIROU_VARIADIC, SpirouOpFlags_Structural, SpirouNodeTheta) \
    X(Map,        "map",        SPIROU_VARIADIC, SPIROU_VARIADIC, SpirouOpFlags_Structural, SpirouNodeMap)

#define SPIROU_OP_ENUM(name, mnemonic, ins, outs, flags, payload) SpirouOp_##name,

typedef enum SpirouOp
{
    SPIROU_OPS(SPIROU_OP_ENUM)
    SpirouOp_Count,
} SpirouOp;

#undef SPIROU_OP_ENUM

typedef struct SpirouOpInfo
{
    const char* mnemonic;
    uint32_t flags;
    uint16_t payload_size;
    uint8_t num_inputs;
    uint8_t num_outputs;
} SpirouOpInfo;

SPIROU_API extern const SpirouOpInfo spirou_op_infos[SpirouOp_Count];

SPIROU_FORCE_INLINE const SpirouOpInfo* spirou_op_info(SpirouOp op)
{
    return &spirou_op_infos[op];
}

SPIROU_FORCE_INLINE bool spirou_op_has_flag(SpirouOp op, uint32_t flag)
{
    return (spirou_op_infos[op].flags & flag) != 0;
}

/* Predicates for ICmp and FCmp, ordered float predicates are false if any operand is NaN */
#define SPIROU_CMPS(X)  \
    X(Eq, "eq")         \
    X(Ne, "ne")         \
    X(SLt, "slt")       \
    X(SLe, "sle")       \
    X(SGt, "sgt")       \
    X(SGe, "sge")       \
    X(ULt, "ult")       \
    X(ULe, "ule")       \
    X(UGt, "ugt")       \
    X(UGe, "uge")       \
    X(OEq, "oeq")       \
    X(ONe, "one")       \
    X(OLt, "olt")       \
    X(OLe, "ole")       \
    X(OGt, "ogt")       \
    X(OGe, "oge")       \
    X(UEq, "ueq")       \
    X(UNe, "une")       \
    X(UnLt, "unlt")     \
    X(UnLe, "unle")     \
    X(UnGt, "ungt")     \
    X(UnGe, "unge")     \
    X(Ord, "ord")       \
    X(Uno, "uno")

#define SPIROU_CMP_ENUM(name, mnemonic) SpirouCmp_##name,

typedef enum SpirouCmp
{
    SPIROU_CMPS(SPIROU_CMP_ENUM)
    SpirouCmp_Count,
} SpirouCmp;

#undef SPIROU_CMP_ENUM

SPIROU_API extern const char* spirou_cmp_mnemonics[SpirouCmp_Count];

SPIROU_FORCE_INLINE bool spirou_cmp_is_int(SpirouCmp cmp)
{
    return cmp <= SpirouCmp_UGe;
}

SPIROU_CPP_END

#endif /* !defined(__SPIROU_OP) */
