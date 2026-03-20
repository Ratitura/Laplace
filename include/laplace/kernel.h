#ifndef LAPLACE_KERNEL_H
#define LAPLACE_KERNEL_H

#include "laplace/assert.h"

#include <stdint.h>

typedef uint8_t laplace_kernel_id_t;

enum {
    LAPLACE_KERNEL_INVALID     = 0u,
    LAPLACE_KERNEL_RELATIONAL  = 1u,
    LAPLACE_KERNEL_PROOF       = 2u,
    LAPLACE_KERNEL_BITVECTOR   = 3u,
    LAPLACE_KERNEL_COUNT       = 4u
};

LAPLACE_STATIC_ASSERT(sizeof(laplace_kernel_id_t) == 1u,
                       "laplace_kernel_id_t must be 8-bit");

const char* laplace_kernel_name(laplace_kernel_id_t id);

#endif /* LAPLACE_KERNEL_H */
