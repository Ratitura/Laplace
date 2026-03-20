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

laplace_error_t laplace_entity_pool_init(laplace_entity_pool_t* pool, laplace_arena_t* arena, uint32_t capacity);

laplace_entity_handle_t laplace_entity_pool_alloc(laplace_entity_pool_t* pool);

laplace_error_t laplace_entity_pool_free(laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

bool laplace_entity_pool_is_alive(const laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

laplace_entity_state_t laplace_entity_pool_get_state(const laplace_entity_pool_t* pool, laplace_entity_handle_t handle);

laplace_error_t laplace_entity_pool_set_state(laplace_entity_pool_t* pool,
                                               laplace_entity_handle_t handle,
                                               laplace_entity_state_t new_state);

laplace_entity_exact_meta_t laplace_entity_pool_get_exact_meta(const laplace_entity_pool_t* pool,
                                                                laplace_entity_handle_t handle);

laplace_error_t laplace_entity_pool_set_exact_meta(laplace_entity_pool_t* pool,
                                                    laplace_entity_handle_t handle,
                                                    const laplace_entity_exact_meta_t* meta);

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

laplace_generation_t laplace_entity_pool_generation(const laplace_entity_pool_t* pool, uint32_t slot_index);

#ifdef __cplusplus
}
#endif

#endif
