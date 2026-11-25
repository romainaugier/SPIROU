/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/module.h"

#include "libromano/memory.h"

#include <string.h>

#define TYPE_KEY_INLINE_MEMBERS 16

typedef struct TypeKey
{
    uint8_t kind;
    uint8_t flags;
    uint16_t width;
    uint32_t elem;
    uint32_t count;
} TypeKey;

static uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static uint32_t pow2_ceil(uint32_t value)
{
    uint32_t p = 1;

    while(p < value)
        p <<= 1;

    return p;
}

static const SpirouTypeInfo* type_at(const SpirouModule* module, SpirouType type)
{
    return (const SpirouTypeInfo*)vector_at((Vector*)&module->types, type);
}

static uint32_t pool_at(const SpirouModule* module, uint32_t offset)
{
    return *(uint32_t*)vector_at((Vector*)&module->type_pool, offset);
}

static void compute_layout(SpirouModule* module,
                           SpirouTypeInfo* info,
                           const SpirouType* members,
                           uint32_t num_members)
{
    const SpirouTypeInfo* elem;
    uint32_t i;

    switch(info->kind)
    {
        case SpirouTypeKind_Int:
        case SpirouTypeKind_Float:
            info->size = pow2_ceil((info->width + 7) / 8);
            info->align = info->size;
            break;
        case SpirouTypeKind_Ptr:
            info->size = module->pointer_size;
            info->align = module->pointer_size;
            break;
        case SpirouTypeKind_Vector:
            elem = type_at(module, info->elem);
            info->size = pow2_ceil(elem->size * info->width);
            info->align = info->size;
            break;
        case SpirouTypeKind_Array:
            elem = type_at(module, info->elem);
            info->size = elem->size * info->count;
            info->align = elem->align;
            break;
        case SpirouTypeKind_Struct:
            info->size = 0;
            info->align = 1;

            for(i = 0; i < num_members; i++)
            {
                elem = type_at(module, members[i]);

                if(!(info->flags & SpirouTypeFlags_Packed))
                {
                    info->size = align_up(info->size, elem->align);
                    info->align = elem->align > info->align ? elem->align : info->align;
                }

                info->size += elem->size;
            }

            info->size = align_up(info->size, info->align);
            break;
        default:
            info->size = 0;
            info->align = 1;
            break;
    }
}

static SpirouType intern(SpirouModule* module,
                         SpirouTypeKind kind,
                         uint8_t flags,
                         uint16_t width,
                         uint32_t elem,
                         uint32_t count,
                         const SpirouType* members,
                         uint32_t num_members)
{
    uint8_t inline_key[sizeof(TypeKey) + TYPE_KEY_INLINE_MEMBERS * sizeof(SpirouType)];
    uint8_t* key;
    uint32_t key_size;
    TypeKey header;
    SpirouTypeInfo info;
    SpirouType type;
    uint32_t value_size;
    uint32_t i;
    void* found;

    memset(&header, 0, sizeof(TypeKey));
    header.kind = (uint8_t)kind;
    header.flags = flags;
    header.width = width;
    header.elem = elem;
    header.count = count;

    key_size = (uint32_t)(sizeof(TypeKey) + num_members * sizeof(SpirouType));
    key = num_members <= TYPE_KEY_INLINE_MEMBERS ? inline_key : (uint8_t*)romano_malloc(key_size);

    memcpy(key, &header, sizeof(TypeKey));

    if(num_members > 0)
        memcpy(key + sizeof(TypeKey), members, num_members * sizeof(SpirouType));

    found = hashmap_get(module->type_map, key, key_size, &value_size);

    if(found != NULL)
    {
        type = *(SpirouType*)found;
    }
    else
    {
        memset(&info, 0, sizeof(SpirouTypeInfo));
        info.kind = (uint8_t)kind;
        info.flags = flags;
        info.width = width;
        info.elem = elem;
        info.count = count;

        if(kind == SpirouTypeKind_Struct || kind == SpirouTypeKind_Func)
        {
            info.elem = (uint32_t)vector_size(&module->type_pool);

            for(i = 0; i < num_members; i++)
                vector_push_back(&module->type_pool, (void*)&members[i]);
        }

        compute_layout(module, &info, members, num_members);

        type = (SpirouType)vector_size(&module->types);
        vector_push_back(&module->types, &info);
        hashmap_insert(module->type_map, key, key_size, &type, sizeof(SpirouType));
    }

    if(key != inline_key)
        romano_free(key);

    return type;
}

