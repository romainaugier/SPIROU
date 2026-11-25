/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "test_common.h"

typedef struct Fixture
{
    SpirouModule* module;
    SpirouFunction* func;
    SpirouBuilder b;
    SpirouType i32;
    SpirouType i64;
} Fixture;

static void fixture_init(Fixture* f)
{
    SpirouType params[2];

    f->module = spirou_module_new(8);
    f->i32 = spirou_type_int(f->module, 32);
    f->i64 = spirou_type_int(f->module, 64);
    params[0] = f->i32;
    params[1] = f->i64;
    f->func = spirou_module_add_function(f->module,
                                         ROMANO_STRLIT_AND_SZ("f"),
                                         spirou_type_func(f->module, params, 2, &f->i32, 1),
                                         SpirouLinkage_Internal);
    spirou_builder_init(&f->b, f->func);
}

static void expect_error(const char* file, int line, Fixture* f, const char* message)
{
    String errors = string_new("");
    uint32_t count = spirou_function_verify(f->func, &errors);

    if(count == 0 || strstr(errors, message) == NULL)
    {
        fprintf(stderr, "%s:%d: expected an error containing \"%s\", got %u errors:\n%s", file, line, message, count, errors);
        test_failures++;
    }

    string_free(errors);
    spirou_module_free(f->module);
}

#define EXPECT_ERROR(f, message) expect_error(__FILE__, __LINE__, (f), (message))

static void test_type_mismatch(void)
{
    Fixture f;
    SpirouValue sum;

    fixture_init(&f);
    sum = spirou_build_binop(&f.b, SpirouOp_Add, spirou_builder_param(&f.b, 0), spirou_builder_param(&f.b, 1));
    spirou_builder_return(&f.b, &sum, 1);

    EXPECT_ERROR(&f, "input 1 type does not match");
}

static void test_float_op_on_int(void)
{
    Fixture f;
    SpirouValue x = SPIROU_INVALID;

    fixture_init(&f);
    x = spirou_builder_param(&f.b, 0);
    x = spirou_build_binop(&f.b, SpirouOp_FAdd, x, x);
    spirou_builder_return(&f.b, &x, 1);

    EXPECT_ERROR(&f, "must be a float");
}

static void test_cross_region_use(void)
{
    Fixture f;
    SpirouNodeId loop;
    SpirouValue x;

    fixture_init(&f);
    x = spirou_builder_param(&f.b, 0);
    loop = spirou_build_theta(&f.b);
    spirou_theta_add_loopvar(f.func, loop, x);
    spirou_builder_set_region(&f.b, spirou_theta_body(f.func, loop));
    spirou_theta_set_next(f.func, loop, 0, spirou_build_binop(&f.b, SpirouOp_Add, x, x));
    spirou_theta_set_predicate(f.func, loop, spirou_build_iconst(&f.b, spirou_type_int(f.module, 1), 0));
    x = spirou_node_output(f.func, loop, 0);
    spirou_builder_return(&f.b, &x, 1);

    EXPECT_ERROR(&f, "defined in another region");
}

static void test_missing_predicate(void)
{
    Fixture f;
    SpirouNodeId loop;
    SpirouValue x;

    fixture_init(&f);
    loop = spirou_build_theta(&f.b);
    spirou_theta_add_loopvar(f.func, loop, spirou_builder_param(&f.b, 0));
    x = spirou_node_output(f.func, loop, 0);
    spirou_builder_return(&f.b, &x, 1);

    EXPECT_ERROR(&f, "loop predicate is not set");
}

static void test_varying_outside_map(void)
{
    Fixture f;
    SpirouValue x;

    fixture_init(&f);
    x = spirou_build_undef(&f.b, spirou_type_varying(f.module, f.i32));
    x = spirou_build_binop(&f.b, SpirouOp_Add, x, spirou_builder_param(&f.b, 0));
    spirou_builder_return(&f.b, &x, 1);

    EXPECT_ERROR(&f, "used outside of a map");
}

static void test_cycle(void)
{
    Fixture f;
    SpirouValue x, a, c;

    fixture_init(&f);
    x = spirou_builder_param(&f.b, 0);
    a = spirou_build_binop(&f.b, SpirouOp_Add, x, x);
    c = spirou_build_binop(&f.b, SpirouOp_Mul, a, a);
    spirou_value_replace(f.func, a, c);
    spirou_builder_return(&f.b, &c, 1);

    EXPECT_ERROR(&f, "cycle");
}

static void test_result_count(void)
{
    Fixture f;

    fixture_init(&f);

    EXPECT_ERROR(&f, "function returns 0 values");
}

static void test_map_capture_must_be_uniform(void)
{
    Fixture f;
    SpirouType state_type;
    SpirouType params[2];
    SpirouNodeId outer, inner;
    SpirouValue result;

    f.module = spirou_module_new(8);
    f.i64 = spirou_type_int(f.module, 64);
    state_type = spirou_type_state(f.module);
    params[0] = f.i64;
    params[1] = state_type;
    f.func = spirou_module_add_function(f.module,
                                        ROMANO_STRLIT_AND_SZ("f"),
                                        spirou_type_func(f.module, params, 2, &state_type, 1),
                                        SpirouLinkage_Internal);
    spirou_builder_init(&f.b, f.func);

    outer = spirou_build_map(&f.b, spirou_builder_param(&f.b, 0), spirou_builder_param(&f.b, 1));
    spirou_builder_set_region(&f.b, spirou_map_body(f.func, outer));
    inner = spirou_build_map(&f.b, spirou_build_iconst(&f.b, f.i64, 4), spirou_map_state(f.func, outer));
    spirou_map_add_capture(f.func, inner, spirou_map_index(f.func, outer));
    spirou_map_set_state(f.func, outer, spirou_node_output(f.func, inner, 0));

    spirou_builder_set_region(&f.b, f.func->body);
    result = spirou_node_output(f.func, outer, 0);
    spirou_builder_return(&f.b, &result, 1);

    EXPECT_ERROR(&f, "capture 0 must be uniform");
}

int main(void)
{
    test_type_mismatch();
    test_float_op_on_int();
    test_cross_region_use();
    test_missing_predicate();
    test_varying_outside_map();
    test_cycle();
    test_result_count();
    test_map_capture_must_be_uniform();

    return TEST_RESULT();
}
