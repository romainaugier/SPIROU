/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/verify.h"

#include "libromano/memory.h"

#include <stdarg.h>

typedef struct Verifier
{
    SpirouFunction* func;
    SpirouModule* module;
    String* errors;
    uint8_t* in_map;
    uint32_t count;
} Verifier;

static void report(Verifier* v, SpirouNodeId node, const char* format, ...)
{
    char buffer[512];
    const SpirouSymbolInfo* symbol;
    va_list args;

    v->count++;

    if(v->errors == NULL)
        return;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    symbol = spirou_function_symbol(v->func);

    if(node == SPIROU_INVALID)
    {
        string_appendf(v->errors, "@%.*s: %s\n", (int)symbol->name_len, symbol->name, buffer);
    }
    else
    {
        string_appendf(v->errors,
                       "@%.*s: node %u (%s): %s\n",
                       (int)symbol->name_len,
                       symbol->name,
                       node,
                       spirou_op_info((SpirouOp)spirou_node(v->func, node)->op)->mnemonic,
                       buffer);
    }
}

static const SpirouTypeInfo* info_of(Verifier* v, SpirouValue value)
{
    return spirou_type_info(v->module, spirou_value_type(v->func, value));
}

static SpirouType uniform_of(Verifier* v, SpirouValue value)
{
    return spirou_type_uniform(v->module, spirou_value_type(v->func, value));
}

static bool varying_of(Verifier* v, SpirouValue value)
{
    return spirou_type_is_varying(v->module, spirou_value_type(v->func, value));
}

static bool is_kind(Verifier* v, SpirouValue value, SpirouTypeKind kind)
{
    return spirou_type_scalar_kind(v->module, spirou_value_type(v->func, value)) == kind;
}

static bool is_bool(Verifier* v, SpirouValue value)
{
    const SpirouTypeInfo* info = info_of(v, value);

    if(info->kind == SpirouTypeKind_Vector)
        info = spirou_type_info(v->module, info->elem);

    return info->kind == SpirouTypeKind_Int && info->width == 1;
}

static bool is_state(Verifier* v, SpirouValue value)
{
    return info_of(v, value)->kind == SpirouTypeKind_State;
}

static bool is_scalar_kind(Verifier* v, SpirouValue value, SpirouTypeKind kind)
{
    return info_of(v, value)->kind == kind;
}

/* Varying only flows from varying inputs: an output may be varying only if an input is */
static void check_uniformity(Verifier* v, SpirouNodeId node)
{
    SpirouNode* n = spirou_node(v->func, node);
    bool any_varying = false;
    uint32_t i;

    for(i = 0; i < n->num_inputs; i++)
        any_varying |= varying_of(v, spirou_node_input(v->func, node, i));

    if(spirou_op_has_flag((SpirouOp)n->op, SpirouOpFlags_Structural))
        return;

    for(i = 0; i < n->num_outputs; i++)
    {
        SpirouValue output = spirou_node_output(v->func, node, i);

        if(!varying_of(v, output) && any_varying && !is_state(v, output))
            report(v, node, "output %u is uniform but depends on varying inputs", i);
    }
}

