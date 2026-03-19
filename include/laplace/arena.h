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

/*
 * Arena allocator: bump-pointer allocator over a caller-owned backing buffer.
 *
 * Invariants:
 *  - No internal malloc/free/realloc.
 *  - Backing buffer is provided by caller (stack, static, or heap-allocated once at init).
 *  - All allocations are aligned to at least the requested alignment
 *    (which must be a power of two).
 *  - Reset reclaims all arena memory without freeing the backing buffer.
 *  - Thread-safety is NOT provided; intended for shard-local usage.
 */
typedef struct laplace_arena {
    uint8_t* base;       /* start of backing buffer */
    size_t   capacity;   /* total byte capacity of backing buffer */
    size_t   offset;     /* current bump offset (bytes used) */
    size_t   peak;       /* high-water mark for diagnostic purposes */
} laplace_arena_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_arena_t) <= LAPLACE_CACHELINE_SIZE,
                       "laplace_arena_t should fit in one cache line");

/*
 * Initialize an arena over the given backing buffer.
 * The buffer must be non-null and capacity must be > 0.
 * Returns LAPLACE_OK on success.
 */
laplace_error_t laplace_arena_init(laplace_arena_t* arena, void* buffer, size_t capacity);

/*
 * Allocate `size` bytes with the given `alignment` from the arena.
 * alignment must be a power of two.
 * Returns pointer to allocated memory, or NULL if exhausted.
 */
void* laplace_arena_alloc(laplace_arena_t* arena, size_t size, size_t alignment);

/*
 * Reset the arena, reclaiming all allocations.
 * Does not zero the memory. Does not free the backing buffer.
 * Preserves peak tracking.
 */
void laplace_arena_reset(laplace_arena_t* arena);

/*
 * Query remaining bytes available (approximate, not accounting for alignment padding).
 */
size_t laplace_arena_remaining(const laplace_arena_t* arena);

size_t laplace_arena_used(const laplace_arena_t* arena);

size_t laplace_arena_peak(const laplace_arena_t* arena);

void laplace_arena_reset_peak(laplace_arena_t* arena);

#ifdef __cplusplus
}
#endif

#endif
