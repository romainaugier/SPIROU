/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU)
#define __SPIROU

#include "libromano/common.h"

#define SPIROU_STRIFY(x) #x
#define SPIROU_STRIFY_MACRO(m) SPIROU_STRIFY(m)

#if !defined(SPIROU_VERSION_MAJOR)
#define SPIROU_VERSION_MAJOR 0
#endif /* !defined(SPIROU_VERSION_MAJOR) */

#if !defined(SPIROU_VERSION_MINOR)
#define SPIROU_VERSION_MINOR 0
#endif /* !defined(SPIROU_VERSION_MINOR) */

#if !defined(SPIROU_VERSION_PATCH)
#define SPIROU_VERSION_PATCH 0
#endif /* !defined(SPIROU_VERSION_PATCH) */

#define SPIROU_VERSION_STR SPIROU_STRIFY_MACRO(SPIROU_VERSION_MAJOR) "." \
                           SPIROU_STRIFY_MACRO(SPIROU_VERSION_MINOR) "." \
                           SPIROU_STRIFY_MACRO(SPIROU_VERSION_PATCH)

#if defined(SPIROU_BUILD_SHARED)
#define SPIROU_API ROMANO_EXPORT
#else
#define SPIROU_API ROMANO_IMPORT
#endif /* defined(SPIROU_BUILD_SHARED) */

#define SPIROU_CPP_ENTER ROMANO_CPP_ENTER
#define SPIROU_CPP_END ROMANO_CPP_END

#define SPIROU_FORCE_INLINE ROMANO_FORCE_INLINE
#define SPIROU_ASSERT(expr, message) ROMANO_ASSERT(expr, message)
#define SPIROU_UNUSED(x) ROMANO_UNUSED(x)

#if defined(DEBUG_BUILD)
#define SPIROU_DEBUG 1
#else
#define SPIROU_DEBUG 0
#endif /* defined(DEBUG_BUILD) */

#define SPIROU_INVALID 0xFFFFFFFFu

/* Types, values, nodes and regions are 32-bit ids into their owner's tables */
typedef uint32_t SpirouType;
typedef uint32_t SpirouValue;
typedef uint32_t SpirouNodeId;
typedef uint32_t SpirouRegionId;
typedef uint32_t SpirouSymbol;

#endif /* !defined(__SPIROU) */