SpirouType spirou_type_int(SpirouModule* module, uint32_t bits)
{
    SPIROU_ASSERT(bits >= 1 && bits <= 64, "Integer width must be in [1, 64]");
    return intern(module, SpirouTypeKind_Int, 0, (uint16_t)bits, 0, 0, NULL, 0);
}

SpirouType spirou_type_float(SpirouModule* module, uint32_t bits)
{
    SPIROU_ASSERT(bits == 16 || bits == 32 || bits == 64, "Float width must be 16, 32 or 64");
    return intern(module, SpirouTypeKind_Float, 0, (uint16_t)bits, 0, 0, NULL, 0);
}

SpirouType spirou_type_ptr(SpirouModule* module, uint32_t address_space)
{
    return intern(module, SpirouTypeKind_Ptr, 0, (uint16_t)address_space, 0, 0, NULL, 0);
}

SpirouType spirou_type_vector(SpirouModule* module, SpirouType elem, uint32_t lanes)
{
    return intern(module, SpirouTypeKind_Vector, 0, (uint16_t)lanes, elem, 0, NULL, 0);
}

SpirouType spirou_type_array(SpirouModule* module, SpirouType elem, uint32_t count)
{
    return intern(module, SpirouTypeKind_Array, 0, 0, elem, count, NULL, 0);
}

SpirouType spirou_type_struct(SpirouModule* module,
                              const SpirouType* fields,
                              uint32_t num_fields,
                              bool packed)
{
    return intern(module,
                  SpirouTypeKind_Struct,
                  packed ? SpirouTypeFlags_Packed : 0,
                  0,
                  0,
                  num_fields,
                  fields,
                  num_fields);
}

SpirouType spirou_type_func(SpirouModule* module,
                            const SpirouType* params,
                            uint32_t num_params,
                            const SpirouType* results,
                            uint32_t num_results)
{
    SpirouType inline_members[TYPE_KEY_INLINE_MEMBERS];
    SpirouType* members;
    uint32_t num_members = num_params + num_results;
    SpirouType type;

    members = num_members <= TYPE_KEY_INLINE_MEMBERS
                  ? inline_members
                  : (SpirouType*)romano_malloc(num_members * sizeof(SpirouType));

    if(num_params > 0)
        memcpy(members, params, num_params * sizeof(SpirouType));

    if(num_results > 0)
        memcpy(members + num_params, results, num_results * sizeof(SpirouType));

    type = intern(module,
                  SpirouTypeKind_Func,
                  0,
                  (uint16_t)num_results,
                  0,
                  num_params,
                  members,
                  num_members);

    if(members != inline_members)
        romano_free(members);

    return type;
}

SpirouType spirou_type_state(SpirouModule* module)
{
    return intern(module, SpirouTypeKind_State, 0, 0, 0, 0, NULL, 0);
}

SpirouType spirou_type_varying(SpirouModule* module, SpirouType type)
{
    const SpirouTypeInfo* info = type_at(module, type);

    if(info->flags & SpirouTypeFlags_Varying)
        return type;

    if(info->kind != SpirouTypeKind_Int && info->kind != SpirouTypeKind_Float &&
       info->kind != SpirouTypeKind_Ptr)
        return SPIROU_INVALID;

    return intern(module,
                  (SpirouTypeKind)info->kind,
                  info->flags | SpirouTypeFlags_Varying,
                  info->width,
                  0,
                  0,
                  NULL,
                  0);
}

SpirouType spirou_type_uniform(SpirouModule* module, SpirouType type)
{
    const SpirouTypeInfo* info = type_at(module, type);

    if(!(info->flags & SpirouTypeFlags_Varying))
        return type;

    return intern(module,
                  (SpirouTypeKind)info->kind,
                  info->flags & ~SpirouTypeFlags_Varying,
                  info->width,
                  0,
                  0,
                  NULL,
                  0);
}

const SpirouTypeInfo* spirou_type_info(const SpirouModule* module, SpirouType type)
{
    return type_at(module, type);
}

