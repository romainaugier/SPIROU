/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "test_common.h"

/* sum(n) = 0 + 1 + ... + (n - 1), with a tail-controlled loop */
static void test_theta_sum(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i64 = spirou_type_int(module, 64);
    SpirouType type = spirou_type_func(module, &i64, 1, &i64, 1);
    SpirouFunction* func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("sum"), type, SpirouLinkage_Export);
    SpirouBuilder b;
    SpirouNodeId loop;
    SpirouValue zero, one, i, acc, n, next_i, next_acc, result;
    String text;

    spirou_builder_init(&b, func);

    zero = spirou_build_iconst(&b, i64, 0);
    loop = spirou_build_theta(&b);
    spirou_theta_add_loopvar(func, loop, zero);
    spirou_theta_add_loopvar(func, loop, zero);
    spirou_theta_add_loopvar(func, loop, spirou_builder_param(&b, 0));

    spirou_builder_set_region(&b, spirou_theta_body(func, loop));
    i = spirou_region_arg(func, spirou_theta_body(func, loop), 0);
    acc = spirou_region_arg(func, spirou_theta_body(func, loop), 1);
    n = spirou_region_arg(func, spirou_theta_body(func, loop), 2);
    one = spirou_build_iconst(&b, i64, 1);
    next_acc = spirou_build_binop(&b, SpirouOp_Add, acc, i);
    next_i = spirou_build_binop(&b, SpirouOp_Add, i, one);
    spirou_theta_set_next(func, loop, 0, next_i);
    spirou_theta_set_next(func, loop, 1, next_acc);
    spirou_theta_set_predicate(func, loop, spirou_build_cmp(&b, SpirouCmp_SLt, next_i, n));

    result = spirou_node_output(func, loop, 1);
    spirou_builder_return(&b, &result, 1);

    CHECK_VERIFIES(func);

    text = print_function(func);
    CHECK_TEXT(text,
               "export func @sum(%0: i64) -> (i64) {\n"
               "  %1 = iconst i64 0\n"
               "  %3, %5, %7 = theta %1, %1, %0 (%2: i64, %4: i64, %6: i64) {\n"
               "    %8 = iconst i64 1\n"
               "    %9 = add i64 %4, %2\n"
               "    %10 = add i64 %2, %8\n"
               "    %11 = icmp slt %10, %6\n"
               "    yield %11, %10, %9, %6\n"
               "  }\n"
               "  return %5\n"
               "}\n");

    string_free(text);
    spirou_module_free(module);
}

/* max(a, b) with a two-way gamma on a comparison */
static void test_gamma_max(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType params[2] = { i32, i32 };
    SpirouType type = spirou_type_func(module, params, 2, &i32, 1);
    SpirouFunction* func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("max"), type, SpirouLinkage_Internal);
    SpirouBuilder b;
    SpirouNodeId gamma;
    SpirouValue a, c, cond, exits[2], result;
    String text;

    spirou_builder_init(&b, func);
    a = spirou_builder_param(&b, 0);
    c = spirou_builder_param(&b, 1);
    cond = spirou_build_cmp(&b, SpirouCmp_SGt, a, c);
    cond = spirou_build_cast(&b, SpirouOp_ZExt, cond, i32);

    gamma = spirou_build_gamma(&b, cond, 2);
    spirou_gamma_add_entry(func, gamma, a);
    spirou_gamma_add_entry(func, gamma, c);

    exits[0] = spirou_region_arg(func, spirou_gamma_branch(func, gamma, 0), 1);
    exits[1] = spirou_region_arg(func, spirou_gamma_branch(func, gamma, 1), 0);
    result = spirou_gamma_add_exit(func, gamma, exits);
    spirou_builder_return(&b, &result, 1);

    CHECK_VERIFIES(func);

    text = print_function(func);
    CHECK_TEXT(text,
               "func @max(%0: i32, %1: i32) -> (i32) {\n"
               "  %2 = icmp sgt %0, %1\n"
               "  %3 = zext i32 %2\n"
               "  %8 = gamma %3, %0, %1 {\n"
               "  case 0 (%4: i32, %6: i32):\n"
               "    yield %6\n"
               "  case 1 (%5: i32, %7: i32):\n"
               "    yield %5\n"
               "  }\n"
               "  return %8\n"
               "}\n");

    string_free(text);
    spirou_module_free(module);
}

