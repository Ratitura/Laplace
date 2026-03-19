#ifndef LAPLACE_ENTITY_H
#define LAPLACE_ENTITY_H

#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/errors.h"
#include "laplace/state.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entity pool: SoA storage for entity metadata.
 *
 * Design:
 *  - Fixed-capacity, allocated from an arena (no hot-path heap).
 *  - SoA arrays: generations[], states[], free_stack[].
 *  - Free-list is an index stack (LIFO) stored as a uint32_t array.
 *    No linked list. No pointer chasing.
 *  - Entity index 0 is reserved as invalid (LAPLACE_ENTITY_ID_INVALID).
 *    The pool maps indices 1..capacity (inclusive) to usable slots 0..capacity-1.
 *  - Generation starts at 1 for each slot. Generation 0 is invalid.
 *  - Recycling a slot increments its generation.
 *
 * Thread safety:
 *  - Not provided. Intended for shard-local usage.
 */

typedef struct laplace_entity_handle {
    laplace_entity_id_t  id;
    laplace_generation_t generation;
} laplace_entity_handle_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_entity_handle_t) == 8u,
                       "laplace_entity_handle_t must be 8 bytes");

typedef enum laplace_entity_exact_role {
    LAPLACE_ENTITY_EXACT_ROLE_NONE = 0u,
    LAPLACE_ENTITY_EXACT_ROLE_CONSTANT = 1u,
    LAPLACE_ENTITY_EXACT_ROLE_FACT = 2u
} laplace_entity_exact_role_t;

typedef struct laplace_entity_exact_meta {
    laplace_entity_exact_role_t role;
    laplace_exact_type_id_t     type_id;
    laplace_exact_fact_row_t    fact_row;
    uint32_t                    flags;
} laplace_entity_exact_meta_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_entity_exact_meta_t) <= 16u,
                       "laplace_entity_exact_meta_t must remain compact");

typedef struct laplace_entity_branch_meta {
    laplace_branch_id_t         branch_id;
    laplace_branch_generation_t branch_generation;
    laplace_epoch_id_t          create_epoch;
    laplace_epoch_id_t          retire_epoch;
    uint32_t                    flags;
} laplace_entity_branch_meta_t;

enum {
    LAPLACE_ENTITY_BRANCH_FLAG_NONE = 0u,
    LAPLACE_ENTITY_BRANCH_FLAG_LOCAL = 1u << 0,
    LAPLACE_ENTITY_BRANCH_FLAG_RETIRED = 1u << 1
};

typedef struct laplace_entity_pool {
    laplace_generation_t*    generations;   /* [capacity] generation counter per slot */
    laplace_entity_state_t*  states;        /* [capacity] lifecycle state per slot (stored as uint8_t-compatible enum) */
    uint32_t*                free_stack;    /* [capacity] LIFO stack of free slot indices */
    uint8_t*                 exact_roles;   /* [capacity] laplace_entity_exact_role_t storage */
    laplace_exact_type_id_t* exact_type_ids;/* [capacity] exact symbolic type/category */
    laplace_exact_fact_row_t* exact_fact_rows;/* [capacity] back-link to fact row if role == FACT */
    uint32_t*                exact_flags;   /* [capacity] exact symbolic flags */
    laplace_branch_id_t*         branch_ids;         /* [capacity] owning branch id, or INVALID for committed/global */
    laplace_branch_generation_t* branch_generations; /* [capacity] owning branch generation */
    laplace_epoch_id_t*          create_epochs;      /* [capacity] creation epoch for branch-local entities */
    laplace_epoch_id_t*          retire_epochs;      /* [capacity] retire epoch for delayed reuse */
    uint32_t*                    branch_flags;       /* [capacity] local/retired flags */
    uint32_t                 capacity;      /* max entities (slot count) */
    uint32_t                 free_count;    /* number of free slots in the stack */
    uint32_t                 alive_count;   /* entities currently not FREE or RETIRED */
} laplace_entity_pool_t;

