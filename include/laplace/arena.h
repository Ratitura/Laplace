#ifndef LAPLACE_ARENA_H
#define LAPLACE_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/align.h"
#include "laplace/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_arena {
    uint8_t* base;       /* start of backing buffer */
    size_t   capacity;   /* total byte capacity of backing buffer */
    size_t   offset;     /* current bump offset (bytes used) */
    size_t   peak;       /* high-water mark for diagnostic purposes */
} laplace_arena_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_arena_t) <= LAPLACE_CACHELINE_SIZE,
                       "laplace_arena_t should fit in one cache line");

laplace_error_t laplace_arena_init(laplace_arena_t* arena, void* buffer, size_t capacity);

void* laplace_arena_alloc(laplace_arena_t* arena, size_t size, size_t alignment);

void laplace_arena_reset(laplace_arena_t* arena);

size_t laplace_arena_remaining(const laplace_arena_t* arena);

size_t laplace_arena_used(const laplace_arena_t* arena);

size_t laplace_arena_peak(const laplace_arena_t* arena);

void laplace_arena_reset_peak(laplace_arena_t* arena);

#ifdef __cplusplus
}
#endif

#endif
