/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/arena.h"
#include "spirou/function.h"

#include "libromano/memory.h"

void spirou_function_init(SpirouFunction* func, SpirouModule* module, SpirouSymbol symbol, SpirouType type);

void spirou_function_release(SpirouFunction* func);

SpirouModule* spirou_module_new(uint32_t pointer_size)
{
    SpirouModule* module = (SpirouModule*)romano_calloc(1, sizeof(SpirouModule));

    if(module == NULL)
        return NULL;

    arena_init(&module->arena, SPIROU_ARENA_BLOCK_SIZE);
    vector_init(&module->types, 64, sizeof(SpirouTypeInfo));
    vector_init(&module->type_pool, 64, sizeof(SpirouType));
    vector_init(&module->symbols, 64, sizeof(SpirouSymbolInfo));
    vector_init(&module->functions, 16, sizeof(SpirouFunction*));
    vector_init(&module->globals, 16, sizeof(SpirouGlobal));
    module->type_map = hashmap_new(128);
    module->symbol_map = hashmap_new(128);
    module->pointer_size = pointer_size == 0 ? (uint32_t)sizeof(void*) : pointer_size;

    return module;
}

void spirou_module_free(SpirouModule* module)
{
    size_t i;

    if(module == NULL)
        return;

    for(i = 0; i < vector_size(&module->functions); i++)
    {
        SpirouFunction* func = *(SpirouFunction**)vector_at(&module->functions, i);
        spirou_function_release(func);
        romano_free(func);
    }

    vector_release(&module->functions);
    vector_release(&module->globals);
    vector_release(&module->symbols);
    vector_release(&module->type_pool);
    vector_release(&module->types);
    hashmap_free(module->type_map);
    hashmap_free(module->symbol_map);
    arena_release(&module->arena);
    romano_free(module);
}

SpirouSymbol spirou_module_find_symbol(SpirouModule* module, const char* name, uint32_t name_len)
{
    uint32_t size;
    void* found = hashmap_get(module->symbol_map, name, name_len, &size);

    return found == NULL ? SPIROU_INVALID : *(SpirouSymbol*)found;
}

const SpirouSymbolInfo* spirou_module_symbol(const SpirouModule* module, SpirouSymbol symbol)
{
    return (const SpirouSymbolInfo*)vector_at((Vector*)&module->symbols, symbol);
}

static SpirouSymbol add_symbol(SpirouModule* module,
                               const char* name,
                               uint32_t name_len,
                               SpirouSymbolKind kind,
                               SpirouLinkage linkage,
                               SpirouType type,
                               uint32_t index)
{
    SpirouSymbolInfo info;
    SpirouSymbol symbol;
    char* name_copy;

    if(spirou_module_find_symbol(module, name, name_len) != SPIROU_INVALID)
        return SPIROU_INVALID;

    name_copy = (char*)spirou_arena_alloc(&module->arena, name_len + 1);
    memcpy(name_copy, name, name_len);

    info.name = name_copy;
    info.name_len = name_len;
    info.kind = (uint8_t)kind;
    info.linkage = (uint8_t)linkage;
    info.flags = 0;
    info.type = type;
    info.index = index;

    symbol = (SpirouSymbol)vector_size(&module->symbols);
    vector_push_back(&module->symbols, &info);
    hashmap_insert(module->symbol_map, name_copy, name_len, &symbol, sizeof(SpirouSymbol));

    return symbol;
}

SpirouFunction* spirou_module_add_function(SpirouModule* module,
                                          const char* name,
                                          uint32_t name_len,
                                          SpirouType func_type,
                                          SpirouLinkage linkage)
{
    SpirouFunction* func;
    SpirouSymbol symbol;

    SPIROU_ASSERT(spirou_type_info(module, func_type)->kind == SpirouTypeKind_Func,
                  "Function type expected");

    symbol = add_symbol(module,
                        name,
                        name_len,
                        SpirouSymbolKind_Function,
                        linkage,
                        func_type,
                        (uint32_t)vector_size(&module->functions));

    if(symbol == SPIROU_INVALID)
        return NULL;

    func = (SpirouFunction*)romano_calloc(1, sizeof(SpirouFunction));
    spirou_function_init(func, module, symbol, func_type);
    vector_push_back(&module->functions, &func);

    return func;
}

SpirouSymbol spirou_module_add_global(SpirouModule* module,
                                      const char* name,
                                      uint32_t name_len,
                                      SpirouType type,
                                      uint32_t align,
                                      uint32_t flags,
                                      const void* init,
                                      uint32_t init_size,
                                      SpirouLinkage linkage)
{
    SpirouGlobal global;
    SpirouSymbol symbol;
    uint8_t* init_copy = NULL;

    symbol = add_symbol(module,
                        name,
                        name_len,
                        SpirouSymbolKind_Global,
                        linkage,
                        type,
                        (uint32_t)vector_size(&module->globals));

    if(symbol == SPIROU_INVALID)
        return SPIROU_INVALID;

    if(init != NULL && init_size > 0)
    {
        init_copy = (uint8_t*)spirou_arena_alloc(&module->arena, init_size);
        memcpy(init_copy, init, init_size);
    }

    global.symbol = symbol;
    global.type = type;
    global.align = align == 0 ? spirou_type_info(module, type)->align : align;
    global.flags = flags;
    global.init = init_copy;
    global.init_size = init_copy != NULL ? init_size : 0;

    vector_push_back(&module->globals, &global);

    return symbol;
}

uint32_t spirou_module_num_functions(SpirouModule* module)
{
    return (uint32_t)vector_size(&module->functions);
}

SpirouFunction* spirou_module_function(SpirouModule* module, uint32_t index)
{
    return *(SpirouFunction**)vector_at(&module->functions, index);
}

SpirouGlobal* spirou_module_global(SpirouModule* module, SpirouSymbol symbol)
{
    const SpirouSymbolInfo* info = spirou_module_symbol(module, symbol);

    if(info->kind != SpirouSymbolKind_Global)
        return NULL;

    return (SpirouGlobal*)vector_at(&module->globals, info->index);
}
