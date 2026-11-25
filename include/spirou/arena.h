/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__SPIROU_ARENA)
#define __SPIROU_ARENA

#include "spirou/common.h"

#include "libromano/arena.h"

#include <string.h>

#define SPIROU_ARENA_BLOCK_SIZE 65536

/* libromano arenas do not align, keeping every size a multiple of 8 keeps everything aligned */
SPIROU_FORCE_INLINE void* spirou_arena_alloc(Arena* arena, size_t size)
{
    void* ptr = arena_push(arena, NULL, (size + 7) & ~(size_t)7);

    if(ptr != NULL)
        memset(ptr, 0, size);

    return ptr;
}

#endif /* !defined(__SPIROU_ARENA) */
