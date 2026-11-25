/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_TYPE)
#define __SPIROU_TYPE

#include "spirou/common.h"

#include "libromano/string.h"

SPIROU_CPP_ENTER

typedef struct SpirouModule SpirouModule;

typedef enum SpirouTypeKind
{
    SpirouTypeKind_Int,
    SpirouTypeKind_Float,
    SpirouTypeKind_Ptr,
    SpirouTypeKind_Vector,
    SpirouTypeKind_Array,
    SpirouTypeKind_Struct,
    SpirouTypeKind_Func,
    SpirouTypeKind_State,
} SpirouTypeKind;

typedef enum SpirouTypeFlags
{
    /* One value per lane of the enclosing map, lowered to a SIMD register or a GPU thread */
    SpirouTypeFlags_Varying = 0x1,
    SpirouTypeFlags_Packed = 0x2,
} SpirouTypeFlags;

/*
 * Interned in the module, two types are equal if their ids are equal.
 * width: int/float bits, vector lanes, ptr address space, func result count
 * elem: vector/array element type, struct fields / func params offset in the module type pool
 * count: array length, struct field count, func param count
 */
typedef struct SpirouTypeInfo
{
    uint8_t kind;
    uint8_t flags;
    uint16_t width;
    uint32_t elem;
    uint32_t count;
    uint32_t size;
    uint32_t align;
} SpirouTypeInfo;

SPIROU_API SpirouType spirou_type_int(SpirouModule* module, uint32_t bits);

SPIROU_API SpirouType spirou_type_float(SpirouModule* module, uint32_t bits);

SPIROU_API SpirouType spirou_type_ptr(SpirouModule* module, uint32_t address_space);

SPIROU_API SpirouType spirou_type_vector(SpirouModule* module, SpirouType elem, uint32_t lanes);

SPIROU_API SpirouType spirou_type_array(SpirouModule* module, SpirouType elem, uint32_t count);

SPIROU_API SpirouType spirou_type_struct(SpirouModule* module,
                                         const SpirouType* fields,
                                         uint32_t num_fields,
                                         bool packed);

SPIROU_API SpirouType spirou_type_func(SpirouModule* module,
                                       const SpirouType* params,
                                       uint32_t num_params,
                                       const SpirouType* results,
                                       uint32_t num_results);

SPIROU_API SpirouType spirou_type_state(SpirouModule* module);

/* Returns SPIROU_INVALID for types that cannot vary per lane (only int, float and ptr can) */
SPIROU_API SpirouType spirou_type_varying(SpirouModule* module, SpirouType type);

SPIROU_API SpirouType spirou_type_uniform(SpirouModule* module, SpirouType type);

SPIROU_API const SpirouTypeInfo* spirou_type_info(const SpirouModule* module, SpirouType type);

SPIROU_API SpirouType spirou_type_field(const SpirouModule* module, SpirouType type, uint32_t index);

SPIROU_API uint32_t spirou_type_field_offset(const SpirouModule* module, SpirouType type, uint32_t index);

SPIROU_API SpirouType spirou_type_func_param(const SpirouModule* module, SpirouType type, uint32_t index);

SPIROU_API SpirouType spirou_type_func_result(const SpirouModule* module, SpirouType type, uint32_t index);

SPIROU_API bool spirou_type_is_varying(const SpirouModule* module, SpirouType type);

/* Kind of the scalar, looking through vectors */
SPIROU_API SpirouTypeKind spirou_type_scalar_kind(const SpirouModule* module, SpirouType type);

SPIROU_API void spirou_type_print(const SpirouModule* module, SpirouType type, String* out);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_TYPE) */