SpirouType spirou_type_field(const SpirouModule* module, SpirouType type, uint32_t index)
{
    const SpirouTypeInfo* info = type_at(module, type);

    SPIROU_ASSERT(info->kind == SpirouTypeKind_Struct && index < info->count, "Invalid struct field");

    return pool_at(module, info->elem + index);
}

uint32_t spirou_type_field_offset(const SpirouModule* module, SpirouType type, uint32_t index)
{
    const SpirouTypeInfo* info = type_at(module, type);
    const SpirouTypeInfo* field;
    uint32_t offset = 0;
    uint32_t i;

    for(i = 0; i <= index; i++)
    {
        field = type_at(module, pool_at(module, info->elem + i));

        if(!(info->flags & SpirouTypeFlags_Packed))
            offset = align_up(offset, field->align);

        if(i < index)
            offset += field->size;
    }

    return offset;
}

SpirouType spirou_type_func_param(const SpirouModule* module, SpirouType type, uint32_t index)
{
    const SpirouTypeInfo* info = type_at(module, type);

    SPIROU_ASSERT(info->kind == SpirouTypeKind_Func && index < info->count, "Invalid func param");

    return pool_at(module, info->elem + index);
}

SpirouType spirou_type_func_result(const SpirouModule* module, SpirouType type, uint32_t index)
{
    const SpirouTypeInfo* info = type_at(module, type);

    SPIROU_ASSERT(info->kind == SpirouTypeKind_Func && index < info->width, "Invalid func result");

    return pool_at(module, info->elem + info->count + index);
}

bool spirou_type_is_varying(const SpirouModule* module, SpirouType type)
{
    return (type_at(module, type)->flags & SpirouTypeFlags_Varying) != 0;
}

SpirouTypeKind spirou_type_scalar_kind(const SpirouModule* module, SpirouType type)
{
    const SpirouTypeInfo* info = type_at(module, type);

    if(info->kind == SpirouTypeKind_Vector)
        info = type_at(module, info->elem);

    return (SpirouTypeKind)info->kind;
}

static void print_list(const SpirouModule* module, uint32_t offset, uint32_t count, String* out)
{
    uint32_t i;

    for(i = 0; i < count; i++)
    {
        if(i > 0)
            string_appendc(out, ", ");

        spirou_type_print(module, pool_at(module, offset + i), out);
    }
}

void spirou_type_print(const SpirouModule* module, SpirouType type, String* out)
{
    const SpirouTypeInfo* info;

    if(type == SPIROU_INVALID)
    {
        string_appendc(out, "<invalid>");
        return;
    }

    info = type_at(module, type);

    if(info->flags & SpirouTypeFlags_Varying)
        string_appendc(out, "varying ");

    switch(info->kind)
    {
        case SpirouTypeKind_Int:
            string_appendf(out, "i%u", (uint32_t)info->width);
            break;
        case SpirouTypeKind_Float:
            string_appendf(out, "f%u", (uint32_t)info->width);
            break;
        case SpirouTypeKind_Ptr:
            if(info->width == 0)
                string_appendc(out, "ptr");
            else
                string_appendf(out, "ptr(%u)", (uint32_t)info->width);
            break;
        case SpirouTypeKind_Vector:
            string_appendf(out, "<%u x ", (uint32_t)info->width);
            spirou_type_print(module, info->elem, out);
            string_appendc(out, ">");
            break;
        case SpirouTypeKind_Array:
            string_appendf(out, "[%u x ", info->count);
            spirou_type_print(module, info->elem, out);
            string_appendc(out, "]");
            break;
        case SpirouTypeKind_Struct:
            string_appendc(out, (info->flags & SpirouTypeFlags_Packed) ? "<{" : "{");
            print_list(module, info->elem, info->count, out);
            string_appendc(out, (info->flags & SpirouTypeFlags_Packed) ? "}>" : "}");
            break;
        case SpirouTypeKind_Func:
            string_appendc(out, "fn(");
            print_list(module, info->elem, info->count, out);
            string_appendc(out, ") -> (");
            print_list(module, info->elem + info->count, info->width, out);
            string_appendc(out, ")");
            break;
        case SpirouTypeKind_State:
            string_appendc(out, "state");
            break;
    }
}
