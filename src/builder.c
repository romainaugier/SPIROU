/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/builder.h"

void spirou_builder_init(SpirouBuilder* builder, SpirouFunction* func)
{
    builder->func = func;
    builder->module = func->module;
    builder->region = func->body;
}

void spirou_builder_set_region(SpirouBuilder* builder, SpirouRegionId region)
{
    builder->region = region;
}

SpirouValue spirou_builder_param(SpirouBuilder* builder, uint32_t index)
{
    return spirou_region_arg(builder->func, builder->func->body, index);
}

void spirou_builder_return(SpirouBuilder* builder, const SpirouValue* values, uint32_t count)
{
    SpirouFunction* func = builder->func;
    SpirouRegion* body = spirou_region(func, func->body);
    uint32_t i;

    for(i = 0; i < count; i++)
    {
        if(i < body->num_results)
            spirou_region_set_result(func, func->body, i, values[i]);
        else
            spirou_region_add_result(func, func->body, values[i]);

        body = spirou_region(func, func->body);
    }
}

static bool is_varying(SpirouBuilder* builder, SpirouValue value)
{
    return spirou_type_is_varying(builder->module, spirou_value_type(builder->func, value));
}

static SpirouType with_varying(SpirouBuilder* builder, SpirouType type, bool varying)
{
    SpirouType result;

    if(!varying)
        return type;

    result = spirou_type_varying(builder->module, type);

    return result == SPIROU_INVALID ? type : result;
}

/* Common type of operands that must match modulo uniformity, mismatches are left to the verifier */
static SpirouType join(SpirouBuilder* builder, SpirouValue lhs, SpirouValue rhs)
{
    SpirouType lhs_type = spirou_value_type(builder->func, lhs);
    SpirouType rhs_type = spirou_value_type(builder->func, rhs);
    SpirouType uniform = spirou_type_uniform(builder->module, lhs_type);

    if(uniform != spirou_type_uniform(builder->module, rhs_type))
        return lhs_type;

    return with_varying(builder, uniform, is_varying(builder, lhs) || is_varying(builder, rhs));
}

static SpirouValue build_simple(SpirouBuilder* builder,
                                SpirouOp op,
                                const SpirouValue* inputs,
                                uint32_t num_inputs,
                                SpirouType type)
{
    SpirouNodeId node = spirou_function_new_node(builder->func, op, builder->region, inputs, num_inputs, &type, 1);

    return node == SPIROU_INVALID ? SPIROU_INVALID : spirou_node_output(builder->func, node, 0);
}

SpirouValue spirou_build_undef(SpirouBuilder* builder, SpirouType type)
{
    return build_simple(builder, SpirouOp_Undef, NULL, 0, type);
}

SpirouValue spirou_build_iconst(SpirouBuilder* builder, SpirouType type, uint64_t value)
{
    SpirouValue result = build_simple(builder, SpirouOp_IConst, NULL, 0, type);
    uint32_t bits = spirou_type_info(builder->module, type)->width;

    if(bits < 64)
        value &= (UINT64_C(1) << bits) - 1;

    ((SpirouNodeIConst*)spirou_node(builder->func, spirou_value_node(builder->func, result)))->value = value;

    return result;
}

SpirouValue spirou_build_fconst(SpirouBuilder* builder, SpirouType type, double value)
{
    SpirouValue result = build_simple(builder, SpirouOp_FConst, NULL, 0, type);

    ((SpirouNodeFConst*)spirou_node(builder->func, spirou_value_node(builder->func, result)))->value = value;

    return result;
}

SpirouValue spirou_build_binop(SpirouBuilder* builder, SpirouOp op, SpirouValue lhs, SpirouValue rhs)
{
    SpirouValue inputs[2] = { lhs, rhs };

    SPIROU_ASSERT(spirou_op_has_flag(op, SpirouOpFlags_Binary), "Binary op expected");

    return build_simple(builder, op, inputs, 2, join(builder, lhs, rhs));
}

