#include "laplace/entity.h"

#include <string.h>

/*
 * Internal: convert entity ID (1-based) to slot index (0-based).
 * Entity ID 0 is reserved invalid.
 */
static inline uint32_t laplace__entity_id_to_slot(const laplace_entity_id_t id) {
    LAPLACE_ASSERT(id != LAPLACE_ENTITY_ID_INVALID);
    return id - 1u;
}

static inline laplace_entity_id_t laplace__slot_to_entity_id(const uint32_t slot) {
    return slot + 1u;
}

laplace_error_t laplace_entity_pool_init(laplace_entity_pool_t* const pool,
                                          laplace_arena_t* const arena,
                                          const uint32_t capacity) {
    LAPLACE_ASSERT(pool != NULL);
    LAPLACE_ASSERT(arena != NULL);
    LAPLACE_ASSERT(capacity > 0u);

    if (pool == NULL || arena == NULL || capacity == 0u) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_generation_t* const generations =
        (laplace_generation_t*)laplace_arena_alloc(arena, (size_t)capacity * sizeof(laplace_generation_t), _Alignof(laplace_generation_t));
    if (generations == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_entity_state_t* const states =
        (laplace_entity_state_t*)laplace_arena_alloc(arena, (size_t)capacity * sizeof(laplace_entity_state_t), _Alignof(laplace_entity_state_t));
    if (states == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    uint32_t* const free_stack =
        (uint32_t*)laplace_arena_alloc(arena, (size_t)capacity * sizeof(uint32_t), _Alignof(uint32_t));
    if (free_stack == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    uint8_t* const exact_roles =
        (uint8_t*)laplace_arena_alloc(arena, (size_t)capacity * sizeof(uint8_t), _Alignof(uint8_t));
    if (exact_roles == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_exact_type_id_t* const exact_type_ids =
        (laplace_exact_type_id_t*)laplace_arena_alloc(arena,
                                                      (size_t)capacity * sizeof(laplace_exact_type_id_t),
                                                      _Alignof(laplace_exact_type_id_t));
    if (exact_type_ids == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_exact_fact_row_t* const exact_fact_rows =
        (laplace_exact_fact_row_t*)laplace_arena_alloc(arena,
                                                       (size_t)capacity * sizeof(laplace_exact_fact_row_t),
                                                       _Alignof(laplace_exact_fact_row_t));
    if (exact_fact_rows == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    uint32_t* const exact_flags =
        (uint32_t*)laplace_arena_alloc(arena, (size_t)capacity * sizeof(uint32_t), _Alignof(uint32_t));
    if (exact_flags == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_branch_id_t* const branch_ids =
        (laplace_branch_id_t*)laplace_arena_alloc(arena,
                                                  (size_t)capacity * sizeof(laplace_branch_id_t),
                                                  _Alignof(laplace_branch_id_t));
    if (branch_ids == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_branch_generation_t* const branch_generations =
        (laplace_branch_generation_t*)laplace_arena_alloc(arena,
                                                          (size_t)capacity * sizeof(laplace_branch_generation_t),
                                                          _Alignof(laplace_branch_generation_t));
    if (branch_generations == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_epoch_id_t* const create_epochs =
        (laplace_epoch_id_t*)laplace_arena_alloc(arena,
                                                 (size_t)capacity * sizeof(laplace_epoch_id_t),
                                                 _Alignof(laplace_epoch_id_t));
    if (create_epochs == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_epoch_id_t* const retire_epochs =
        (laplace_epoch_id_t*)laplace_arena_alloc(arena,
                                                 (size_t)capacity * sizeof(laplace_epoch_id_t),
                                                 _Alignof(laplace_epoch_id_t));
    if (retire_epochs == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    uint32_t* const branch_flags =
        (uint32_t*)laplace_arena_alloc(arena,
                                       (size_t)capacity * sizeof(uint32_t),
                                       _Alignof(uint32_t));
    if (branch_flags == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    pool->generations = generations;
    pool->states = states;
    pool->free_stack = free_stack;
    pool->exact_roles = exact_roles;
    pool->exact_type_ids = exact_type_ids;
    pool->exact_fact_rows = exact_fact_rows;
    pool->exact_flags = exact_flags;
    pool->branch_ids = branch_ids;
    pool->branch_generations = branch_generations;
    pool->create_epochs = create_epochs;
    pool->retire_epochs = retire_epochs;
    pool->branch_flags = branch_flags;
    pool->capacity = capacity;
    pool->free_count = capacity;
    pool->alive_count = 0u;

    /* Initialize: all slots are FREE, generation starts at 1.
     * Free stack is filled top-down so that slot 0 is popped first. */
    for (uint32_t i = 0u; i < capacity; ++i) {
        generations[i] = 1u;           /* first valid generation */
        states[i] = LAPLACE_STATE_FREE;
        free_stack[i] = capacity - 1u - i;  /* stack[0] = last, stack[top] = 0 */
        exact_roles[i] = (uint8_t)LAPLACE_ENTITY_EXACT_ROLE_NONE;
        exact_type_ids[i] = LAPLACE_EXACT_TYPE_ID_INVALID;
        exact_fact_rows[i] = LAPLACE_EXACT_FACT_ROW_INVALID;
        exact_flags[i] = 0u;
        branch_ids[i] = LAPLACE_BRANCH_ID_INVALID;
        branch_generations[i] = LAPLACE_BRANCH_GENERATION_INVALID;
        create_epochs[i] = LAPLACE_EPOCH_ID_INVALID;
        retire_epochs[i] = LAPLACE_EPOCH_ID_INVALID;
        branch_flags[i] = LAPLACE_ENTITY_BRANCH_FLAG_NONE;
    }

    return LAPLACE_OK;
}

laplace_entity_handle_t laplace_entity_pool_alloc(laplace_entity_pool_t* const pool) {
    LAPLACE_ASSERT(pool != NULL);

    laplace_entity_handle_t handle = { LAPLACE_ENTITY_ID_INVALID, LAPLACE_GENERATION_INVALID };

    if (pool == NULL || pool->free_count == 0u) {
        return handle;
    }

    const uint32_t slot = pool->free_stack[pool->free_count - 1u];
    --pool->free_count;

    LAPLACE_ASSERT(slot < pool->capacity);
    LAPLACE_ASSERT(pool->states[slot] == LAPLACE_STATE_FREE);

    pool->states[slot] = LAPLACE_STATE_PENDING;
    ++pool->alive_count;

    handle.id = laplace__slot_to_entity_id(slot);
    handle.generation = pool->generations[slot];

    return handle;
}

laplace_error_t laplace_entity_pool_free(laplace_entity_pool_t* const pool,
                                          const laplace_entity_handle_t handle) {
    LAPLACE_ASSERT(pool != NULL);

    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    const laplace_entity_state_t current = pool->states[slot];

    if (current == LAPLACE_STATE_FREE || current == LAPLACE_STATE_RETIRED) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    laplace_error_t rc = laplace_entity_pool_mark_dead(pool, handle);
    if (rc != LAPLACE_OK && rc != LAPLACE_ERR_INVALID_STATE) {
        return rc;
    }

    rc = laplace_entity_pool_mark_retired(pool, handle, LAPLACE_EPOCH_ID_INVALID);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    return laplace_entity_pool_reclaim_retired(pool, handle);
}

bool laplace_entity_pool_is_alive(const laplace_entity_pool_t* const pool,
                                   const laplace_entity_handle_t handle) {
    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return false;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return false;
    }

    if (pool->generations[slot] != handle.generation) {
        return false;
    }

    const laplace_entity_state_t state = pool->states[slot];
    return state != LAPLACE_STATE_FREE && state != LAPLACE_STATE_RETIRED;
}

laplace_entity_state_t laplace_entity_pool_get_state(const laplace_entity_pool_t* const pool,
                                                      const laplace_entity_handle_t handle) {
    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_STATE_FREE;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_STATE_FREE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_STATE_FREE;
    }

    return pool->states[slot];
}

laplace_error_t laplace_entity_pool_set_state(laplace_entity_pool_t* const pool,
                                               const laplace_entity_handle_t handle,
                                               const laplace_entity_state_t new_state) {
    LAPLACE_ASSERT(pool != NULL);

    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    const laplace_entity_state_t current = pool->states[slot];
    if (!laplace_state_transition_valid(current, new_state)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->states[slot] = new_state;
    return LAPLACE_OK;
}

laplace_entity_exact_meta_t laplace_entity_pool_get_exact_meta(const laplace_entity_pool_t* const pool,
                                                                const laplace_entity_handle_t handle) {
    laplace_entity_exact_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.role = LAPLACE_ENTITY_EXACT_ROLE_NONE;
    meta.type_id = LAPLACE_EXACT_TYPE_ID_INVALID;
    meta.fact_row = LAPLACE_EXACT_FACT_ROW_INVALID;

    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return meta;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return meta;
    }

    if (pool->generations[slot] != handle.generation) {
        return meta;
    }

    meta.role = (laplace_entity_exact_role_t)pool->exact_roles[slot];
    meta.type_id = pool->exact_type_ids[slot];
    meta.fact_row = pool->exact_fact_rows[slot];
    meta.flags = pool->exact_flags[slot];
    return meta;
}

laplace_error_t laplace_entity_pool_set_exact_meta(laplace_entity_pool_t* const pool,
                                                    const laplace_entity_handle_t handle,
                                                    const laplace_entity_exact_meta_t* const meta) {
    LAPLACE_ASSERT(pool != NULL);

    if (pool == NULL || meta == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    if (pool->states[slot] == LAPLACE_STATE_FREE || pool->states[slot] == LAPLACE_STATE_RETIRED) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->exact_roles[slot] = (uint8_t)meta->role;
    pool->exact_type_ids[slot] = meta->type_id;
    pool->exact_fact_rows[slot] = meta->fact_row;
    pool->exact_flags[slot] = meta->flags;
    return LAPLACE_OK;
}

laplace_error_t laplace_entity_pool_clear_exact_meta(laplace_entity_pool_t* const pool,
                                                      const laplace_entity_handle_t handle) {
    const laplace_entity_exact_meta_t meta = {
        .role = LAPLACE_ENTITY_EXACT_ROLE_NONE,
        .type_id = LAPLACE_EXACT_TYPE_ID_INVALID,
        .fact_row = LAPLACE_EXACT_FACT_ROW_INVALID,
        .flags = 0u
    };
    return laplace_entity_pool_set_exact_meta(pool, handle, &meta);
}

laplace_entity_branch_meta_t laplace_entity_pool_get_branch_meta(const laplace_entity_pool_t* const pool,
                                                                  const laplace_entity_handle_t handle) {
    laplace_entity_branch_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.branch_id = LAPLACE_BRANCH_ID_INVALID;
    meta.branch_generation = LAPLACE_BRANCH_GENERATION_INVALID;
    meta.create_epoch = LAPLACE_EPOCH_ID_INVALID;
    meta.retire_epoch = LAPLACE_EPOCH_ID_INVALID;

    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return meta;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity || pool->generations[slot] != handle.generation) {
        return meta;
    }

    meta.branch_id = pool->branch_ids[slot];
    meta.branch_generation = pool->branch_generations[slot];
    meta.create_epoch = pool->create_epochs[slot];
    meta.retire_epoch = pool->retire_epochs[slot];
    meta.flags = pool->branch_flags[slot];
    return meta;
}

laplace_error_t laplace_entity_pool_set_branch_meta(laplace_entity_pool_t* const pool,
                                                     const laplace_entity_handle_t handle,
                                                     const laplace_entity_branch_meta_t* const meta) {
    if (pool == NULL || meta == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    if (pool->states[slot] == LAPLACE_STATE_FREE || pool->states[slot] == LAPLACE_STATE_RETIRED) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->branch_ids[slot] = meta->branch_id;
    pool->branch_generations[slot] = meta->branch_generation;
    pool->create_epochs[slot] = meta->create_epoch;
    pool->retire_epochs[slot] = meta->retire_epoch;
    pool->branch_flags[slot] = meta->flags;
    return LAPLACE_OK;
}

laplace_error_t laplace_entity_pool_clear_branch_meta(laplace_entity_pool_t* const pool,
                                                       const laplace_entity_handle_t handle) {
    const laplace_entity_branch_meta_t meta = {
        .branch_id = LAPLACE_BRANCH_ID_INVALID,
        .branch_generation = LAPLACE_BRANCH_GENERATION_INVALID,
        .create_epoch = LAPLACE_EPOCH_ID_INVALID,
        .retire_epoch = LAPLACE_EPOCH_ID_INVALID,
        .flags = LAPLACE_ENTITY_BRANCH_FLAG_NONE
    };
    return laplace_entity_pool_set_branch_meta(pool, handle, &meta);
}

laplace_error_t laplace_entity_pool_mark_dead(laplace_entity_pool_t* const pool,
                                               const laplace_entity_handle_t handle) {
    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    const laplace_entity_state_t current = pool->states[slot];
    if (current == LAPLACE_STATE_FREE || current == LAPLACE_STATE_RETIRED || current == LAPLACE_STATE_DEAD) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    if (!laplace_state_transition_valid(current, LAPLACE_STATE_DEAD)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->states[slot] = LAPLACE_STATE_DEAD;
    return LAPLACE_OK;
}

laplace_error_t laplace_entity_pool_mark_retired(laplace_entity_pool_t* const pool,
                                                  const laplace_entity_handle_t handle,
                                                  const laplace_epoch_id_t retire_epoch) {
    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    if (pool->states[slot] != LAPLACE_STATE_DEAD) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->states[slot] = LAPLACE_STATE_RETIRED;
    pool->retire_epochs[slot] = retire_epoch;
    pool->branch_flags[slot] |= LAPLACE_ENTITY_BRANCH_FLAG_RETIRED;

    LAPLACE_ASSERT(pool->alive_count > 0u);
    --pool->alive_count;
    return LAPLACE_OK;
}

laplace_error_t laplace_entity_pool_reclaim_retired(laplace_entity_pool_t* const pool,
                                                     const laplace_entity_handle_t handle) {
    if (pool == NULL || handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__entity_id_to_slot(handle.id);
    if (slot >= pool->capacity) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (pool->generations[slot] != handle.generation) {
        return LAPLACE_ERR_GENERATION_MISMATCH;
    }

    if (pool->states[slot] != LAPLACE_STATE_RETIRED) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    pool->states[slot] = LAPLACE_STATE_FREE;
    pool->exact_roles[slot] = (uint8_t)LAPLACE_ENTITY_EXACT_ROLE_NONE;
    pool->exact_type_ids[slot] = LAPLACE_EXACT_TYPE_ID_INVALID;
    pool->exact_fact_rows[slot] = LAPLACE_EXACT_FACT_ROW_INVALID;
    pool->exact_flags[slot] = 0u;
    pool->branch_ids[slot] = LAPLACE_BRANCH_ID_INVALID;
    pool->branch_generations[slot] = LAPLACE_BRANCH_GENERATION_INVALID;
    pool->create_epochs[slot] = LAPLACE_EPOCH_ID_INVALID;
    pool->retire_epochs[slot] = LAPLACE_EPOCH_ID_INVALID;
    pool->branch_flags[slot] = LAPLACE_ENTITY_BRANCH_FLAG_NONE;

    pool->generations[slot] += 1u;
    if (pool->generations[slot] == LAPLACE_GENERATION_INVALID) {
        pool->generations[slot] = 1u;
    }

    LAPLACE_ASSERT(pool->free_count < pool->capacity);
    pool->free_stack[pool->free_count] = slot;
    ++pool->free_count;
    return LAPLACE_OK;
}

laplace_generation_t laplace_entity_pool_generation(const laplace_entity_pool_t* const pool,
                                                     const uint32_t slot_index) {
    if (pool == NULL || slot_index >= pool->capacity) {
        return LAPLACE_GENERATION_INVALID;
    }
    return pool->generations[slot_index];
}
