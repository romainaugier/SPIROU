/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_PASSES)
#define __SPIROU_PASSES

#include "spirou/function.h"

SPIROU_CPP_ENTER

/*
 * Removes nodes that do not contribute to the function results. Side effects are kept alive by
 * their state outputs, so a store is only removed if its state never reaches a result.
 * Returns the number of removed nodes
 */
SPIROU_API uint32_t spirou_function_dce(SpirouFunction* func);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_PASSES) */
