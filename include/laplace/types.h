#ifndef LAPLACE_TYPES_H
#define LAPLACE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"

typedef uint32_t laplace_entity_id_t;
typedef uint32_t laplace_generation_t;
typedef uint16_t laplace_predicate_id_t;
typedef uint32_t laplace_rule_id_t;
typedef uint32_t laplace_provenance_id_t;
typedef uint32_t laplace_exact_fact_row_t;
typedef uint16_t laplace_exact_type_id_t;
typedef uint16_t laplace_exact_var_id_t;
typedef uint16_t laplace_branch_id_t;
typedef uint16_t laplace_branch_generation_t;
typedef uint32_t laplace_epoch_id_t;
typedef uint64_t laplace_tick_t;

typedef struct laplace_branch_handle {
    laplace_branch_id_t         id;
    laplace_branch_generation_t generation;
} laplace_branch_handle_t;

enum {
    LAPLACE_ENTITY_ID_INVALID = 0u,
    LAPLACE_GENERATION_INVALID = 0u,
    LAPLACE_PREDICATE_ID_INVALID = 0u,
    LAPLACE_RULE_ID_INVALID = 0u,
    LAPLACE_PROVENANCE_ID_INVALID = 0u,
    LAPLACE_EXACT_FACT_ROW_INVALID = 0u,
    LAPLACE_EXACT_TYPE_ID_INVALID = 0u,
    LAPLACE_EXACT_VAR_ID_INVALID = 0u,
    LAPLACE_BRANCH_ID_INVALID = 0u,
    LAPLACE_BRANCH_GENERATION_INVALID = 0u,
    LAPLACE_EPOCH_ID_INVALID = 0u
};

LAPLACE_STATIC_ASSERT(sizeof(laplace_entity_id_t) == 4u, "laplace_entity_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_generation_t) == 4u, "laplace_generation_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_predicate_id_t) == 2u, "laplace_predicate_id_t must be 16-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_rule_id_t) == 4u, "laplace_rule_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_provenance_id_t) == 4u, "laplace_provenance_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_exact_fact_row_t) == 4u, "laplace_exact_fact_row_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_exact_type_id_t) == 2u, "laplace_exact_type_id_t must be 16-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_exact_var_id_t) == 2u, "laplace_exact_var_id_t must be 16-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_branch_id_t) == 2u, "laplace_branch_id_t must be 16-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_branch_generation_t) == 2u, "laplace_branch_generation_t must be 16-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_epoch_id_t) == 4u, "laplace_epoch_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_tick_t) == 8u, "laplace_tick_t must be 64-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_branch_handle_t) == 4u, "laplace_branch_handle_t must be 4 bytes");

#endif