/* saxpy(n, a, x, y, state): y[i] = a * x[i] + y[i] for i in [0, n), as a data-parallel map */
static void test_map_saxpy(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i64 = spirou_type_int(module, 64);
    SpirouType f32 = spirou_type_float(module, 32);
    SpirouType ptr = spirou_type_ptr(module, 0);
    SpirouType state_type = spirou_type_state(module);
    SpirouType params[5] = { i64, f32, ptr, ptr, state_type };
    SpirouType type = spirou_type_func(module, params, 5, &state_type, 1);
    SpirouFunction* func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("saxpy"), type, SpirouLinkage_Export);
    SpirouBuilder b;
    SpirouNodeId map;
    SpirouValue a, x, y, index, offset, xp, yp, xv, yv, state, result;
    String text;

    spirou_builder_init(&b, func);
    map = spirou_build_map(&b, spirou_builder_param(&b, 0), spirou_builder_param(&b, 4));
    a = spirou_map_add_capture(func, map, spirou_builder_param(&b, 1));
    x = spirou_map_add_capture(func, map, spirou_builder_param(&b, 2));
    y = spirou_map_add_capture(func, map, spirou_builder_param(&b, 3));

    spirou_builder_set_region(&b, spirou_map_body(func, map));
    index = spirou_map_index(func, map);
    state = spirou_map_state(func, map);
    offset = spirou_build_binop(&b, SpirouOp_Mul, index, spirou_build_iconst(&b, i64, 4));
    xp = spirou_build_ptradd(&b, x, offset);
    yp = spirou_build_ptradd(&b, y, offset);
    xv = spirou_build_load(&b, &state, xp, f32, 4);
    yv = spirou_build_load(&b, &state, yp, f32, 4);
    spirou_build_store(&b, &state, yp, spirou_build_fma(&b, a, xv, yv), 4);
    spirou_map_set_state(func, map, state);

    CHECK(spirou_type_is_varying(module, spirou_value_type(func, xv)));
    CHECK(spirou_type_is_varying(module, spirou_value_type(func, offset)));
    CHECK(!spirou_type_is_varying(module, spirou_value_type(func, a)));

    result = spirou_node_output(func, map, 0);
    spirou_builder_return(&b, &result, 1);

    CHECK_VERIFIES(func);

    text = print_function(func);
    CHECK_TEXT(text,
               "export func @saxpy(%0: i64, %1: f32, %2: ptr, %3: ptr, %4: state) -> (state) {\n"
               "  %5 = map %0, %4, %1, %2, %3 (%6: varying i64, %7: state, %8: f32, %9: ptr, %10: ptr) {\n"
               "    %11 = iconst i64 4\n"
               "    %12 = mul varying i64 %6, %11\n"
               "    %13 = ptradd varying ptr %9, %12\n"
               "    %14 = ptradd varying ptr %10, %12\n"
               "    %15, %16 = load varying f32 %7, %13, align 4\n"
               "    %17, %18 = load varying f32 %15, %14, align 4\n"
               "    %19 = fma varying f32 %8, %16, %18\n"
               "    %20 = store %17, %14, %19, align 4\n"
               "    yield %20\n"
               "  }\n"
               "  return %5\n"
               "}\n");

    string_free(text);
    spirou_module_free(module);
}

/* Calling an imported function through its address */
static void test_call_import(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType ptr = spirou_type_ptr(module, 0);
    SpirouType state_type = spirou_type_state(module);
    SpirouType puts_params[2] = { ptr, state_type };
    SpirouType puts_results[2] = { i32, state_type };
    SpirouType puts_type = spirou_type_func(module, puts_params, 2, puts_results, 2);
    SpirouType main_type = spirou_type_func(module, &state_type, 1, puts_results, 2);
    SpirouSymbol message = spirou_module_add_global(module,
                                                    ROMANO_STRLIT_AND_SZ("message"),
                                                    spirou_type_array(module, spirou_type_int(module, 8), 6),
                                                    1,
                                                    SpirouGlobalFlags_Constant,
                                                    "hello",
                                                    6,
                                                    SpirouLinkage_Internal);
    SpirouFunction* puts_func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("puts"), puts_type, SpirouLinkage_Import);
    SpirouFunction* func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("main"), main_type, SpirouLinkage_Export);
    SpirouBuilder b;
    SpirouNodeId call;
    SpirouValue args[2], results[2];
    String text = string_new("");

    CHECK(spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("puts"), puts_type, SpirouLinkage_Import) == NULL);
    CHECK(spirou_function_is_import(puts_func));

    spirou_builder_init(&b, func);
    args[0] = spirou_build_symbol_addr(&b, message);
    args[1] = spirou_builder_param(&b, 0);
    call = spirou_build_call(&b, spirou_build_symbol_addr(&b, puts_func->symbol), puts_type, args, 2);
    results[0] = spirou_node_output(func, call, 0);
    results[1] = spirou_node_output(func, call, 1);
    spirou_builder_return(&b, results, 2);

    CHECK_EQ_UINT(spirou_module_verify(module, NULL), 0);

    spirou_module_print(module, &text);
    CHECK_TEXT(text,
               "global @message [6 x i8], align 1, constant, init 6 bytes\n"
               "\n"
               "import func @puts fn(ptr, state) -> (i32, state)\n"
               "\n"
               "export func @main(%0: state) -> (i32, state) {\n"
               "  %1 = addr @message\n"
               "  %2 = addr @puts\n"
               "  %3, %4 = call %2(%1, %0) : fn(ptr, state) -> (i32, state)\n"
               "  return %3, %4\n"
               "}\n");

    string_free(text);
    spirou_module_free(module);
}

/* Replacing a value forwards every use without touching the users */
static void test_replace(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType type = spirou_type_func(module, &i32, 1, &i32, 1);
    SpirouFunction* func = spirou_module_add_function(module, ROMANO_STRLIT_AND_SZ("f"), type, SpirouLinkage_Internal);
    SpirouBuilder b;
    SpirouValue x, zero, sum, product;

    spirou_builder_init(&b, func);
    x = spirou_builder_param(&b, 0);
    zero = spirou_build_iconst(&b, i32, 0);
    sum = spirou_build_binop(&b, SpirouOp_Add, x, zero);
    product = spirou_build_binop(&b, SpirouOp_Mul, sum, sum);
    spirou_builder_return(&b, &product, 1);

    spirou_value_replace(func, sum, x);

    CHECK(spirou_node_input(func, spirou_value_node(func, product), 0) == x);
    CHECK(spirou_node_input(func, spirou_value_node(func, product), 1) == x);
    CHECK_EQ_UINT(spirou_function_dce(func), 2);
    CHECK_VERIFIES(func);

    spirou_module_free(module);
}

int main(void)
{
    test_theta_sum();
    test_gamma_max();
    test_map_saxpy();
    test_call_import();
    test_replace();

    return TEST_RESULT();
}
