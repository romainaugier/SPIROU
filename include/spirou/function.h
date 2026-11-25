/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_FUNCTION)
#define __SPIROU_FUNCTION

#include "spirou/module.h"
#include "spirou/op.h"

SPIROU_CPP_ENTER

/*
 * The function body is a tree of regions (RVSDG). A region is an acyclic dataflow graph with
 * arguments and results. Structural nodes (gamma, theta, map) own sub-regions. Nodes only
 * consume values defined in their own region: outputs of sibling nodes or region arguments.
 * Memory and IO ordering is explicit through values of state type.
 */

typedef enum SpirouNodeFlags
{
    SpirouNodeFlags_Dead = 0x1,
} SpirouNodeFlags;

/* Inputs and outputs are spans in the function operand pool */
typedef struct SpirouNode
{
    uint16_t op;
    uint16_t flags;
    SpirouRegionId region;
    SpirouNodeId next;
    uint32_t inputs;
    uint32_t outputs;
    uint16_t num_inputs;
    uint16_t num_outputs;
} SpirouNode;

typedef struct SpirouNodeIConst
{
    SpirouNode node;
    uint64_t value;
} SpirouNodeIConst;

typedef struct SpirouNodeFConst
{
    SpirouNode node;
    double value;
} SpirouNodeFConst;

typedef struct SpirouNodeCmp
{
    SpirouNode node;
    uint32_t predicate;
} SpirouNodeCmp;

typedef enum SpirouMemoryFlags
{
    SpirouMemoryFlags_Volatile = 0x1,
} SpirouMemoryFlags;

/* Load: (state, ptr) -> (state, value), Store: (state, ptr, value) -> (state) */
typedef struct SpirouNodeMemory
{
    SpirouNode node;
    uint32_t align;
    uint32_t flags;
} SpirouNodeMemory;

typedef struct SpirouNodeStackSlot
{
    SpirouNode node;
    uint32_t size;
    uint32_t align;
} SpirouNodeStackSlot;

typedef struct SpirouNodeSymbolAddr
{
    SpirouNode node;
    SpirouSymbol symbol;
} SpirouNodeSymbolAddr;

/* (callee, args...) -> (results...), args and results follow func_type */
typedef struct SpirouNodeCall
{
    SpirouNode node;
    SpirouType func_type;
} SpirouNodeCall;

/*
 * (predicate, entries...) -> (exits...)
 * Branch i is taken when predicate == i. Each branch region has one argument per entry and
 * one result per exit
 */
typedef struct SpirouNodeGamma
{
    SpirouNode node;
    SpirouRegionId first_region;
    uint32_t num_regions;
} SpirouNodeGamma;

/*
 * (loopvars...) -> (loopvars...)
 * Tail-controlled loop: body arguments are the loop variables, body results are
 * (continue: i1, next loopvars...)
 */
typedef struct SpirouNodeTheta
{
    SpirouNode node;
    SpirouRegionId body;
} SpirouNodeTheta;

/*
 * (count: i64, state, captures...) -> (state)
 * Data-parallel loop over [0, count): iterations have no ordering between them.
 * Body arguments are (index: varying i64, state, captures...), body results are (state).
 * Values depending on the index are varying, lowering maps them to SIMD lanes or GPU threads
 */
typedef struct SpirouNodeMap
{
    SpirouNode node;
    SpirouRegionId body;
} SpirouNodeMap;

typedef struct SpirouRegion
{
    SpirouNodeId parent;
    SpirouNodeId first;
    SpirouNodeId last;
    uint32_t args;
    uint32_t results;
    uint32_t num_args;
    uint32_t num_results;
} SpirouRegion;

typedef enum SpirouValueKind
{
    SpirouValueKind_Output,
    SpirouValueKind_Argument,
} SpirouValueKind;

/* def is the defining node for outputs, the region for arguments */
typedef struct SpirouValueInfo
{
    SpirouType type;
    uint32_t def;
    uint16_t index;
    uint16_t kind;
} SpirouValueInfo;

struct SpirouFunction
{
    SpirouModule* module;
    SpirouSymbol symbol;
    SpirouType type;
    SpirouRegionId body;
    Arena arena;
    Vector nodes;
    Vector regions;
    Vector values;
    Vector forward;
    Vector operands;
};

SPIROU_FORCE_INLINE SpirouNode* spirou_node(SpirouFunction* func, SpirouNodeId node)
{
    return *(SpirouNode**)vector_at(&func->nodes, node);
}

SPIROU_FORCE_INLINE SpirouRegion* spirou_region(SpirouFunction* func, SpirouRegionId region)
{
    return (SpirouRegion*)vector_at(&func->regions, region);
}

SPIROU_FORCE_INLINE const SpirouValueInfo* spirou_value_info(SpirouFunction* func, SpirouValue value)
{
    return (const SpirouValueInfo*)vector_at(&func->values, value);
}

