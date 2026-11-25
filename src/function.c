/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/arena.h"
#include "spirou/function.h"

#include "libromano/memory.h"

void spirou_function_init(SpirouFunction* func, SpirouModule* module, SpirouSymbol symbol, SpirouType type);

void spirou_function_release(SpirouFunction* func);

void spirou_function_init(SpirouFunction* func, SpirouModule* module, SpirouSymbol symbol, SpirouType type)
{
    const SpirouTypeInfo* info;
    uint32_t i;

    func->module = module;
    func->symbol = symbol;
    func->type = type;
    func->body = SPIROU_INVALID;

    arena_init(&func->arena, SPIROU_ARENA_BLOCK_SIZE);
    vector_init(&func->nodes, 64, sizeof(SpirouNode*));
    vector_init(&func->regions, 16, sizeof(SpirouRegion));
    vector_init(&func->values, 128, sizeof(SpirouValueInfo));
    vector_init(&func->forward, 128, sizeof(SpirouValue));
    vector_init(&func->operands, 256, sizeof(uint32_t));

    if(spirou_function_is_import(func))
        return;

    func->body = spirou_function_new_region(func, SPIROU_INVALID);
    info = spirou_type_info(module, type);

    for(i = 0; i < info->count; i++)
        spirou_region_add_arg(func, func->body, spirou_type_func_param(module, type, i));
}

void spirou_function_release(SpirouFunction* func)
{
    vector_release(&func->operands);
    vector_release(&func->forward);
    vector_release(&func->values);
    vector_release(&func->regions);
    vector_release(&func->nodes);
    arena_release(&func->arena);
}

const SpirouSymbolInfo* spirou_function_symbol(SpirouFunction* func)
{
    return spirou_module_symbol(func->module, func->symbol);
}

bool spirou_function_is_import(SpirouFunction* func)
{
    return spirou_function_symbol(func)->linkage == SpirouLinkage_Import;
}

static void operands_reserve(SpirouFunction* func, size_t extra)
{
    size_t size = vector_size(&func->operands);
    size_t capacity = vector_capacity(&func->operands);
    size_t needed = size + extra;

    if(needed > capacity)
        vector_resize(&func->operands, needed > capacity * 2 ? needed : capacity * 2);
}

/* Appends to a span of the operand pool, moving the span to the end of the pool if needed */
static void span_append(SpirouFunction* func, uint32_t* offset, uint32_t count, uint32_t value)
{
    uint32_t size = (uint32_t)vector_size(&func->operands);
    uint32_t i;

    operands_reserve(func, count + 1);

    if(count == 0)
    {
        *offset = size;
    }
    else if(*offset + count != size)
    {
        for(i = 0; i < count; i++)
        {
            uint32_t moved = *spirou_operands(func, *offset + i);
            vector_push_back(&func->operands, &moved);
        }

        *offset = size;
    }

    vector_push_back(&func->operands, &value);
}

static SpirouValue new_value(SpirouFunction* func,
                             SpirouType type,
                             SpirouValueKind kind,
                             uint32_t def,
                             uint32_t index)
{
    SpirouValueInfo info;
    SpirouValue value = (SpirouValue)vector_size(&func->values);

    info.type = type;
    info.def = def;
    info.index = (uint16_t)index;
    info.kind = (uint16_t)kind;

    vector_push_back(&func->values, &info);
    vector_push_back(&func->forward, &value);

    return value;
}

SpirouValue spirou_value_resolve(SpirouFunction* func, SpirouValue value)
{
    SpirouValue* forward = (SpirouValue*)vector_at(&func->forward, 0);

    while(forward[value] != value)
    {
        forward[value] = forward[forward[value]];
        value = forward[value];
    }

    return value;
}

void spirou_value_replace(SpirouFunction* func, SpirouValue from, SpirouValue to)
{
    from = spirou_value_resolve(func, from);
    to = spirou_value_resolve(func, to);

    if(from != to)
        *(SpirouValue*)vector_at(&func->forward, from) = to;
}

SpirouNodeId spirou_value_node(SpirouFunction* func, SpirouValue value)
{
    const SpirouValueInfo* info = spirou_value_info(func, value);

    return info->kind == SpirouValueKind_Output ? info->def : SPIROU_INVALID;
}

SpirouRegionId spirou_value_region(SpirouFunction* func, SpirouValue value)
{
    const SpirouValueInfo* info = spirou_value_info(func, value);

    return info->kind == SpirouValueKind_Output ? spirou_node(func, info->def)->region : info->def;
}

SpirouValue spirou_node_input(SpirouFunction* func, SpirouNodeId node, uint32_t index)
{
    SpirouNode* n = spirou_node(func, node);

    SPIROU_ASSERT(index < n->num_inputs, "Node input out of bounds");

    return spirou_value_resolve(func, *spirou_operands(func, n->inputs + index));
}