SpirouValue spirou_build_unop(SpirouBuilder* builder, SpirouOp op, SpirouValue value)
{
    SPIROU_ASSERT(spirou_op_has_flag(op, SpirouOpFlags_Unary), "Unary op expected");

    return build_simple(builder, op, &value, 1, spirou_value_type(builder->func, value));
}

SpirouValue spirou_build_fma(SpirouBuilder* builder, SpirouValue a, SpirouValue b, SpirouValue c)
{
    SpirouValue inputs[3] = { a, b, c };
    SpirouType type = join(builder, a, b);
    bool varying = spirou_type_is_varying(builder->module, type) || is_varying(builder, c);

    return build_simple(builder, SpirouOp_FMA, inputs, 3, with_varying(builder, type, varying));
}

static SpirouType bool_like(SpirouBuilder* builder, SpirouType type)
{
    const SpirouTypeInfo* info = spirou_type_info(builder->module, type);
    SpirouType i1 = spirou_type_int(builder->module, 1);

    if(info->kind == SpirouTypeKind_Vector)
        return spirou_type_vector(builder->module, i1, info->width);

    return with_varying(builder, i1, (info->flags & SpirouTypeFlags_Varying) != 0);
}

SpirouValue spirou_build_cmp(SpirouBuilder* builder, SpirouCmp predicate, SpirouValue lhs, SpirouValue rhs)
{
    SpirouValue inputs[2] = { lhs, rhs };
    SpirouOp op = spirou_cmp_is_int(predicate) ? SpirouOp_ICmp : SpirouOp_FCmp;
    SpirouValue result = build_simple(builder, op, inputs, 2, bool_like(builder, join(builder, lhs, rhs)));

    ((SpirouNodeCmp*)spirou_node(builder->func, spirou_value_node(builder->func, result)))->predicate = predicate;

    return result;
}

SpirouValue spirou_build_select(SpirouBuilder* builder,
                                SpirouValue cond,
                                SpirouValue if_true,
                                SpirouValue if_false)
{
    SpirouValue inputs[3] = { cond, if_true, if_false };
    SpirouType type = join(builder, if_true, if_false);

    return build_simple(builder, SpirouOp_Select, inputs, 3, with_varying(builder, type, is_varying(builder, cond)));
}

SpirouValue spirou_build_cast(SpirouBuilder* builder, SpirouOp op, SpirouValue value, SpirouType type)
{
    SPIROU_ASSERT(spirou_op_has_flag(op, SpirouOpFlags_Cast), "Cast op expected");

    return build_simple(builder, op, &value, 1, with_varying(builder, type, is_varying(builder, value)));
}

SpirouValue spirou_build_ptradd(SpirouBuilder* builder, SpirouValue ptr, SpirouValue offset)
{
    SpirouValue inputs[2] = { ptr, offset };
    SpirouType type = spirou_type_uniform(builder->module, spirou_value_type(builder->func, ptr));
    bool varying = is_varying(builder, ptr) || is_varying(builder, offset);

    return build_simple(builder, SpirouOp_PtrAdd, inputs, 2, with_varying(builder, type, varying));
}

SpirouValue spirou_build_symbol_addr(SpirouBuilder* builder, SpirouSymbol symbol)
{
    SpirouValue result = build_simple(builder, SpirouOp_SymbolAddr, NULL, 0, spirou_type_ptr(builder->module, 0));

    ((SpirouNodeSymbolAddr*)spirou_node(builder->func, spirou_value_node(builder->func, result)))->symbol = symbol;

    return result;
}

SpirouValue spirou_build_stackslot(SpirouBuilder* builder, uint32_t size, uint32_t align)
{
    SpirouValue result = build_simple(builder, SpirouOp_StackSlot, NULL, 0, spirou_type_ptr(builder->module, 0));
    SpirouNodeStackSlot* slot = (SpirouNodeStackSlot*)spirou_node(builder->func, spirou_value_node(builder->func, result));

    slot->size = size;
    slot->align = align;

    return result;
}

