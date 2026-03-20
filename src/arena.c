#include "laplace/arena.h"

#include <string.h>

laplace_error_t laplace_arena_init(laplace_arena_t* const arena, void* const buffer, const size_t capacity) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena == NULL || buffer == NULL || capacity == 0u) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    arena->base = (uint8_t*)buffer;
    arena->capacity = capacity;
    arena->offset = 0u;
    arena->peak = 0u;

    return LAPLACE_OK;
}

void* laplace_arena_alloc(laplace_arena_t* const arena, const size_t size, const size_t alignment) {
    LAPLACE_ASSERT(arena != NULL);
    LAPLACE_ASSERT(laplace_is_pow2_u64((uint64_t)alignment));

    if (arena == NULL || size == 0u || !laplace_is_pow2_u64((uint64_t)alignment)) {
        return NULL;
    }

    const uintptr_t current = (uintptr_t)(arena->base + arena->offset);
    const uintptr_t aligned = laplace_align_up_uintptr(current, (uintptr_t)alignment);
    const size_t padding = (size_t)(aligned - current);

    if (padding > arena->capacity - arena->offset) {
        return NULL;
    }
    const size_t total = padding + size;
    if (total < size) {
        return NULL; /* size_t overflow */
    }
    if (arena->offset + total > arena->capacity) {
        return NULL;
    }

    void* const ptr = (void*)aligned;
    arena->offset += total;

    if (arena->offset > arena->peak) {
        arena->peak = arena->offset;
    }

    return ptr;
}

void laplace_arena_reset(laplace_arena_t* const arena) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena != NULL) {
        arena->offset = 0u;
    }
}

size_t laplace_arena_remaining(const laplace_arena_t* const arena) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena == NULL) {
        return 0u;
    }
    return arena->capacity - arena->offset;
}

size_t laplace_arena_used(const laplace_arena_t* const arena) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena == NULL) {
        return 0u;
    }
    return arena->offset;
}

size_t laplace_arena_peak(const laplace_arena_t* const arena) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena == NULL) {
        return 0u;
    }
    return arena->peak;
}

void laplace_arena_reset_peak(laplace_arena_t* const arena) {
    LAPLACE_ASSERT(arena != NULL);
    if (arena != NULL) {
        arena->peak = arena->offset;
    }
}
