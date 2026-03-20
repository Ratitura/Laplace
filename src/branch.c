#include "laplace/branch.h"

#include <string.h>

#include "laplace/assert.h"
#include "laplace/exact.h"
#include "laplace/observe.h"
#include "laplace/state.h"

static inline bool laplace__branch_slot_valid(const laplace_branch_id_t branch_id) {
    return branch_id != LAPLACE_BRANCH_ID_INVALID && branch_id <= LAPLACE_BRANCH_MAX_BRANCHES;
}

static inline uint32_t laplace__branch_slot(const laplace_branch_id_t branch_id) {
    LAPLACE_ASSERT(laplace__branch_slot_valid(branch_id));
    return (uint32_t)branch_id - 1u;
}

static bool laplace__branch_handle_valid(const laplace_branch_system_t* const system,
                                          const laplace_branch_handle_t branch) {
    if (system == NULL || !laplace__branch_slot_valid(branch.id) ||
        branch.generation == LAPLACE_BRANCH_GENERATION_INVALID) {
        return false;
    }

    const uint32_t slot = laplace__branch_slot(branch.id);
    return system->generations[slot] == branch.generation &&
           system->statuses[slot] != (uint8_t)LAPLACE_BRANCH_STATUS_INVALID;
}

static uint32_t* laplace__branch_fact_count_ptr(laplace_branch_system_t* const system,
                                                const laplace_branch_handle_t branch) {
    return &system->owned_fact_counts[laplace__branch_slot(branch.id)];
}

static uint32_t* laplace__branch_entity_count_ptr(laplace_branch_system_t* const system,
                                                  const laplace_branch_handle_t branch) {
    return &system->owned_entity_counts[laplace__branch_slot(branch.id)];
}

static laplace_exact_fact_row_t* laplace__branch_fact_base(laplace_branch_system_t* const system,
                                                            const laplace_branch_handle_t branch) {
    return &system->owned_fact_rows[(size_t)laplace__branch_slot(branch.id) * LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH];
}

static laplace_entity_handle_t* laplace__branch_fact_promotions_base(laplace_branch_system_t* const system,
                                                                      const laplace_branch_handle_t branch) {
    return &system->owned_fact_promotions[(size_t)laplace__branch_slot(branch.id) * LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH];
}

static laplace_entity_handle_t* laplace__branch_entity_base(laplace_branch_system_t* const system,
                                                             const laplace_branch_handle_t branch) {
    return &system->owned_entities[(size_t)laplace__branch_slot(branch.id) * LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH];
}

static laplace_entity_handle_t* laplace__branch_entity_promotions_base(laplace_branch_system_t* const system,
                                                                        const laplace_branch_handle_t branch) {
    return &system->owned_entity_promotions[(size_t)laplace__branch_slot(branch.id) * LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH];
}