SpirouValue spirou_build_load(SpirouBuilder* builder,
                              SpirouValue* state,
                              SpirouValue ptr,
                              SpirouType type,
                              uint32_t align)
{
    SpirouValue inputs[2] = { *state, ptr };
    SpirouType outputs[2];
    SpirouNodeId node;

    outputs[0] = spirou_value_type(builder->func, *state);
    outputs[1] = with_varying(builder, type, is_varying(builder, ptr));

    node = spirou_function_new_node(builder->func, SpirouOp_Load, builder->region, inputs, 2, outputs, 2);
    ((SpirouNodeMemory*)spirou_node(builder->func, node))->align = align;

    *state = spirou_node_output(builder->func, node, 0);

    return spirou_node_output(builder->func, node, 1);
}

void spirou_build_store(SpirouBuilder* builder,
                        SpirouValue* state,
                        SpirouValue ptr,
                        SpirouValue value,
                        uint32_t align)
{
    SpirouValue inputs[3] = { *state, ptr, value };
    SpirouType output = spirou_value_type(builder->func, *state);
    SpirouNodeId node;

    node = spirou_function_new_node(builder->func, SpirouOp_Store, builder->region, inputs, 3, &output, 1);
    ((SpirouNodeMemory*)spirou_node(builder->func, node))->align = align;

    *state = spirou_node_output(builder->func, node, 0);
}

SpirouNodeId spirou_build_call(SpirouBuilder* builder,
                               SpirouValue callee,
                               SpirouType func_type,
                               const SpirouValue* args,
                               uint32_t num_args)
{
    const SpirouTypeInfo* info = spirou_type_info(builder->module, func_type);
    SpirouNodeId node;
    uint32_t i;

    node = spirou_function_new_node(builder->func, SpirouOp_Call, builder->region, &callee, 1, NULL, 0);
    ((SpirouNodeCall*)spirou_node(builder->func, node))->func_type = func_type;

    for(i = 0; i < num_args; i++)
        spirou_node_add_input(builder->func, node, args[i]);

    for(i = 0; i < info->width; i++)
        spirou_node_add_output(builder->func, node, spirou_type_func_result(builder->module, func_type, i));

    return node;
}

SpirouNodeId spirou_build_gamma(SpirouBuilder* builder, SpirouValue predicate, uint32_t num_branches)
{
    SpirouNodeId node;
    SpirouNodeGamma* gamma;
    uint32_t i;

    node = spirou_function_new_node(builder->func, SpirouOp_Gamma, builder->region, &predicate, 1, NULL, 0);
    gamma = (SpirouNodeGamma*)spirou_node(builder->func, node);
    gamma->first_region = spirou_function_new_region(builder->func, node);
    gamma->num_regions = num_branches;

    for(i = 1; i < num_branches; i++)
        spirou_function_new_region(builder->func, node);

    return node;
}

SpirouRegionId spirou_gamma_branch(SpirouFunction* func, SpirouNodeId gamma, uint32_t branch)
{
    SPIROU_ASSERT(spirou_node(func, gamma)->op == SpirouOp_Gamma, "Gamma node expected");

    return spirou_node_subregion(func, gamma, branch);
}

uint32_t spirou_gamma_add_entry(SpirouFunction* func, SpirouNodeId gamma, SpirouValue value)
{
    SpirouNodeGamma* node = (SpirouNodeGamma*)spirou_node(func, gamma);
    SpirouType type = spirou_value_type(func, value);
    uint32_t i;

    for(i = 0; i < node->num_regions; i++)
        spirou_region_add_arg(func, node->first_region + i, type);

    return spirou_node_add_input(func, gamma, value) - 1;
}

SpirouValue spirou_gamma_add_exit(SpirouFunction* func, SpirouNodeId gamma, const SpirouValue* branch_values)
{
    SpirouNodeGamma* node = (SpirouNodeGamma*)spirou_node(func, gamma);
    SpirouModule* module = func->module;
    SpirouType type = spirou_type_uniform(module, spirou_value_type(func, branch_values[0]));
    bool varying = spirou_type_is_varying(module, spirou_value_type(func, spirou_node_input(func, gamma, 0)));
    SpirouType output;
    uint32_t i;

    for(i = 0; i < node->num_regions; i++)
    {
        spirou_region_add_result(func, node->first_region + i, branch_values[i]);
        varying |= spirou_type_is_varying(module, spirou_value_type(func, branch_values[i]));
    }

    output = varying ? spirou_type_varying(module, type) : type;

    return spirou_node_add_output(func, gamma, output == SPIROU_INVALID ? type : output);
}

