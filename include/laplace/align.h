#ifndef LAPLACE_ALIGN_H
#define LAPLACE_ALIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"

#define LAPLACE_CACHELINE_SIZE 64u
#define LAPLACE_ALIGNAS(bytes) _Alignas(bytes)
#define LAPLACE_ALIGNOF(type) _Alignof(type)

static inline bool laplace_is_pow2_u64(const uint64_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static inline uintptr_t laplace_align_up_uintptr(const uintptr_t value, const uintptr_t alignment) {
    LAPLACE_ASSERT(laplace_is_pow2_u64((uint64_t)alignment));
    return (value + (alignment - 1u)) & ~(alignment - 1u);
}

static inline bool laplace_is_aligned_uintptr(const uintptr_t value, const uintptr_t alignment) {
    LAPLACE_ASSERT(laplace_is_pow2_u64((uint64_t)alignment));
    return (value & (alignment - 1u)) == 0u;
}

#endif
