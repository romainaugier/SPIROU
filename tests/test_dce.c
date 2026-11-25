/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "test_common.h"

/* An unused load is dead even though it touches memory, a store reaching the result is live */
static void test_memory(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType ptr = spirou_type_ptr(module, 0);
    SpirouType state_type = spirou_type_state(module);
    SpirouType params[2] = { ptr, state_type };
    SpirouFunction* func = spirou_module_add_function(module,
                                                     ROMANO_STRLIT_AND_SZ("f"),
                                                     spirou_type_func(module, params, 2, &state_type, 1),
                                                     SpirouLinkage_Export);
    SpirouBuilder b;
    SpirouValue state, dead_state, p;
    String text;

    spirou_builder_init(&b, func);
    p = spirou_builder_param(&b, 0);
    state = spirou_builder_param(&b, 1);

    dead_state = state;
    spirou_build_load(&b, &dead_state, p, i32, 0);
    spirou_build_binop(&b, SpirouOp_Mul, spirou_build_iconst(&b, i32, 3), spirou_build_iconst(&b, i32, 5));
    spirou_build_store(&b, &state, p, spirou_build_iconst(&b, i32, 42), 0);
    spirou_builder_return(&b, &state, 1);

    CHECK_EQ_UINT(spirou_function_dce(func), 4);
    CHECK_EQ_UINT(spirou_function_dce(func), 0);
    CHECK_VERIFIES(func);

    text = print_function(func);
    CHECK_TEXT(text,
               "export func @f(%0: ptr, %1: state) -> (state) {\n"
               "  %7 = iconst i32 42\n"
               "  %8 = store %1, %0, %7\n"
               "  return %8\n"
               "}\n");

    string_free(text);
    spirou_module_free(module);
}

/* Nodes inside a live structural node are traced through its region results */
static void test_nested(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouFunction* func = spirou_module_add_function(module,
                                                     ROMANO_STRLIT_AND_SZ("f"),
                                                     spirou_type_func(module, &i32, 1, &i32, 1),
                                                     SpirouLinkage_Export);
    SpirouBuilder b;
    SpirouNodeId gamma, dead_gamma;
    SpirouValue x, exits[2], result;
    uint32_t i;

    spirou_builder_init(&b, func);
    x = spirou_builder_param(&b, 0);

    dead_gamma = spirou_build_gamma(&b, x, 2);
    spirou_gamma_add_entry(func, dead_gamma, x);

    gamma = spirou_build_gamma(&b, x, 2);
    spirou_gamma_add_entry(func, gamma, x);

    for(i = 0; i < 2; i++)
    {
        SpirouRegionId branch = spirou_gamma_branch(func, gamma, i);
        SpirouValue arg = spirou_region_arg(func, branch, 0);

        spirou_builder_set_region(&b, branch);
        spirou_build_binop(&b, SpirouOp_Sub, arg, arg);
        exits[i] = spirou_build_binop(&b, SpirouOp_Add, arg, spirou_build_iconst(&b, i32, i + 1));
    }

    result = spirou_gamma_add_exit(func, gamma, exits);
    spirou_builder_set_region(&b, func->body);
    spirou_builder_return(&b, &result, 1);

    CHECK_EQ_UINT(spirou_function_dce(func), 3);
    CHECK(spirou_node(func, dead_gamma)->flags & SpirouNodeFlags_Dead);
    CHECK(!(spirou_node(func, gamma)->flags & SpirouNodeFlags_Dead));
    CHECK_VERIFIES(func);

    spirou_module_free(module);
}

int main(void)
{
    test_memory();
    test_nested();

    return TEST_RESULT();
}
