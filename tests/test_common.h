/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_TEST_COMMON)
#define __SPIROU_TEST_COMMON

#include "spirou/spirou.h"

#include <stdio.h>
#include <string.h>

static int test_failures = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if(!(cond))                                                              \
        {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            test_failures++;                                                     \
        }                                                                        \
    } while(0)

#define CHECK_EQ_UINT(a, b)                                                      \
    do                                                                           \
    {                                                                            \
        unsigned long long _a = (unsigned long long)(a);                         \
        unsigned long long _b = (unsigned long long)(b);                         \
        if(_a != _b)                                                             \
        {                                                                        \
            fprintf(stderr, "%s:%d: %s == %s failed: %llu != %llu\n", __FILE__, __LINE__, #a, #b, _a, _b); \
            test_failures++;                                                     \
        }                                                                        \
    } while(0)

static inline void check_text(const char* file, int line, const char* got, const char* expected)
{
    if(strcmp(got, expected) != 0)
    {
        fprintf(stderr, "%s:%d: text mismatch\n--- got ---\n%s--- expected ---\n%s", file, line, got, expected);
        test_failures++;
    }
}

#define CHECK_TEXT(got, expected) check_text(__FILE__, __LINE__, (got), (expected))

static inline void check_verifies(const char* file, int line, SpirouFunction* func)
{
    String errors = string_new("");
    uint32_t count = spirou_function_verify(func, &errors);

    if(count != 0)
    {
        fprintf(stderr, "%s:%d: %u verifier errors\n%s", file, line, count, errors);
        test_failures++;
    }

    string_free(errors);
}

#define CHECK_VERIFIES(func) check_verifies(__FILE__, __LINE__, (func))

static inline String print_function(SpirouFunction* func)
{
    String out = string_new("");
    spirou_function_print(func, &out);
    return out;
}

#define TEST_RESULT() (test_failures == 0 ? 0 : 1)

#endif /* !defined(__SPIROU_TEST_COMMON) */