SpirouNodeId spirou_build_theta(SpirouBuilder* builder)
{
    SpirouNodeId node = spirou_function_new_node(builder->func, SpirouOp_Theta, builder->region, NULL, 0, NULL, 0);
    SpirouNodeTheta* theta = (SpirouNodeTheta*)spirou_node(builder->func, node);

    theta->body = spirou_function_new_region(builder->func, node);
    spirou_region_add_result(builder->func, theta->body, SPIROU_INVALID);

    return node;
}

SpirouRegionId spirou_theta_body(SpirouFunction* func, SpirouNodeId theta)
{
    SPIROU_ASSERT(spirou_node(func, theta)->op == SpirouOp_Theta, "Theta node expected");

    return ((SpirouNodeTheta*)spirou_node(func, theta))->body;
}

uint32_t spirou_theta_add_loopvar(SpirouFunction* func, SpirouNodeId theta, SpirouValue init)
{
    SpirouRegionId body = spirou_theta_body(func, theta);
    SpirouType type = spirou_value_type(func, init);
    SpirouValue arg = spirou_region_add_arg(func, body, type);

    spirou_region_add_result(func, body, arg);
    spirou_node_add_output(func, theta, type);

    return spirou_node_add_input(func, theta, init);
}

void spirou_theta_set_next(SpirouFunction* func, SpirouNodeId theta, uint32_t index, SpirouValue next)
{
    spirou_region_set_result(func, spirou_theta_body(func, theta), index + 1, next);
}

void spirou_theta_set_predicate(SpirouFunction* func, SpirouNodeId theta, SpirouValue cont)
{
    spirou_region_set_result(func, spirou_theta_body(func, theta), 0, cont);
}

SpirouNodeId spirou_build_map(SpirouBuilder* builder, SpirouValue count, SpirouValue state)
{
    SpirouModule* module = builder->module;
    SpirouValue inputs[2] = { count, state };
    SpirouType state_type = spirou_value_type(builder->func, state);
    SpirouNodeId node;
    SpirouNodeMap* map;
    SpirouValue state_arg;

    node = spirou_function_new_node(builder->func, SpirouOp_Map, builder->region, inputs, 2, &state_type, 1);
    map = (SpirouNodeMap*)spirou_node(builder->func, node);
    map->body = spirou_function_new_region(builder->func, node);

    spirou_region_add_arg(builder->func, map->body, spirou_type_varying(module, spirou_type_int(module, 64)));
    state_arg = spirou_region_add_arg(builder->func, map->body, state_type);
    spirou_region_add_result(builder->func, map->body, state_arg);

    return node;
}

SpirouRegionId spirou_map_body(SpirouFunction* func, SpirouNodeId map)
{
    SPIROU_ASSERT(spirou_node(func, map)->op == SpirouOp_Map, "Map node expected");

    return ((SpirouNodeMap*)spirou_node(func, map))->body;
}

SpirouValue spirou_map_index(SpirouFunction* func, SpirouNodeId map)
{
    return spirou_region_arg(func, spirou_map_body(func, map), 0);
}

SpirouValue spirou_map_state(SpirouFunction* func, SpirouNodeId map)
{
    return spirou_region_arg(func, spirou_map_body(func, map), 1);
}

SpirouValue spirou_map_add_capture(SpirouFunction* func, SpirouNodeId map, SpirouValue value)
{
    SpirouValue arg = spirou_region_add_arg(func, spirou_map_body(func, map), spirou_value_type(func, value));

    spirou_node_add_input(func, map, value);

    return arg;
}

void spirou_map_set_state(SpirouFunction* func, SpirouNodeId map, SpirouValue state)
{
    spirou_region_set_result(func, spirou_map_body(func, map), 0, state);
}