void spirou_node_set_input(SpirouFunction* func, SpirouNodeId node, uint32_t index, SpirouValue value)
{
    SpirouNode* n = spirou_node(func, node);

    SPIROU_ASSERT(index < n->num_inputs, "Node input out of bounds");

    *spirou_operands(func, n->inputs + index) = value;
}

SpirouValue spirou_node_output(SpirouFunction* func, SpirouNodeId node, uint32_t index)
{
    SpirouNode* n = spirou_node(func, node);

    SPIROU_ASSERT(index < n->num_outputs, "Node output out of bounds");

    return *spirou_operands(func, n->outputs + index);
}

uint32_t spirou_node_num_regions(SpirouFunction* func, SpirouNodeId node)
{
    SpirouNode* n = spirou_node(func, node);

    switch(n->op)
    {
        case SpirouOp_Gamma:
            return ((SpirouNodeGamma*)n)->num_regions;
        case SpirouOp_Theta:
        case SpirouOp_Map:
            return 1;
        default:
            return 0;
    }
}

SpirouRegionId spirou_node_subregion(SpirouFunction* func, SpirouNodeId node, uint32_t index)
{
    SpirouNode* n = spirou_node(func, node);

    switch(n->op)
    {
        case SpirouOp_Gamma:
            SPIROU_ASSERT(index < ((SpirouNodeGamma*)n)->num_regions, "Gamma branch out of bounds");
            return ((SpirouNodeGamma*)n)->first_region + index;
        case SpirouOp_Theta:
            return ((SpirouNodeTheta*)n)->body;
        case SpirouOp_Map:
            return ((SpirouNodeMap*)n)->body;
        default:
            return SPIROU_INVALID;
    }
}

SpirouValue spirou_region_arg(SpirouFunction* func, SpirouRegionId region, uint32_t index)
{
    SpirouRegion* r = spirou_region(func, region);

    SPIROU_ASSERT(index < r->num_args, "Region argument out of bounds");

    return *spirou_operands(func, r->args + index);
}

SpirouValue spirou_region_result(SpirouFunction* func, SpirouRegionId region, uint32_t index)
{
    SpirouRegion* r = spirou_region(func, region);
    SpirouValue value;

    SPIROU_ASSERT(index < r->num_results, "Region result out of bounds");

    value = *spirou_operands(func, r->results + index);

    return value == SPIROU_INVALID ? value : spirou_value_resolve(func, value);
}

void spirou_region_set_result(SpirouFunction* func, SpirouRegionId region, uint32_t index, SpirouValue value)
{
    SpirouRegion* r = spirou_region(func, region);

    SPIROU_ASSERT(index < r->num_results, "Region result out of bounds");

    *spirou_operands(func, r->results + index) = value;
}

SpirouRegionId spirou_function_new_region(SpirouFunction* func, SpirouNodeId parent)
{
    SpirouRegion region;
    SpirouRegionId id = (SpirouRegionId)vector_size(&func->regions);

    region.parent = parent;
    region.first = SPIROU_INVALID;
    region.last = SPIROU_INVALID;
    region.args = 0;
    region.results = 0;
    region.num_args = 0;
    region.num_results = 0;

    vector_push_back(&func->regions, &region);

    return id;
}

SpirouValue spirou_region_add_arg(SpirouFunction* func, SpirouRegionId region, SpirouType type)
{
    SpirouRegion* r = spirou_region(func, region);
    SpirouValue value = new_value(func, type, SpirouValueKind_Argument, region, r->num_args);

    span_append(func, &r->args, r->num_args, value);
    r->num_args++;

    return value;
}

uint32_t spirou_region_add_result(SpirouFunction* func, SpirouRegionId region, SpirouValue value)
{
    SpirouRegion* r = spirou_region(func, region);

    span_append(func, &r->results, r->num_results, value);

    return r->num_results++;
}

SpirouNodeId spirou_function_new_node(SpirouFunction* func,
                                      SpirouOp op,
                                      SpirouRegionId region,
                                      const SpirouValue* inputs,
                                      uint32_t num_inputs,
                                      const SpirouType* output_types,
                                      uint32_t num_outputs)
{
    SpirouNodeId id = (SpirouNodeId)vector_size(&func->nodes);
    SpirouNode* node;
    SpirouRegion* r;
    uint32_t i;

    node = (SpirouNode*)spirou_arena_alloc(&func->arena, spirou_op_info(op)->payload_size);

    if(node == NULL)
        return SPIROU_INVALID;

    node->op = (uint16_t)op;
    node->region = region;
    node->next = SPIROU_INVALID;
    node->inputs = (uint32_t)vector_size(&func->operands);
    node->outputs = node->inputs;

    vector_push_back(&func->nodes, &node);

    operands_reserve(func, num_inputs + num_outputs);

    for(i = 0; i < num_inputs; i++)
        vector_push_back(&func->operands, (void*)&inputs[i]);

    node->num_inputs = (uint16_t)num_inputs;
    node->outputs = (uint32_t)vector_size(&func->operands);

    for(i = 0; i < num_outputs; i++)
    {
        SpirouValue value = new_value(func, output_types[i], SpirouValueKind_Output, id, i);
        vector_push_back(&func->operands, &value);
    }

    node->num_outputs = (uint16_t)num_outputs;

    r = spirou_region(func, region);

    if(r->last == SPIROU_INVALID)
        r->first = id;
    else
        spirou_node(func, r->last)->next = id;

    r->last = id;

    return id;
}