/*
 * Initialize an entity pool, allocating SoA arrays from the given arena.
 * capacity must be > 0.
 * All slots start as FREE with generation = 1.
 * Returns LAPLACE_OK on success, LAPLACE_ERR_CAPACITY_EXHAUSTED if arena is too small.
 */
laplace_error_t laplace_entity_pool_init(laplace_entity_pool_t* pool, laplace_arena_t* arena, uint32_t capacity);

/*
 * Allocate a new entity from the pool.
 * Returns a handle with a valid ID and current generation.
 * On failure (pool exhausted), handle.id == LAPLACE_ENTITY_ID_INVALID.
 * The new entity is placed in PENDING state.
 */
laplace_entity_handle_t laplace_entity_pool_alloc(laplace_entity_pool_t* pool);

/*
 * Release an entity back to the pool.
 * The entity must match the expected generation (stale-handle safety).
 * The entity transitions through DEAD -> RETIRED -> FREE and its generation is bumped.
 * Returns LAPLACE_OK on success.
 */
laplace_error_t laplace_entity_pool_free(laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

/*
 * Check if a handle is currently alive (state is not FREE or RETIRED)
 * and matches the current generation.
 */
bool laplace_entity_pool_is_alive(const laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

/*
 * Get the current state of an entity by handle.
 * Returns LAPLACE_STATE_FREE if the handle is invalid or stale.
 */
laplace_entity_state_t laplace_entity_pool_get_state(const laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

/*
 * Transition an entity to a new state.
 * Validates both generation and transition legality.
 */
laplace_error_t laplace_entity_pool_set_state(laplace_entity_pool_t* pool,
                                               laplace_entity_handle_t handle,
                                               laplace_entity_state_t new_state);

/*
 * Read exact symbolic metadata for an entity.
 * Invalid or stale handles return a zeroed metadata record with role NONE.
 */
laplace_entity_exact_meta_t laplace_entity_pool_get_exact_meta(const laplace_entity_pool_t* pool,
                                                                laplace_entity_handle_t handle);

/*
 * Set exact symbolic metadata for a live entity.
 * Metadata changes are generation-safe and deterministic.
 */
laplace_error_t laplace_entity_pool_set_exact_meta(laplace_entity_pool_t* pool,
                                                    laplace_entity_handle_t handle,
                                                    const laplace_entity_exact_meta_t* meta);

/*
 * Clear exact symbolic metadata for a live entity.
 */
laplace_error_t laplace_entity_pool_clear_exact_meta(laplace_entity_pool_t* pool,
                                                      laplace_entity_handle_t handle);

laplace_entity_branch_meta_t laplace_entity_pool_get_branch_meta(const laplace_entity_pool_t* pool,
                                                                  laplace_entity_handle_t handle);

laplace_error_t laplace_entity_pool_set_branch_meta(laplace_entity_pool_t* pool,
                                                     laplace_entity_handle_t handle,
                                                     const laplace_entity_branch_meta_t* meta);

laplace_error_t laplace_entity_pool_clear_branch_meta(laplace_entity_pool_t* pool,
                                                       laplace_entity_handle_t handle);

laplace_error_t laplace_entity_pool_mark_dead(laplace_entity_pool_t* pool,
                                               laplace_entity_handle_t handle);

laplace_error_t laplace_entity_pool_mark_retired(laplace_entity_pool_t* pool,
                                                  laplace_entity_handle_t handle,
                                                  laplace_epoch_id_t retire_epoch);

laplace_error_t laplace_entity_pool_reclaim_retired(laplace_entity_pool_t* pool,
                                                     laplace_entity_handle_t handle);

/*
 * Get the current generation for a slot index (0-based).
 * Returns LAPLACE_GENERATION_INVALID if out of range.
 */
laplace_generation_t laplace_entity_pool_generation(const laplace_entity_pool_t* pool, uint32_t slot_index);

#ifdef __cplusplus
}
#endif

#endif
