/*
  +----------------------------------------------------------------------+
  | em                                                                   |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2025                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct _em_arena_t {
    char*  base;
    char*  next;
    char*  end;
    size_t alignment;
    size_t size;
} em_arena_t;

#define EM_ARENA_ALIGNED(arena, size) \
    (((size) + (arena->alignment-1)) & ~(arena->alignment-1))

em_arena_t* em_arena_create(size_t size, size_t alignment) {
    em_arena_t* arena = calloc(1, sizeof(em_arena_t));

    if (!arena) {
        errno =
            ENOMEM;
        return NULL;
    }

    arena->alignment = alignment;
    arena->base =
        calloc(1, EM_ARENA_ALIGNED(arena, size));

    if (!arena->base) {
        errno =
            ENOMEM;
        free(arena);
        return NULL;
    }

    arena->size = EM_ARENA_ALIGNED(arena, size);
    arena->next = arena->base;
    arena->end =
        arena->base +
            arena->size;
    return arena;
}

void* em_arena_alloc(em_arena_t* arena, size_t size) {
    while ((arena->next + EM_ARENA_ALIGNED(arena, size)) > arena->end) {
        size_t position = arena->next - arena->base;

        arena->size *= 2;
        arena->base = realloc(arena->base, arena->size);

        if (!arena->base) {
            errno =
                ENOMEM;
            return NULL;
        }

        arena->next =
            arena->base + position;
        arena->end =
            arena->base +
                arena->size;
    }

    void* chunk = arena->next;
    arena->next +=
        EM_ARENA_ALIGNED(arena, size);
    return chunk;
}

void* em_arena_calloc(em_arena_t* arena, size_t num, size_t size) {
    size_t aligned = EM_ARENA_ALIGNED(arena, num * size);

    void* zeroed =
        em_arena_alloc(
            arena, aligned);

    if (!zeroed) {
        return NULL;
    }

    memset(zeroed, 0, aligned);
    return zeroed;
}

void em_arena_reset(em_arena_t* arena) {
    arena->next = arena->base;
}

void em_arena_destroy(em_arena_t* arena) {
    free(arena->base);
    free(arena);
}