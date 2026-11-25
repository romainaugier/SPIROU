/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/print.h"

#include <inttypes.h>

static void print_indent(String* out, uint32_t depth)
{
    uint32_t i;

    for(i = 0; i < depth; i++)
        string_appendc(out, "  ");
}

static void print_value(SpirouFunction* func, SpirouValue value, String* out)
{
    if(value == SPIROU_INVALID)
        string_appendc(out, "<none>");
    else
        string_appendf(out, "%%%u", spirou_value_resolve(func, value));
}

static void print_symbol(SpirouModule* module, SpirouSymbol symbol, String* out)
{
    const SpirouSymbolInfo* info = spirou_module_symbol(module, symbol);

    string_appendf(out, "@%.*s", (int)info->name_len, info->name);
}

static void print_type(SpirouFunction* func, SpirouType type, String* out)
{
    spirou_type_print(func->module, type, out);
}

static void print_inputs(SpirouFunction* func, SpirouNodeId node, uint32_t first, String* out)
{
    SpirouNode* n = spirou_node(func, node);
    uint32_t i;

    for(i = first; i < n->num_inputs; i++)
    {
        if(i > first)
            string_appendc(out, ", ");

        print_value(func, spirou_node_input(func, node, i), out);
    }
}

static void print_region_args(SpirouFunction* func, SpirouRegionId region, uint32_t first, String* out)
{
    SpirouRegion* r = spirou_region(func, region);
    uint32_t i;

    string_appendc(out, "(");

    for(i = first; i < r->num_args; i++)
    {
        SpirouValue arg = spirou_region_arg(func, region, i);

        if(i > first)
            string_appendc(out, ", ");

        print_value(func, arg, out);
        string_appendc(out, ": ");
        print_type(func, spirou_value_type(func, arg), out);
    }

    string_appendc(out, ")");
}

static void print_region_results(SpirouFunction* func, SpirouRegionId region, const char* keyword, String* out)
{
    SpirouRegion* r = spirou_region(func, region);
    uint32_t i;

    string_appendc(out, keyword);

    for(i = 0; i < r->num_results; i++)
    {
        string_appendc(out, i == 0 ? " " : ", ");
        print_value(func, spirou_region_result(func, region, i), out);
    }

    string_appendc(out, "\n");
}

static void print_region(SpirouFunction* func, SpirouTopo* topo, SpirouRegionId region, uint32_t depth, String* out);

static void print_node(SpirouFunction* func, SpirouTopo* topo, SpirouNodeId node, uint32_t depth, String* out)
{
    SpirouNode* n = spirou_node(func, node);
    SpirouOp op = (SpirouOp)n->op;
    uint32_t i;

    print_indent(out, depth);

    for(i = 0; i < n->num_outputs; i++)
    {
        if(i > 0)
            string_appendc(out, ", ");

        print_value(func, spirou_node_output(func, node, i), out);
    }

    if(n->num_outputs > 0)
        string_appendc(out, " = ");

    string_appendc(out, spirou_op_info(op)->mnemonic);

    switch(op)
    {
        case SpirouOp_IConst:
        {
            SpirouValue output = spirou_node_output(func, node, 0);
            uint32_t bits = spirou_type_info(func->module, spirou_value_type(func, output))->width;
            uint64_t value = ((SpirouNodeIConst*)n)->value;
            int64_t sign_extended = bits < 64 && (value >> (bits - 1)) & 1
                                        ? (int64_t)(value | ~((UINT64_C(1) << bits) - 1))
                                        : (int64_t)value;

            string_appendc(out, " ");
            print_type(func, spirou_value_type(func, output), out);
            string_appendf(out, " %" PRId64 "\n", bits == 1 ? (int64_t)value : sign_extended);
            return;
        }
        case SpirouOp_FConst:
            string_appendc(out, " ");
            print_type(func, spirou_value_type(func, spirou_node_output(func, node, 0)), out);
            string_appendf(out, " %.17g\n", ((SpirouNodeFConst*)n)->value);
            return;
        case SpirouOp_ICmp:
        case SpirouOp_FCmp:
            string_appendf(out, " %s ", spirou_cmp_mnemonics[((SpirouNodeCmp*)n)->predicate]);
            print_inputs(func, node, 0, out);
            string_appendc(out, "\n");
            return;
        case SpirouOp_SymbolAddr:
            string_appendc(out, " ");
            print_symbol(func->module, ((SpirouNodeSymbolAddr*)n)->symbol, out);
            string_appendc(out, "\n");
            return;
        case SpirouOp_StackSlot:
            string_appendf(out,
                           " %u, align %u\n",
                           ((SpirouNodeStackSlot*)n)->size,
                           ((SpirouNodeStackSlot*)n)->align);
            return;
        case SpirouOp_Load:
        case SpirouOp_Store:
            if(op == SpirouOp_Load)
            {
                string_appendc(out, " ");
                print_type(func, spirou_value_type(func, spirou_node_output(func, node, 1)), out);
            }

            string_appendc(out, " ");
            print_inputs(func, node, 0, out);

            if(((SpirouNodeMemory*)n)->align != 0)
                string_appendf(out, ", align %u", ((SpirouNodeMemory*)n)->align);

            if(((SpirouNodeMemory*)n)->flags & SpirouMemoryFlags_Volatile)
                string_appendc(out, ", volatile");

            string_appendc(out, "\n");
            return;
        case SpirouOp_Call:
            string_appendc(out, " ");
            print_value(func, spirou_node_input(func, node, 0), out);
            string_appendc(out, "(");
            print_inputs(func, node, 1, out);
            string_appendc(out, ") : ");
            print_type(func, ((SpirouNodeCall*)n)->func_type, out);
            string_appendc(out, "\n");
            return;
        case SpirouOp_Gamma:
            string_appendc(out, " ");
            print_inputs(func, node, 0, out);
            string_appendc(out, " {\n");

            for(i = 0; i < ((SpirouNodeGamma*)n)->num_regions; i++)
            {
                SpirouRegionId branch = spirou_node_subregion(func, node, i);

                print_indent(out, depth);
                string_appendf(out, "case %u ", i);
                print_region_args(func, branch, 0, out);
                string_appendc(out, ":\n");
                print_region(func, topo, branch, depth + 1, out);
                print_indent(out, depth + 1);
                print_region_results(func, branch, "yield", out);
            }

            print_indent(out, depth);
            string_appendc(out, "}\n");
            return;
        case SpirouOp_Theta:
        case SpirouOp_Map:
        {
            SpirouRegionId body = spirou_node_subregion(func, node, 0);

            string_appendc(out, " ");
            print_inputs(func, node, 0, out);
            string_appendc(out, n->num_inputs > 0 ? " " : "");
            print_region_args(func, body, 0, out);
            string_appendc(out, " {\n");
            print_region(func, topo, body, depth + 1, out);
            print_indent(out, depth + 1);
            print_region_results(func, body, "yield", out);
            print_indent(out, depth);
            string_appendc(out, "}\n");
            return;
        }
        default:
            break;
    }

    if(n->num_outputs == 1)
    {
        string_appendc(out, " ");
        print_type(func, spirou_value_type(func, spirou_node_output(func, node, 0)), out);
    }

    if(n->num_inputs > 0)
    {
        string_appendc(out, " ");
        print_inputs(func, node, 0, out);
    }

    string_appendc(out, "\n");
}

