/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_BUILDER)
#define __SPIROU_BUILDER

#include "spirou/function.h"

SPIROU_CPP_ENTER

/*
 * Builds nodes in the current region. Uniform operands mixed with varying ones are promoted:
 * the result of an op is varying if any of its inputs is varying.
 * Memory ops take the current state by pointer and update it with the new state.
 */
typedef struct SpirouBuilder
{
    SpirouFunction* func;
    SpirouModule* module;
    SpirouRegionId region;
} SpirouBuilder;

SPIROU_API void spirou_builder_init(SpirouBuilder* builder, SpirouFunction* func);

SPIROU_API void spirou_builder_set_region(SpirouBuilder* builder, SpirouRegionId region);

SPIROU_API SpirouValue spirou_builder_param(SpirouBuilder* builder, uint32_t index);

/* Sets the function results, in the order of the function type */
SPIROU_API void spirou_builder_return(SpirouBuilder* builder, const SpirouValue* values, uint32_t count);

SPIROU_API SpirouValue spirou_build_undef(SpirouBuilder* builder, SpirouType type);

SPIROU_API SpirouValue spirou_build_iconst(SpirouBuilder* builder, SpirouType type, uint64_t value);

SPIROU_API SpirouValue spirou_build_fconst(SpirouBuilder* builder, SpirouType type, double value);

SPIROU_API SpirouValue spirou_build_binop(SpirouBuilder* builder, SpirouOp op, SpirouValue lhs, SpirouValue rhs);

SPIROU_API SpirouValue spirou_build_unop(SpirouBuilder* builder, SpirouOp op, SpirouValue value);

SPIROU_API SpirouValue spirou_build_fma(SpirouBuilder* builder, SpirouValue a, SpirouValue b, SpirouValue c);

SPIROU_API SpirouValue spirou_build_cmp(SpirouBuilder* builder, SpirouCmp predicate, SpirouValue lhs, SpirouValue rhs);

SPIROU_API SpirouValue spirou_build_select(SpirouBuilder* builder,
                                           SpirouValue cond,
                                           SpirouValue if_true,
                                           SpirouValue if_false);

/* type is the uniform destination type, it becomes varying if value is varying */
SPIROU_API SpirouValue spirou_build_cast(SpirouBuilder* builder, SpirouOp op, SpirouValue value, SpirouType type);

SPIROU_API SpirouValue spirou_build_ptradd(SpirouBuilder* builder, SpirouValue ptr, SpirouValue offset);

SPIROU_API SpirouValue spirou_build_symbol_addr(SpirouBuilder* builder, SpirouSymbol symbol);

SPIROU_API SpirouValue spirou_build_stackslot(SpirouBuilder* builder, uint32_t size, uint32_t align);

/* align 0 means natural alignment */
SPIROU_API SpirouValue spirou_build_load(SpirouBuilder* builder,
                                         SpirouValue* state,
                                         SpirouValue ptr,
                                         SpirouType type,
                                         uint32_t align);

SPIROU_API void spirou_build_store(SpirouBuilder* builder,
                                   SpirouValue* state,
                                   SpirouValue ptr,
                                   SpirouValue value,
                                   uint32_t align);

/* Results are the node outputs, in the order of the function type results */
SPIROU_API SpirouNodeId spirou_build_call(SpirouBuilder* builder,
                                          SpirouValue callee,
                                          SpirouType func_type,
                                          const SpirouValue* args,
                                          uint32_t num_args);

/* Gamma */

SPIROU_API SpirouNodeId spirou_build_gamma(SpirouBuilder* builder, SpirouValue predicate, uint32_t num_branches);

SPIROU_API SpirouRegionId spirou_gamma_branch(SpirouFunction* func, SpirouNodeId gamma, uint32_t branch);

/* Returns the entry index, the value is the argument at that index in every branch */
SPIROU_API uint32_t spirou_gamma_add_entry(SpirouFunction* func, SpirouNodeId gamma, SpirouValue value);

/* branch_values holds one value per branch, returns the gamma output */
SPIROU_API SpirouValue spirou_gamma_add_exit(SpirouFunction* func,
                                             SpirouNodeId gamma,
                                             const SpirouValue* branch_values);

/* Theta */

SPIROU_API SpirouNodeId spirou_build_theta(SpirouBuilder* builder);

SPIROU_API SpirouRegionId spirou_theta_body(SpirouFunction* func, SpirouNodeId theta);

/*
 * Returns the loop variable index: body argument i, node output i, body result i + 1.
 * The next value defaults to the argument (the variable is loop invariant)
 */
SPIROU_API uint32_t spirou_theta_add_loopvar(SpirouFunction* func, SpirouNodeId theta, SpirouValue init);

SPIROU_API void spirou_theta_set_next(SpirouFunction* func, SpirouNodeId theta, uint32_t index, SpirouValue next);

SPIROU_API void spirou_theta_set_predicate(SpirouFunction* func, SpirouNodeId theta, SpirouValue cont);

/* Map */

SPIROU_API SpirouNodeId spirou_build_map(SpirouBuilder* builder, SpirouValue count, SpirouValue state);

SPIROU_API SpirouRegionId spirou_map_body(SpirouFunction* func, SpirouNodeId map);

SPIROU_API SpirouValue spirou_map_index(SpirouFunction* func, SpirouNodeId map);

SPIROU_API SpirouValue spirou_map_state(SpirouFunction* func, SpirouNodeId map);

/* Returns the body argument for the captured uniform value */
SPIROU_API SpirouValue spirou_map_add_capture(SpirouFunction* func, SpirouNodeId map, SpirouValue value);

SPIROU_API void spirou_map_set_state(SpirouFunction* func, SpirouNodeId map, SpirouValue state);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_BUILDER) */
