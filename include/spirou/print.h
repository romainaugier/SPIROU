/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_PRINT)
#define __SPIROU_PRINT

#include "spirou/function.h"

SPIROU_CPP_ENTER

/* Appends the text form, nodes are printed in topological order within each region */
SPIROU_API void spirou_function_print(SpirouFunction* func, String* out);

SPIROU_API void spirou_module_print(SpirouModule* module, String* out);

SPIROU_CPP_END

#endif /* !defined(__SPIROU_PRINT) */