static void check_simple(Verifier* v, SpirouNodeId node)
{
    SpirouNode* n = spirou_node(v->func, node);
    SpirouOp op = (SpirouOp)n->op;
    const SpirouOpInfo* info = spirou_op_info(op);
    SpirouValue out;
    SpirouValue in0;
    SpirouValue in1;
    uint32_t i;

    if(info->num_inputs != SPIROU_VARIADIC && n->num_inputs != info->num_inputs)
    {
        report(v, node, "expected %u inputs, got %u", info->num_inputs, n->num_inputs);
        return;
    }

    if(info->num_outputs != SPIROU_VARIADIC && n->num_outputs != info->num_outputs)
    {
        report(v, node, "expected %u outputs, got %u", info->num_outputs, n->num_outputs);
        return;
    }

    out = n->num_outputs > 0 ? spirou_node_output(v->func, node, n->num_outputs - 1) : SPIROU_INVALID;
    in0 = n->num_inputs > 0 ? spirou_node_input(v->func, node, 0) : SPIROU_INVALID;
    in1 = n->num_inputs > 1 ? spirou_node_input(v->func, node, 1) : SPIROU_INVALID;

    if(info->flags & (SpirouOpFlags_Binary | SpirouOpFlags_Unary) || op == SpirouOp_FMA)
    {
        SpirouTypeKind kind = (info->flags & SpirouOpFlags_IntOp) ? SpirouTypeKind_Int : SpirouTypeKind_Float;

        for(i = 0; i < n->num_inputs; i++)
        {
            SpirouValue input = spirou_node_input(v->func, node, i);

            if(uniform_of(v, input) != uniform_of(v, out))
                report(v, node, "input %u type does not match the output type", i);
            else if(!is_kind(v, input, kind))
                report(v, node, "input %u must be %s", i, kind == SpirouTypeKind_Int ? "an integer" : "a float");
        }

        return;
    }

    switch(op)
    {
        case SpirouOp_ICmp:
        case SpirouOp_FCmp:
        {
            SpirouCmp predicate = (SpirouCmp)((SpirouNodeCmp*)n)->predicate;
            bool int_cmp = op == SpirouOp_ICmp;

            if(uniform_of(v, in0) != uniform_of(v, in1))
                report(v, node, "operand types differ");

            if(int_cmp != spirou_cmp_is_int(predicate))
                report(v, node, "predicate %s is invalid for this comparison", spirou_cmp_mnemonics[predicate]);

            if(int_cmp && !is_kind(v, in0, SpirouTypeKind_Int) && !is_kind(v, in0, SpirouTypeKind_Ptr))
                report(v, node, "icmp operands must be integers or pointers");

            if(!int_cmp && !is_kind(v, in0, SpirouTypeKind_Float))
                report(v, node, "fcmp operands must be floats");

            if(!is_bool(v, out))
                report(v, node, "result must be i1");
            break;
        }
        case SpirouOp_Select:
            if(!is_bool(v, in0))
                report(v, node, "condition must be i1");

            if(uniform_of(v, in1) != uniform_of(v, out) ||
               uniform_of(v, spirou_node_input(v->func, node, 2)) != uniform_of(v, out))
                report(v, node, "operand types do not match the result type");
            break;
        case SpirouOp_ZExt:
        case SpirouOp_SExt:
        case SpirouOp_Trunc:
            if(!is_kind(v, in0, SpirouTypeKind_Int) || !is_kind(v, out, SpirouTypeKind_Int))
                report(v, node, "integer operand and result expected");
            else if(op == SpirouOp_Trunc ? info_of(v, out)->width >= info_of(v, in0)->width
                                         : info_of(v, out)->width <= info_of(v, in0)->width)
                report(v, node, "invalid widths for %s", info->mnemonic);
            break;
        case SpirouOp_FPExt:
        case SpirouOp_FPTrunc:
            if(!is_kind(v, in0, SpirouTypeKind_Float) || !is_kind(v, out, SpirouTypeKind_Float))
                report(v, node, "float operand and result expected");
            else if(op == SpirouOp_FPTrunc ? info_of(v, out)->width >= info_of(v, in0)->width
                                           : info_of(v, out)->width <= info_of(v, in0)->width)
                report(v, node, "invalid widths for %s", info->mnemonic);
            break;
        case SpirouOp_SIToFP:
        case SpirouOp_UIToFP:
            if(!is_kind(v, in0, SpirouTypeKind_Int) || !is_kind(v, out, SpirouTypeKind_Float))
                report(v, node, "integer operand and float result expected");
            break;
        case SpirouOp_FPToSI:
        case SpirouOp_FPToUI:
            if(!is_kind(v, in0, SpirouTypeKind_Float) || !is_kind(v, out, SpirouTypeKind_Int))
                report(v, node, "float operand and integer result expected");
            break;
        case SpirouOp_Bitcast:
            if(info_of(v, in0)->size != info_of(v, out)->size)
                report(v, node, "bitcast between types of different sizes");
            break;
        case SpirouOp_PtrToInt:
            if(!is_scalar_kind(v, in0, SpirouTypeKind_Ptr) || !is_scalar_kind(v, out, SpirouTypeKind_Int))
                report(v, node, "pointer operand and integer result expected");
            break;
        case SpirouOp_IntToPtr:
            if(!is_scalar_kind(v, in0, SpirouTypeKind_Int) || !is_scalar_kind(v, out, SpirouTypeKind_Ptr))
                report(v, node, "integer operand and pointer result expected");
            break;
        case SpirouOp_PtrAdd:
            if(!is_scalar_kind(v, in0, SpirouTypeKind_Ptr) || !is_scalar_kind(v, in1, SpirouTypeKind_Int))
                report(v, node, "pointer and integer offset expected");
            break;
        case SpirouOp_Load:
        case SpirouOp_Store:
            if(!is_state(v, in0))
                report(v, node, "input 0 must be a state");

            if(!is_scalar_kind(v, in1, SpirouTypeKind_Ptr))
                report(v, node, "input 1 must be a pointer");

            if(!is_state(v, spirou_node_output(v->func, node, 0)))
                report(v, node, "output 0 must be a state");
            break;
        case SpirouOp_Call:
        {
            SpirouType func_type = ((SpirouNodeCall*)n)->func_type;
            const SpirouTypeInfo* ftype = spirou_type_info(v->module, func_type);

            if(ftype->kind != SpirouTypeKind_Func)
            {
                report(v, node, "callee type is not a function type");
                break;
            }

            if(n->num_inputs < 1 || !is_scalar_kind(v, in0, SpirouTypeKind_Ptr))
                report(v, node, "callee must be a pointer");

            if(n->num_inputs != ftype->count + 1 || n->num_outputs != ftype->width)
            {
                report(v, node, "argument or result count does not match the function type");
                break;
            }

            for(i = 0; i < ftype->count; i++)
                if(spirou_value_type(v->func, spirou_node_input(v->func, node, i + 1)) !=
                   spirou_type_func_param(v->module, func_type, i))
                    report(v, node, "argument %u type does not match the function type", i);

            for(i = 0; i < ftype->width; i++)
                if(spirou_value_type(v->func, spirou_node_output(v->func, node, i)) !=
                   spirou_type_func_result(v->module, func_type, i))
                    report(v, node, "result %u type does not match the function type", i);
            break;
        }
        default:
            break;
    }
}

