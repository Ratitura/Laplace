#ifndef LAPLACE_STATE_H
#define LAPLACE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum laplace_entity_state {
    LAPLACE_STATE_FREE    = 0u,
    LAPLACE_STATE_PENDING = 1u,
    LAPLACE_STATE_READY   = 2u,
    LAPLACE_STATE_ACTIVE  = 3u,
    LAPLACE_STATE_MASKED  = 4u,
    LAPLACE_STATE_DEAD    = 5u,
    LAPLACE_STATE_RETIRED = 6u,
    LAPLACE_STATE_COUNT_  = 7u  /* sentinel, not a valid state */
} laplace_entity_state_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_entity_state_t) <= sizeof(uint32_t),
                       "laplace_entity_state_t must fit in uint32_t or less");

bool laplace_state_transition_valid(laplace_entity_state_t from, laplace_entity_state_t to);

const char* laplace_state_name(laplace_entity_state_t state);

#ifdef __cplusplus
}
#endif

#endif