SPIROU_FORCE_INLINE SpirouType spirou_value_type(SpirouFunction* func, SpirouValue value)
{
    return spirou_value_info(func, value)->type;
}

SPIROU_FORCE_INLINE uint32_t* spirou_operands(SpirouFunction* func, uint32_t offset)
{
    return (uint32_t*)vector_at(&func->operands, offset);
}

SPIROU_FORCE_INLINE uint32_t spirou_function_num_nodes(SpirouFunction* func)
{
    return (uint32_t)vector_size(&func->nodes);
}

SPIROU_FORCE_INLINE uint32_t spirou_function_num_regions(SpirouFunction* func)
{
    return (uint32_t)vector_size(&func->regions);
}

SPIROU_FORCE_INLINE uint32_t spirou_function_num_values(SpirouFunction* func)
{
    return (uint32_t)vector_size(&func->values);
}

SPIROU_API bool spirou_function_is_import(SpirouFunction* func);

SPIROU_API const SpirouSymbolInfo* spirou_function_symbol(SpirouFunction* func);

/* Follows replacements, rewrites never mutate users, they forward the replaced value */
SPIROU_API SpirouValue spirou_value_resolve(SpirouFunction* func, SpirouValue value);

SPIROU_API void spirou_value_replace(SpirouFunction* func, SpirouValue from, SpirouValue to);

/* Returns SPIROU_INVALID for region arguments */
SPIROU_API SpirouNodeId spirou_value_node(SpirouFunction* func, SpirouValue value);

SPIROU_API SpirouRegionId spirou_value_region(SpirouFunction* func, SpirouValue value);

SPIROU_API SpirouValue spirou_node_input(SpirouFunction* func, SpirouNodeId node, uint32_t index);

SPIROU_API void spirou_node_set_input(SpirouFunction* func, SpirouNodeId node, uint32_t index, SpirouValue value);

SPIROU_API SpirouValue spirou_node_output(SpirouFunction* func, SpirouNodeId node, uint32_t index);

SPIROU_API uint32_t spirou_node_num_regions(SpirouFunction* func, SpirouNodeId node);

SPIROU_API SpirouRegionId spirou_node_subregion(SpirouFunction* func, SpirouNodeId node, uint32_t index);

SPIROU_API SpirouValue spirou_region_arg(SpirouFunction* func, SpirouRegionId region, uint32_t index);

SPIROU_API SpirouValue spirou_region_result(SpirouFunction* func, SpirouRegionId region, uint32_t index);

SPIROU_API void spirou_region_set_result(SpirouFunction* func,
                                         SpirouRegionId region,
                                         uint32_t index,
                                         SpirouValue value);

/* Low-level graph construction, prefer the builder */

SPIROU_API SpirouRegionId spirou_function_new_region(SpirouFunction* func, SpirouNodeId parent);

SPIROU_API SpirouValue spirou_region_add_arg(SpirouFunction* func, SpirouRegionId region, SpirouType type);

SPIROU_API uint32_t spirou_region_add_result(SpirouFunction* func, SpirouRegionId region, SpirouValue value);

/* The node struct size comes from the op table, payload fields are zeroed */
SPIROU_API SpirouNodeId spirou_function_new_node(SpirouFunction* func,
                                                 SpirouOp op,
                                                 SpirouRegionId region,
                                                 const SpirouValue* inputs,
                                                 uint32_t num_inputs,
                                                 const SpirouType* output_types,
                                                 uint32_t num_outputs);

SPIROU_API uint32_t spirou_node_add_input(SpirouFunction* func, SpirouNodeId node, SpirouValue value);

SPIROU_API SpirouValue spirou_node_add_output(SpirouFunction* func, SpirouNodeId node, SpirouType type);

/* Removes the node from its region list and marks it dead, its values must be unused */
SPIROU_API void spirou_node_kill(SpirouFunction* func, SpirouNodeId node);

/*
 * Topological order of each region, inputs before users. order holds node ids, spans holds
 * (offset, count) into order per region id. Returns false if a region has a cycle
 */
typedef struct SpirouTopo
{
    Vector order;
    Vector spans;
} SpirouTopo;

typedef struct SpirouSpan
{
    uint32_t offset;
    uint32_t count;
} SpirouSpan;

SPIROU_API bool spirou_topo_compute(SpirouFunction* func, SpirouTopo* topo);

SPIROU_API void spirou_topo_release(SpirouTopo* topo);

SPIROU_FORCE_INLINE SpirouSpan spirou_topo_region(SpirouTopo* topo, SpirouRegionId region)
{
    return *(SpirouSpan*)vector_at(&topo->spans, region);
}

SPIROU_FORCE_INLINE SpirouNodeId spirou_topo_node(SpirouTopo* topo, uint32_t index)
{
    return *(SpirouNodeId*)vector_at(&topo->order, index);
}

SPIROU_CPP_END

#endif /* !defined(__SPIROU_FUNCTION) */