laplace_error_t laplace_branch_system_init(laplace_branch_system_t* const system,
                                            laplace_arena_t* const arena,
                                            laplace_exact_store_t* const store,
                                            laplace_entity_pool_t* const entity_pool) {
    if (system == NULL || arena == NULL || store == NULL || entity_pool == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(system, 0, sizeof(*system));
    system->store = store;
    system->entity_pool = entity_pool;
    system->current_epoch = 1u;

    system->generations = (laplace_branch_generation_t*)laplace_arena_alloc(arena,
                                                                             (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_branch_generation_t),
                                                                             _Alignof(laplace_branch_generation_t));
    system->statuses = (uint8_t*)laplace_arena_alloc(arena,
                                                     (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint8_t),
                                                     _Alignof(uint8_t));
    system->parent_ids = (laplace_branch_id_t*)laplace_arena_alloc(arena,
                                                                   (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_branch_id_t),
                                                                   _Alignof(laplace_branch_id_t));
    system->parent_generations = (laplace_branch_generation_t*)laplace_arena_alloc(arena,
                                                                                    (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_branch_generation_t),
                                                                                    _Alignof(laplace_branch_generation_t));
    system->create_epochs = (laplace_epoch_id_t*)laplace_arena_alloc(arena,
                                                                     (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_epoch_id_t),
                                                                     _Alignof(laplace_epoch_id_t));
    system->close_epochs = (laplace_epoch_id_t*)laplace_arena_alloc(arena,
                                                                    (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_epoch_id_t),
                                                                    _Alignof(laplace_epoch_id_t));
    system->owned_fact_counts = (uint32_t*)laplace_arena_alloc(arena,
                                                               (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint32_t),
                                                               _Alignof(uint32_t));
    system->owned_entity_counts = (uint32_t*)laplace_arena_alloc(arena,
                                                                 (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint32_t),
                                                                 _Alignof(uint32_t));
    system->owned_fact_rows = (laplace_exact_fact_row_t*)laplace_arena_alloc(arena,
                                                                              (size_t)LAPLACE_BRANCH_MAX_BRANCHES * LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH * sizeof(laplace_exact_fact_row_t),
                                                                              _Alignof(laplace_exact_fact_row_t));
    system->owned_fact_promotions = (laplace_entity_handle_t*)laplace_arena_alloc(arena,
                                                                                   (size_t)LAPLACE_BRANCH_MAX_BRANCHES * LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH * sizeof(laplace_entity_handle_t),
                                                                                   _Alignof(laplace_entity_handle_t));
    system->owned_entities = (laplace_entity_handle_t*)laplace_arena_alloc(arena,
                                                                            (size_t)LAPLACE_BRANCH_MAX_BRANCHES * LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH * sizeof(laplace_entity_handle_t),
                                                                            _Alignof(laplace_entity_handle_t));
    system->owned_entity_promotions = (laplace_entity_handle_t*)laplace_arena_alloc(arena,
                                                                                      (size_t)LAPLACE_BRANCH_MAX_BRANCHES * LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH * sizeof(laplace_entity_handle_t),
                                                                                      _Alignof(laplace_entity_handle_t));

    if (system->generations == NULL || system->statuses == NULL || system->parent_ids == NULL ||
        system->parent_generations == NULL || system->create_epochs == NULL || system->close_epochs == NULL ||
        system->owned_fact_counts == NULL || system->owned_entity_counts == NULL ||
        system->owned_fact_rows == NULL || system->owned_fact_promotions == NULL ||
        system->owned_entities == NULL || system->owned_entity_promotions == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    memset(system->statuses, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint8_t));
    memset(system->parent_ids, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_branch_id_t));
    memset(system->parent_generations, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_branch_generation_t));
    memset(system->create_epochs, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_epoch_id_t));
    memset(system->close_epochs, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(laplace_epoch_id_t));
    memset(system->owned_fact_counts, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint32_t));
    memset(system->owned_entity_counts, 0, (size_t)LAPLACE_BRANCH_MAX_BRANCHES * sizeof(uint32_t));

    for (uint32_t i = 0u; i < LAPLACE_BRANCH_MAX_BRANCHES; ++i) {
        system->generations[i] = 1u;
    }

    return LAPLACE_OK;
}

