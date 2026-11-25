/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "test_common.h"

static void test_interning(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType f32 = spirou_type_float(module, 32);
    SpirouType fields[2] = { i32, f32 };

    CHECK(i32 == spirou_type_int(module, 32));
    CHECK(i32 != spirou_type_int(module, 64));
    CHECK(i32 != f32);
    CHECK(spirou_type_vector(module, f32, 4) == spirou_type_vector(module, f32, 4));
    CHECK(spirou_type_struct(module, fields, 2, false) == spirou_type_struct(module, fields, 2, false));
    CHECK(spirou_type_struct(module, fields, 2, false) != spirou_type_struct(module, fields, 2, true));
    CHECK(spirou_type_func(module, fields, 2, &i32, 1) == spirou_type_func(module, fields, 2, &i32, 1));
    CHECK(spirou_type_func(module, fields, 2, &i32, 1) != spirou_type_func(module, fields, 1, &i32, 1));

    spirou_module_free(module);
}

static void test_varying(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType f32 = spirou_type_float(module, 32);
    SpirouType vf32 = spirou_type_varying(module, f32);

    CHECK(vf32 != f32);
    CHECK(spirou_type_is_varying(module, vf32));
    CHECK(!spirou_type_is_varying(module, f32));
    CHECK(spirou_type_uniform(module, vf32) == f32);
    CHECK(spirou_type_varying(module, vf32) == vf32);
    CHECK(spirou_type_varying(module, spirou_type_state(module)) == SPIROU_INVALID);
    CHECK(spirou_type_varying(module, spirou_type_vector(module, f32, 4)) == SPIROU_INVALID);

    spirou_module_free(module);
}

static void test_layout(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i8 = spirou_type_int(module, 8);
    SpirouType i1 = spirou_type_int(module, 1);
    SpirouType i64 = spirou_type_int(module, 64);
    SpirouType f32 = spirou_type_float(module, 32);
    SpirouType fields[3] = { i8, i64, f32 };
    SpirouType s = spirou_type_struct(module, fields, 3, false);
    SpirouType p = spirou_type_struct(module, fields, 3, true);

    CHECK_EQ_UINT(spirou_type_info(module, i1)->size, 1);
    CHECK_EQ_UINT(spirou_type_info(module, s)->size, 24);
    CHECK_EQ_UINT(spirou_type_info(module, s)->align, 8);
    CHECK_EQ_UINT(spirou_type_field_offset(module, s, 1), 8);
    CHECK_EQ_UINT(spirou_type_field_offset(module, s, 2), 16);
    CHECK_EQ_UINT(spirou_type_info(module, p)->size, 13);
    CHECK_EQ_UINT(spirou_type_field_offset(module, p, 2), 9);
    CHECK_EQ_UINT(spirou_type_info(module, spirou_type_array(module, f32, 10))->size, 40);
    CHECK_EQ_UINT(spirou_type_info(module, spirou_type_vector(module, f32, 3))->size, 16);
    CHECK_EQ_UINT(spirou_type_info(module, spirou_type_ptr(module, 0))->size, 8);

    spirou_module_free(module);
}

static void test_print(void)
{
    SpirouModule* module = spirou_module_new(8);
    SpirouType i32 = spirou_type_int(module, 32);
    SpirouType f32 = spirou_type_float(module, 32);
    SpirouType state = spirou_type_state(module);
    SpirouType params[3] = { i32, spirou_type_ptr(module, 1), state };
    SpirouType results[2] = { spirou_type_vector(module, f32, 8), state };
    SpirouType fields[2] = { i32, spirou_type_array(module, f32, 4) };
    String out = string_new("");

    spirou_type_print(module, spirou_type_func(module, params, 3, results, 2), &out);
    string_appendc(&out, " ");
    spirou_type_print(module, spirou_type_struct(module, fields, 2, true), &out);
    string_appendc(&out, " ");
    spirou_type_print(module, spirou_type_varying(module, f32), &out);

    CHECK_TEXT(out, "fn(i32, ptr(1), state) -> (<8 x f32>, state) <{i32, [4 x f32]}> varying f32");

    string_free(out);
    spirou_module_free(module);
}

int main(void)
{
    test_interning();
    test_varying();
    test_layout();
    test_print();

    return TEST_RESULT();
}
