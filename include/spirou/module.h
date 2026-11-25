/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_MODULE)
#define __SPIROU_MODULE

#include "spirou/type.h"

#include "libromano/arena.h"
#include "libromano/hashmap.h"
#include "libromano/vector.h"

SPIROU_CPP_ENTER

typedef struct SpirouFunction SpirouFunction;

typedef enum SpirouLinkage
{
    SpirouLinkage_Internal,
    SpirouLinkage_Export,
    SpirouLinkage_Import,
} SpirouLinkage;

typedef enum SpirouSymbolKind
{
    SpirouSymbolKind_Function,
    SpirouSymbolKind_Global,
} SpirouSymbolKind;

typedef struct SpirouSymbolInfo
{
    const char* name;
    uint32_t name_len;
    uint8_t kind;
    uint8_t linkage;
    uint16_t flags;
    SpirouType type;
    uint32_t index;
} SpirouSymbolInfo;

typedef enum SpirouGlobalFlags
{
    SpirouGlobalFlags_Constant = 0x1,
} SpirouGlobalFlags;

typedef struct SpirouGlobal
{
    SpirouSymbol symbol;
    SpirouType type;
    uint32_t align;
    uint32_t flags;
    const uint8_t* init;
    uint32_t init_size;
} SpirouGlobal;

/*
 * A module owns types, symbols, globals and functions. Each function owns its graph and arena,
 * so functions can be built, optimized and freed independently (and in parallel)
 */
struct SpirouModule
{
    Arena arena;
    Vector types;
    Vector type_pool;
    HashMap* type_map;
    Vector symbols;
    HashMap* symbol_map;
    Vector functions;
    Vector globals;
    uint32_t pointer_size;
};

SPIROU_API SpirouModule* spirou_module_new(uint32_t pointer_size);

SPIROU_API void spirou_module_free(SpirouModule* module);

SPIROU_API SpirouSymbol spirou_module_find_symbol(SpirouModule* module, const char* name, uint32_t name_len);

SPIROU_API const SpirouSymbolInfo* spirou_module_symbol(const SpirouModule* module, SpirouSymbol symbol);

/* Returns NULL if the name is already used. Import functions have no body */
SPIROU_API SpirouFunction* spirou_module_add_function(SpirouModule* module,
                                                     const char* name,
                                                     uint32_t name_len,
                                                     SpirouType func_type,
                                                     SpirouLinkage linkage);

/* init can be NULL for zero-initialized globals, init bytes are copied in the module */
SPIROU_API SpirouSymbol spirou_module_add_global(SpirouModule* module,
                                                 const char* name,
                                                 uint32_t name_len,
                                                 SpirouType type,
                                                 uint32_t align,
                                                 uint32_t flags,
                                                 const void* init,
                                                 uint32_t init_size,
                                                 SpirouLinkage linkage);

SPIROU_API uint32_t spirou_module_num_functions(SpirouModule* module);

SPIROU_API SpirouFunction* spirou_module_function(SpirouModule* module, uint32_t index);

SPIROU_API SpirouGlobal* spirou_module_global(SpirouModule* module, SpirouSymbol symbol);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_MODULE) */