laplace_branch_handle_t laplace_branch_create(laplace_branch_system_t* const system,
                                               const laplace_branch_handle_t parent_branch,
                                               laplace_error_t* const out_error) {
    laplace_branch_handle_t handle = {
        .id = LAPLACE_BRANCH_ID_INVALID,
        .generation = LAPLACE_BRANCH_GENERATION_INVALID
    };

    if (out_error != NULL) {
        *out_error = LAPLACE_OK;
    }

    if (system == NULL) {
        if (out_error != NULL) {
            *out_error = LAPLACE_ERR_INVALID_ARGUMENT;
        }
        return handle;
    }

    if (parent_branch.id != LAPLACE_BRANCH_ID_INVALID && !laplace__branch_handle_valid(system, parent_branch)) {
        if (out_error != NULL) {
            *out_error = LAPLACE_ERR_INVALID_ARGUMENT;
        }
        return handle;
    }

    for (uint32_t slot = 0u; slot < LAPLACE_BRANCH_MAX_BRANCHES; ++slot) {
        if (system->statuses[slot] == (uint8_t)LAPLACE_BRANCH_STATUS_INVALID ||
            system->statuses[slot] == (uint8_t)LAPLACE_BRANCH_STATUS_RETIRED) {
            handle.id = (laplace_branch_id_t)(slot + 1u);
            handle.generation = system->generations[slot];
            system->statuses[slot] = (uint8_t)LAPLACE_BRANCH_STATUS_ACTIVE;
            system->parent_ids[slot] = parent_branch.id;
            system->parent_generations[slot] = parent_branch.generation;
            system->create_epochs[slot] = system->current_epoch;
            system->close_epochs[slot] = LAPLACE_EPOCH_ID_INVALID;
            system->owned_fact_counts[slot] = 0u;
            system->owned_entity_counts[slot] = 0u;

            if (system->observe != NULL) {
                laplace_observe_trace_branch_create(system->observe, handle.id,
                    handle.generation, system->current_epoch);
            }

            return handle;
        }
    }

    if (out_error != NULL) {
        *out_error = LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    return handle;
}

bool laplace_branch_is_active(const laplace_branch_system_t* const system,
                               const laplace_branch_handle_t branch) {
    if (!laplace__branch_handle_valid(system, branch)) {
        return false;
    }

    const uint32_t slot = laplace__branch_slot(branch.id);
    return system->statuses[slot] == (uint8_t)LAPLACE_BRANCH_STATUS_ACTIVE;
}

laplace_error_t laplace_branch_get_info(const laplace_branch_system_t* const system,
                                         const laplace_branch_handle_t branch,
                                         laplace_branch_info_t* const out_info) {
    if (system == NULL || out_info == NULL || !laplace__branch_handle_valid(system, branch)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__branch_slot(branch.id);
    out_info->parent.id = system->parent_ids[slot];
    out_info->parent.generation = system->parent_generations[slot];
    out_info->status = (laplace_branch_status_t)system->statuses[slot];
    out_info->create_epoch = system->create_epochs[slot];
    out_info->close_epoch = system->close_epochs[slot];
    out_info->owned_fact_count = system->owned_fact_counts[slot];
    out_info->owned_entity_count = system->owned_entity_counts[slot];
    return LAPLACE_OK;
}

laplace_epoch_id_t laplace_branch_current_epoch(const laplace_branch_system_t* const system) {
    return (system != NULL) ? system->current_epoch : LAPLACE_EPOCH_ID_INVALID;
}

laplace_error_t laplace_branch_advance_epoch(laplace_branch_system_t* const system,
                                              laplace_epoch_id_t* const out_epoch) {
    if (system == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    system->current_epoch += 1u;
    if (system->current_epoch == LAPLACE_EPOCH_ID_INVALID) {
        system->current_epoch = 1u;
    }

    if (out_epoch != NULL) {
        *out_epoch = system->current_epoch;
    }

    if (system->observe != NULL) {
        laplace_observe_trace_epoch_advance(system->observe,
            system->current_epoch);
    }

    return LAPLACE_OK;
}

laplace_error_t laplace_branch_register_entity(laplace_branch_system_t* const system,
                                                const laplace_branch_handle_t branch,
                                                const laplace_entity_handle_t entity) {
    if (system == NULL || !laplace_branch_is_active(system, branch) ||
        !laplace_entity_pool_is_alive(system->entity_pool, entity)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    uint32_t* const count = laplace__branch_entity_count_ptr(system, branch);
    if (*count >= LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_entity_handle_t* const entities = laplace__branch_entity_base(system, branch);
    laplace_entity_handle_t* const promotions = laplace__branch_entity_promotions_base(system, branch);
    entities[*count] = entity;
    promotions[*count].id = LAPLACE_ENTITY_ID_INVALID;
    promotions[*count].generation = LAPLACE_GENERATION_INVALID;
    *count += 1u;

    const laplace_entity_branch_meta_t meta = {
        .branch_id = branch.id,
        .branch_generation = branch.generation,
        .create_epoch = system->current_epoch,
        .retire_epoch = LAPLACE_EPOCH_ID_INVALID,
        .flags = LAPLACE_ENTITY_BRANCH_FLAG_LOCAL
    };
    return laplace_entity_pool_set_branch_meta(system->entity_pool, entity, &meta);
}

laplace_error_t laplace_branch_register_constant(laplace_branch_system_t* const system,
                                                  const laplace_branch_handle_t branch,
                                                  const laplace_entity_handle_t constant,
                                                  const laplace_exact_type_id_t type_id,
                                                  const uint32_t flags) {
    laplace_error_t rc = laplace_branch_register_entity(system, branch, constant);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    return laplace_exact_register_constant(system->store, constant, type_id, flags);
}

laplace_error_t laplace_branch_insert_asserted_provenance(laplace_branch_system_t* const system,
                                                           const laplace_branch_handle_t branch,
                                                           laplace_provenance_id_t* const out_provenance_id) {
    if (system == NULL || out_provenance_id == NULL || !laplace_branch_is_active(system, branch)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_exact_provenance_desc_t desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = system->current_epoch,
        .reserved_branch = branch.id
    };
    return laplace_exact_insert_provenance(system->store, &desc, out_provenance_id);
}

laplace_error_t laplace_branch_assert_fact(laplace_branch_system_t* const system,
                                            const laplace_branch_handle_t branch,
                                            const laplace_predicate_id_t predicate_id,
                                            const laplace_entity_handle_t* const args,
                                            const uint32_t arg_count,
                                            const laplace_provenance_id_t provenance_id,
                                            const uint32_t flags,
                                            laplace_exact_fact_row_t* const out_fact_row,
                                            laplace_entity_handle_t* const out_fact_entity,
                                            bool* const out_inserted) {
    if (system == NULL || !laplace_branch_is_active(system, branch)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_error_t rc = laplace_exact_assert_fact_in_branch(system->store,
                                                             branch,
                                                             system->current_epoch,
                                                             predicate_id,
                                                             args,
                                                             arg_count,
                                                             provenance_id,
                                                             flags,
                                                             out_fact_row,
                                                             out_fact_entity,
                                                             out_inserted);
    if (rc != LAPLACE_OK || !*out_inserted) {
        return rc;
    }

    uint32_t* const fact_count = laplace__branch_fact_count_ptr(system, branch);
    if (*fact_count >= LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_exact_fact_row_t* const rows = laplace__branch_fact_base(system, branch);
    laplace_entity_handle_t* const promotions = laplace__branch_fact_promotions_base(system, branch);
    rows[*fact_count] = *out_fact_row;
    promotions[*fact_count].id = LAPLACE_ENTITY_ID_INVALID;
    promotions[*fact_count].generation = LAPLACE_GENERATION_INVALID;
    *fact_count += 1u;
    return LAPLACE_OK;
}

laplace_error_t laplace_branch_fail(laplace_branch_system_t* const system,
                                     const laplace_branch_handle_t branch) {
    if (system == NULL || !laplace_branch_is_active(system, branch)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t slot = laplace__branch_slot(branch.id);
    laplace_exact_fact_row_t* const rows = laplace__branch_fact_base(system, branch);
    for (uint32_t i = 0u; i < system->owned_fact_counts[slot]; ++i) {
        const laplace_exact_fact_row_t row = rows[i];
        if (row != LAPLACE_EXACT_FACT_ROW_INVALID) {
            (void)laplace_exact_retire_fact(system->store, row, system->current_epoch);
        }
    }

    laplace_entity_handle_t* const entities = laplace__branch_entity_base(system, branch);
    for (uint32_t i = 0u; i < system->owned_entity_counts[slot]; ++i) {
        const laplace_entity_handle_t entity = entities[i];
        if (!laplace_entity_pool_is_alive(system->entity_pool, entity)) {
            continue;
        }

        const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(system->entity_pool, entity);
        if (meta.role == LAPLACE_ENTITY_EXACT_ROLE_FACT) {
            continue;
        }

        laplace_error_t rc = laplace_entity_pool_mark_dead(system->entity_pool, entity);
        if (rc == LAPLACE_OK) {
            (void)laplace_entity_pool_mark_retired(system->entity_pool, entity, system->current_epoch);
        }
    }

    system->statuses[slot] = (uint8_t)LAPLACE_BRANCH_STATUS_FAILED;
    system->close_epochs[slot] = system->current_epoch;

    if (system->observe != NULL) {
        laplace_observe_trace_branch_fail(system->observe, branch.id,
            branch.generation, system->current_epoch);
    }

    return LAPLACE_OK;
}

laplace_error_t laplace_branch_commit(laplace_branch_system_t* const system,
                                       const laplace_branch_handle_t branch,
                                       uint32_t* const out_promoted_facts,
                                       uint32_t* const out_deduplicated_facts) {
    if (system == NULL || !laplace_branch_is_active(system, branch)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    uint32_t promoted = 0u;
    uint32_t deduplicated = 0u;
    const uint32_t slot = laplace__branch_slot(branch.id);
    laplace_exact_fact_row_t* const rows = laplace__branch_fact_base(system, branch);

    for (uint32_t i = 0u; i < system->owned_fact_counts[slot]; ++i) {
        const laplace_exact_fact_row_t row = rows[i];
        const laplace_exact_fact_t* const fact = laplace_exact_get_fact(system->store, row);
        if (fact == NULL || !laplace_exact_fact_is_active(fact)) {
            continue;
        }

        const laplace_exact_fact_row_t committed = laplace_exact_find_fact(system->store,
                                                                           fact->predicate,
                                                                           fact->args,
                                                                           fact->arity);
        if (committed != LAPLACE_EXACT_FACT_ROW_INVALID) {
            (void)laplace_exact_retire_fact(system->store, row, system->current_epoch);
            deduplicated += 1u;
            continue;
        }

        (void)laplace_exact_promote_fact(system->store, row);
        promoted += 1u;
    }

    const laplace_entity_handle_t* const entities = laplace__branch_entity_base(system, branch);
    for (uint32_t i = 0u; i < system->owned_entity_counts[slot]; ++i) {
        const laplace_entity_handle_t entity = entities[i];
        if (!laplace_entity_pool_is_alive(system->entity_pool, entity)) {
            continue;
        }

        const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(system->entity_pool, entity);
        if (meta.role == LAPLACE_ENTITY_EXACT_ROLE_CONSTANT) {
            const laplace_entity_branch_meta_t branch_meta = {
                .branch_id = LAPLACE_BRANCH_ID_INVALID,
                .branch_generation = LAPLACE_BRANCH_GENERATION_INVALID,
                .create_epoch = LAPLACE_EPOCH_ID_INVALID,
                .retire_epoch = LAPLACE_EPOCH_ID_INVALID,
                .flags = LAPLACE_ENTITY_BRANCH_FLAG_NONE
            };
            (void)laplace_entity_pool_set_branch_meta(system->entity_pool, entity, &branch_meta);
        }
    }

    system->statuses[slot] = (uint8_t)LAPLACE_BRANCH_STATUS_COMMITTED;
    system->close_epochs[slot] = system->current_epoch;

    if (out_promoted_facts != NULL) {
        *out_promoted_facts = promoted;
    }
    if (out_deduplicated_facts != NULL) {
        *out_deduplicated_facts = deduplicated;
    }

    if (system->observe != NULL) {
        laplace_observe_trace_branch_commit(system->observe, branch.id,
            branch.generation, promoted, system->current_epoch);
    }

    return LAPLACE_OK;
}

laplace_error_t laplace_branch_reclaim_closed(laplace_branch_system_t* const system,
                                               uint32_t* const out_reclaimed_branches,
                                               uint32_t* const out_reclaimed_entities) {
    if (system == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    uint32_t reclaimed_branches = 0u;
    uint32_t reclaimed_entities = 0u;

    for (uint32_t slot = 0u; slot < LAPLACE_BRANCH_MAX_BRANCHES; ++slot) {
        const uint8_t status = system->statuses[slot];
        if (status != (uint8_t)LAPLACE_BRANCH_STATUS_FAILED && status != (uint8_t)LAPLACE_BRANCH_STATUS_COMMITTED) {
            continue;
        }

        const laplace_epoch_id_t close_epoch = system->close_epochs[slot];
        if (close_epoch == LAPLACE_EPOCH_ID_INVALID || close_epoch >= system->current_epoch) {
            continue;
        }

        const laplace_branch_handle_t branch = {
            .id = (laplace_branch_id_t)(slot + 1u),
            .generation = system->generations[slot]
        };
        laplace_exact_fact_row_t* const rows = laplace__branch_fact_base(system, branch);
        for (uint32_t i = 0u; i < system->owned_fact_counts[slot]; ++i) {
            const laplace_exact_fact_t* const fact = laplace_exact_get_fact(system->store, rows[i]);
            if (fact == NULL) {
                continue;
            }

            const uint32_t entity_slot = (uint32_t)fact->entity - 1u;
            if (fact->entity == LAPLACE_ENTITY_ID_INVALID || entity_slot >= system->entity_pool->capacity) {
                continue;
            }

            const laplace_entity_handle_t fact_entity = {
                .id = fact->entity,
                .generation = system->entity_pool->generations[entity_slot]
            };
            const laplace_entity_branch_meta_t meta = laplace_entity_pool_get_branch_meta(system->entity_pool, fact_entity);
            if ((meta.flags & LAPLACE_ENTITY_BRANCH_FLAG_RETIRED) != 0u &&
                meta.retire_epoch != LAPLACE_EPOCH_ID_INVALID &&
                meta.retire_epoch < system->current_epoch) {
                if (laplace_entity_pool_reclaim_retired(system->entity_pool, fact_entity) == LAPLACE_OK) {
                    reclaimed_entities += 1u;
                }
            }
        }

        laplace_entity_handle_t* const entities = laplace__branch_entity_base(system, branch);
        for (uint32_t i = 0u; i < system->owned_entity_counts[slot]; ++i) {
            const laplace_entity_handle_t entity = entities[i];
            if (entity.id == LAPLACE_ENTITY_ID_INVALID) {
                continue;
            }

            const laplace_entity_branch_meta_t meta = laplace_entity_pool_get_branch_meta(system->entity_pool, entity);
            if ((meta.flags & LAPLACE_ENTITY_BRANCH_FLAG_RETIRED) != 0u &&
                meta.retire_epoch != LAPLACE_EPOCH_ID_INVALID &&
                meta.retire_epoch < system->current_epoch) {
                if (laplace_entity_pool_reclaim_retired(system->entity_pool, entity) == LAPLACE_OK) {
                    reclaimed_entities += 1u;
                }
            }
        }

        system->statuses[slot] = (uint8_t)LAPLACE_BRANCH_STATUS_RETIRED;
        system->close_epochs[slot] = system->current_epoch;
        system->owned_fact_counts[slot] = 0u;
        system->owned_entity_counts[slot] = 0u;
        system->generations[slot] += 1u;
        if (system->generations[slot] == LAPLACE_BRANCH_GENERATION_INVALID) {
            system->generations[slot] = 1u;
        }
        reclaimed_branches += 1u;
    }

    if (out_reclaimed_branches != NULL) {
        *out_reclaimed_branches = reclaimed_branches;
    }
    if (out_reclaimed_entities != NULL) {
        *out_reclaimed_entities = reclaimed_entities;
    }
    return LAPLACE_OK;
}

void laplace_branch_bind_observe(laplace_branch_system_t* const system,
                                 laplace_observe_context_t* const observe) {
    if (system != NULL) {
        system->observe = observe;
    }
}