static void check_region_args_match_inputs(Verifier* v,
                                           SpirouNodeId node,
                                           SpirouRegionId region,
                                           uint32_t first_input,
                                           uint32_t first_arg)
{
    SpirouNode* n = spirou_node(v->func, node);
    SpirouRegion* r = spirou_region(v->func, region);
    uint32_t i;

    if(r->num_args - first_arg != n->num_inputs - first_input)
    {
        report(v, node, "region %u has %u arguments, expected %u", region, r->num_args - first_arg, n->num_inputs - first_input);
        return;
    }

    for(i = first_input; i < n->num_inputs; i++)
        if(spirou_value_type(v->func, spirou_region_arg(v->func, region, first_arg + i - first_input)) !=
           spirou_value_type(v->func, spirou_node_input(v->func, node, i)))
            report(v, node, "region %u argument %u type does not match input %u", region, first_arg + i - first_input, i);
}

static void check_region_results_match_outputs(Verifier* v,
                                               SpirouNodeId node,
                                               SpirouRegionId region,
                                               uint32_t first_result)
{
    SpirouNode* n = spirou_node(v->func, node);
    SpirouRegion* r = spirou_region(v->func, region);
    uint32_t i;

    if(r->num_results - first_result != n->num_outputs)
    {
        report(v, node, "region %u has %u results, expected %u", region, r->num_results - first_result, n->num_outputs);
        return;
    }

    for(i = 0; i < n->num_outputs; i++)
    {
        SpirouValue result = spirou_region_result(v->func, region, first_result + i);
        SpirouValue output = spirou_node_output(v->func, node, i);

        if(result == SPIROU_INVALID)
            continue;

        if(uniform_of(v, result) != uniform_of(v, output) || (varying_of(v, result) && !varying_of(v, output)))
            report(v, node, "region %u result %u type does not match output %u", region, first_result + i, i);
    }
}