uint32_t spirou_node_add_input(SpirouFunction* func, SpirouNodeId node, SpirouValue value)
{
    SpirouNode* n = spirou_node(func, node);

    span_append(func, &n->inputs, n->num_inputs, value);

    return n->num_inputs++;
}

SpirouValue spirou_node_add_output(SpirouFunction* func, SpirouNodeId node, SpirouType type)
{
    SpirouNode* n = spirou_node(func, node);
    SpirouValue value = new_value(func, type, SpirouValueKind_Output, node, n->num_outputs);

    span_append(func, &n->outputs, n->num_outputs, value);
    n->num_outputs++;

    return value;
}

void spirou_node_kill(SpirouFunction* func, SpirouNodeId node)
{
    SpirouNode* n = spirou_node(func, node);
    SpirouRegion* r = spirou_region(func, n->region);
    SpirouNodeId prev = SPIROU_INVALID;
    SpirouNodeId it;

    if(n->flags & SpirouNodeFlags_Dead)
        return;

    for(it = r->first; it != node && it != SPIROU_INVALID; it = spirou_node(func, it)->next)
        prev = it;

    if(it == node)
    {
        if(prev == SPIROU_INVALID)
            r->first = n->next;
        else
            spirou_node(func, prev)->next = n->next;

        if(r->last == node)
            r->last = prev;
    }

    n->next = SPIROU_INVALID;
    n->flags |= SpirouNodeFlags_Dead;
}

typedef struct TopoFrame
{
    SpirouNodeId node;
    uint32_t input;
} TopoFrame;

bool spirou_topo_compute(SpirouFunction* func, SpirouTopo* topo)
{
    uint32_t num_nodes = spirou_function_num_nodes(func);
    uint32_t num_regions = spirou_function_num_regions(func);
    uint8_t* state = (uint8_t*)romano_calloc(num_nodes + 1, 1);
    Vector stack;
    bool acyclic = true;
    SpirouRegionId region;

    vector_init(&topo->order, num_nodes + 1, sizeof(SpirouNodeId));
    vector_init(&topo->spans, num_regions + 1, sizeof(SpirouSpan));
    vector_init(&stack, 64, sizeof(TopoFrame));

    for(region = 0; region < num_regions; region++)
    {
        SpirouSpan span;
        SpirouNodeId root;

        span.offset = (uint32_t)vector_size(&topo->order);

        for(root = spirou_region(func, region)->first; root != SPIROU_INVALID;
            root = spirou_node(func, root)->next)
        {
            TopoFrame frame;

            if(state[root] != 0)
                continue;

            state[root] = 1;
            frame.node = root;
            frame.input = 0;
            vector_push_back(&stack, &frame);

            while(vector_size(&stack) > 0)
            {
                TopoFrame* top = (TopoFrame*)vector_back(&stack);
                SpirouNodeId current = top->node;
                SpirouNode* node = spirou_node(func, current);

                if(top->input < node->num_inputs)
                {
                    SpirouValue input = spirou_value_resolve(func, *spirou_operands(func, node->inputs + top->input));
                    const SpirouValueInfo* info = spirou_value_info(func, input);
                    SpirouNode* def;

                    top->input++;

                    if(info->kind != SpirouValueKind_Output)
                        continue;

                    def = spirou_node(func, info->def);

                    if(def->region != node->region || (def->flags & SpirouNodeFlags_Dead))
                        continue;

                    if(state[info->def] == 1)
                    {
                        acyclic = false;
                    }
                    else if(state[info->def] == 0)
                    {
                        state[info->def] = 1;
                        frame.node = info->def;
                        frame.input = 0;
                        vector_push_back(&stack, &frame);
                    }
                }
                else
                {
                    state[current] = 2;
                    vector_push_back(&topo->order, &current);
                    vector_pop(&stack);
                }
            }
        }

        span.count = (uint32_t)vector_size(&topo->order) - span.offset;
        vector_push_back(&topo->spans, &span);
    }

    vector_release(&stack);
    romano_free(state);

    return acyclic;
}

void spirou_topo_release(SpirouTopo* topo)
{
    vector_release(&topo->order);
    vector_release(&topo->spans);
}