static void print_region(SpirouFunction* func, SpirouTopo* topo, SpirouRegionId region, uint32_t depth, String* out)
{
    SpirouSpan span = spirou_topo_region(topo, region);
    uint32_t i;

    for(i = 0; i < span.count; i++)
        print_node(func, topo, spirou_topo_node(topo, span.offset + i), depth, out);
}

static const char* linkage_prefix(SpirouLinkage linkage)
{
    switch(linkage)
    {
        case SpirouLinkage_Export:
            return "export ";
        case SpirouLinkage_Import:
            return "import ";
        default:
            return "";
    }
}

void spirou_function_print(SpirouFunction* func, String* out)
{
    const SpirouSymbolInfo* symbol = spirou_function_symbol(func);
    const SpirouTypeInfo* type = spirou_type_info(func->module, func->type);
    SpirouTopo topo;
    uint32_t i;

    string_appendf(out, "%sfunc ", linkage_prefix((SpirouLinkage)symbol->linkage));
    print_symbol(func->module, func->symbol, out);

    if(spirou_function_is_import(func))
    {
        string_appendc(out, " ");
        print_type(func, func->type, out);
        string_appendc(out, "\n");
        return;
    }

    print_region_args(func, func->body, 0, out);
    string_appendc(out, " -> (");

    for(i = 0; i < type->width; i++)
    {
        if(i > 0)
            string_appendc(out, ", ");

        print_type(func, spirou_type_func_result(func->module, func->type, i), out);
    }

    string_appendc(out, ") {\n");

    spirou_topo_compute(func, &topo);
    print_region(func, &topo, func->body, 1, out);
    spirou_topo_release(&topo);

    print_indent(out, 1);
    print_region_results(func, func->body, "return", out);
    string_appendc(out, "}\n");
}

void spirou_module_print(SpirouModule* module, String* out)
{
    uint32_t i;

    for(i = 0; i < (uint32_t)vector_size(&module->globals); i++)
    {
        SpirouGlobal* global = (SpirouGlobal*)vector_at(&module->globals, i);
        const SpirouSymbolInfo* symbol = spirou_module_symbol(module, global->symbol);

        string_appendf(out, "%sglobal ", linkage_prefix((SpirouLinkage)symbol->linkage));
        print_symbol(module, global->symbol, out);
        string_appendc(out, " ");
        spirou_type_print(module, global->type, out);
        string_appendf(out, ", align %u", global->align);

        if(global->flags & SpirouGlobalFlags_Constant)
            string_appendc(out, ", constant");

        if(global->init_size > 0)
            string_appendf(out, ", init %u bytes", global->init_size);

        string_appendc(out, "\n");
    }

    for(i = 0; i < spirou_module_num_functions(module); i++)
    {
        if(i > 0 || vector_size(&module->globals) > 0)
            string_appendc(out, "\n");

        spirou_function_print(spirou_module_function(module, i), out);
    }
}