static void check_structural(Verifier* v, SpirouNodeId node)
{
    SpirouNode* n = spirou_node(v->func, node);
    SpirouModule* module = v->module;
    uint32_t i;

    switch(n->op)
    {
        case SpirouOp_Gamma:
        {
            SpirouNodeGamma* gamma = (SpirouNodeGamma*)n;
            SpirouValue predicate;

            if(n->num_inputs < 1)
            {
                report(v, node, "missing predicate");
                return;
            }

            predicate = spirou_node_input(v->func, node, 0);

            if(!is_scalar_kind(v, predicate, SpirouTypeKind_Int))
                report(v, node, "predicate must be an integer");

            if(gamma->num_regions < 1)
                report(v, node, "gamma needs at least one branch");

            for(i = 0; i < gamma->num_regions; i++)
            {
                check_region_args_match_inputs(v, node, gamma->first_region + i, 1, 0);
                check_region_results_match_outputs(v, node, gamma->first_region + i, 0);
            }

            for(i = 0; i < n->num_outputs; i++)
                if(varying_of(v, predicate) && !varying_of(v, spirou_node_output(v->func, node, i)))
                    report(v, node, "output %u must be varying, the predicate is varying", i);
            break;
        }
        case SpirouOp_Theta:
        {
            SpirouRegionId body = ((SpirouNodeTheta*)n)->body;
            SpirouValue cont = spirou_region_result(v->func, body, 0);

            check_region_args_match_inputs(v, node, body, 0, 0);
            check_region_results_match_outputs(v, node, body, 1);

            if(cont == SPIROU_INVALID)
                report(v, node, "loop predicate is not set");
            else if(!is_scalar_kind(v, cont, SpirouTypeKind_Int) || info_of(v, cont)->width != 1)
                report(v, node, "loop predicate must be i1");

            for(i = 0; i < n->num_inputs; i++)
                if(spirou_value_type(v->func, spirou_node_input(v->func, node, i)) !=
                   spirou_value_type(v->func, spirou_node_output(v->func, node, i)))
                    report(v, node, "loop variable %u output type does not match its input", i);
            break;
        }
        case SpirouOp_Map:
        {
            SpirouRegionId body = ((SpirouNodeMap*)n)->body;
            SpirouRegion* r = spirou_region(v->func, body);
            SpirouType i64 = spirou_type_int(module, 64);

            if(n->num_inputs < 2 || r->num_args < 2)
            {
                report(v, node, "map needs a count and a state");
                return;
            }

            if(spirou_value_type(v->func, spirou_node_input(v->func, node, 0)) != i64)
                report(v, node, "count must be a uniform i64");

            if(!is_state(v, spirou_node_input(v->func, node, 1)))
                report(v, node, "input 1 must be a state");

            if(spirou_value_type(v->func, spirou_region_arg(v->func, body, 0)) != spirou_type_varying(module, i64))
                report(v, node, "index must be a varying i64");

            check_region_args_match_inputs(v, node, body, 1, 1);
            check_region_results_match_outputs(v, node, body, 0);

            for(i = 2; i < n->num_inputs; i++)
                if(varying_of(v, spirou_node_input(v->func, node, i)))
                    report(v, node, "capture %u must be uniform", i - 2);
            break;
        }
        default:
            break;
    }
}

static void check_varying_scope(Verifier* v, SpirouRegionId region, SpirouValue value, SpirouNodeId node)
{
    if(!v->in_map[region] && varying_of(v, value))
        report(v, node, "varying value %%%u used outside of a map", value);
}

