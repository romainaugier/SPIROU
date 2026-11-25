/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_VERIFY)
#define __SPIROU_VERIFY

#include "spirou/function.h"

SPIROU_CPP_ENTER

/*
 * Checks region scoping, operand types, structural node signatures, uniformity (varying values
 * only live inside maps) and acyclicity. Returns the number of errors, messages are appended to
 * errors if not NULL
 */
SPIROU_API uint32_t spirou_function_verify(SpirouFunction* func, String* errors);

SPIROU_API uint32_t spirou_module_verify(SpirouModule* module, String* errors);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_VERIFY) */