static void check_region(Verifier* v, SpirouRegionId region)
{
    SpirouFunction* func = v->func;
    SpirouRegion* r = spirou_region(func, region);
    SpirouNodeId node;
    uint32_t i;

    for(i = 0; i < r->num_args; i++)
        check_varying_scope(v, region, spirou_region_arg(func, region, i), SPIROU_INVALID);

    for(i = 0; i < r->num_results; i++)
    {
        SpirouValue result = spirou_region_result(func, region, i);

        if(result == SPIROU_INVALID)
        {
            if(!(r->parent != SPIROU_INVALID && spirou_node(func, r->parent)->op == SpirouOp_Theta && i == 0))
                report(v, r->parent, "region %u result %u is not set", region, i);
        }
        else if(spirou_value_region(func, result) != region)
        {
            report(v, r->parent, "region %u result %u is defined in another region", region, i);
        }
    }

    for(node = r->first; node != SPIROU_INVALID; node = spirou_node(func, node)->next)
    {
        SpirouNode* n = spirou_node(func, node);

        if(n->region != region)
        {
            report(v, node, "node is linked in region %u but belongs to region %u", region, n->region);
            continue;
        }

        for(i = 0; i < n->num_inputs; i++)
        {
            SpirouValue input = spirou_node_input(func, node, i);

            if(input >= spirou_function_num_values(func))
            {
                report(v, node, "input %u is not a value", i);
                return;
            }

            if(spirou_value_region(func, input) != region)
                report(v, node, "input %u (%%%u) is defined in another region", i, input);

            if(spirou_value_node(func, input) != SPIROU_INVALID &&
               (spirou_node(func, spirou_value_node(func, input))->flags & SpirouNodeFlags_Dead))
                report(v, node, "input %u (%%%u) is defined by a dead node", i, input);
        }

        for(i = 0; i < n->num_outputs; i++)
            check_varying_scope(v, region, spirou_node_output(func, node, i), node);

        check_uniformity(v, node);

        if(spirou_op_has_flag((SpirouOp)n->op, SpirouOpFlags_Structural))
            check_structural(v, node);
        else
            check_simple(v, node);
    }
}

uint32_t spirou_function_verify(SpirouFunction* func, String* errors)
{
    Verifier v;
    SpirouTopo topo;
    uint32_t num_regions;
    uint32_t region;
    const SpirouTypeInfo* ftype;
    uint32_t i;

    if(spirou_function_is_import(func))
        return 0;

    v.func = func;
    v.module = func->module;
    v.errors = errors;
    v.count = 0;

    num_regions = spirou_function_num_regions(func);
    v.in_map = (uint8_t*)romano_calloc(num_regions + 1, 1);

    /* Regions are created after their parent node, so parents come first */
    for(region = 0; region < num_regions; region++)
    {
        SpirouRegion* r = spirou_region(func, region);

        if(r->parent == SPIROU_INVALID)
            continue;

        SpirouNode* parent = spirou_node(func, r->parent);
        v.in_map[region] = parent->op == SpirouOp_Map || v.in_map[parent->region];
    }

    ftype = spirou_type_info(func->module, func->type);

    if(spirou_region(func, func->body)->num_results != ftype->width)
    {
        report(&v, SPIROU_INVALID, "function returns %u values, its type has %u results",
               spirou_region(func, func->body)->num_results, (uint32_t)ftype->width);
    }
    else
    {
        for(i = 0; i < ftype->width; i++)
        {
            SpirouValue result = spirou_region_result(func, func->body, i);

            if(result != SPIROU_INVALID &&
               spirou_value_type(func, result) != spirou_type_func_result(func->module, func->type, i))
                report(&v, SPIROU_INVALID, "result %u type does not match the function type", i);
        }
    }

    for(region = 0; region < num_regions; region++)
        check_region(&v, region);

    if(!spirou_topo_compute(func, &topo))
        report(&v, SPIROU_INVALID, "the graph has a cycle");

    spirou_topo_release(&topo);
    romano_free(v.in_map);

    return v.count;
}

uint32_t spirou_module_verify(SpirouModule* module, String* errors)
{
    uint32_t count = 0;
    uint32_t i;

    for(i = 0; i < spirou_module_num_functions(module); i++)
        count += spirou_function_verify(spirou_module_function(module, i), errors);

    return count;
}
