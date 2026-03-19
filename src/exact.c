#include "laplace/exact.h"

#include <string.h>

#include "laplace/observe.h"
static inline bool laplace__exact_valid_predicate_id(const laplace_predicate_id_t predicate_id) {
    return predicate_id != LAPLACE_PREDICATE_ID_INVALID && predicate_id <= LAPLACE_EXACT_MAX_PREDICATES;
}

static inline bool laplace__exact_valid_fact_row(const laplace_exact_fact_row_t fact_row,
                                                  const uint32_t fact_count) {
    return fact_row != LAPLACE_EXACT_FACT_ROW_INVALID && fact_row <= fact_count;
}

static inline uint32_t laplace__exact_entity_slot(const laplace_entity_id_t entity_id) {
    LAPLACE_ASSERT(entity_id != LAPLACE_ENTITY_ID_INVALID);
    return entity_id - 1u;
}

static uint32_t laplace__exact_hash_tuple(const laplace_predicate_id_t predicate_id,
                                           const laplace_entity_id_t* const args,
                                           const uint32_t arg_count) {
    uint32_t hash = 2166136261u;
    hash ^= (uint32_t)predicate_id;
    hash *= 16777619u;
    hash ^= arg_count;
    hash *= 16777619u;

    for (uint32_t i = 0u; i < arg_count; ++i) {
        hash ^= args[i];
        hash *= 16777619u;
    }

    if (hash == 0u) {
        hash = 1u;
    }

    return hash;
}

static bool laplace__exact_fact_matches(const laplace_exact_fact_t* const fact,
                                         const laplace_predicate_id_t predicate_id,
                                         const laplace_entity_id_t* const args,
                                         const uint32_t arg_count) {
    if (fact->predicate != predicate_id || fact->arity != (uint8_t)arg_count) {
        return false;
    }

    for (uint32_t i = 0u; i < arg_count; ++i) {
        if (fact->args[i] != args[i]) {
            return false;
        }
    }

    return true;
}

static bool laplace__exact_branch_handle_valid(const laplace_branch_handle_t branch) {
    return branch.id != LAPLACE_BRANCH_ID_INVALID &&
           branch.generation != LAPLACE_BRANCH_GENERATION_INVALID;
}

static bool laplace__exact_fact_active(const laplace_exact_fact_t* const fact) {
    return fact != NULL && (fact->flags & LAPLACE_EXACT_FACT_FLAG_RETIRED) == 0u;
}

static bool laplace__exact_fact_committed(const laplace_exact_fact_t* const fact) {
    return fact != NULL &&
           laplace__exact_fact_active(fact) &&
           fact->branch_id == LAPLACE_BRANCH_ID_INVALID &&
           fact->branch_generation == LAPLACE_BRANCH_GENERATION_INVALID;
}

static bool laplace__exact_fact_visible_to_branch(const laplace_exact_fact_t* const fact,
                                                   const laplace_branch_handle_t branch) {
    if (!laplace__exact_fact_active(fact)) {
        return false;
    }

    if (!laplace__exact_branch_handle_valid(branch)) {
        return laplace__exact_fact_committed(fact);
    }

    if (laplace__exact_fact_committed(fact)) {
        return true;
    }

    return fact->branch_id == branch.id && fact->branch_generation == branch.generation;
}

static bool laplace__exact_entity_alive(const laplace_entity_pool_t* const pool,
                                         const laplace_entity_id_t entity_id,
                                         laplace_entity_handle_t* const out_handle) {
    if (pool == NULL || entity_id == LAPLACE_ENTITY_ID_INVALID) {
        return false;
    }

    const uint32_t slot = laplace__exact_entity_slot(entity_id);
    if (slot >= pool->capacity) {
        return false;
    }

    const laplace_generation_t generation = pool->generations[slot];
    const laplace_entity_handle_t handle = {
        .id = entity_id,
        .generation = generation
    };

    if (!laplace_entity_pool_is_alive(pool, handle)) {
        return false;
    }

    if (out_handle != NULL) {
        *out_handle = handle;
    }

    return true;
}

static bool laplace__exact_entity_visible_to_branch(const laplace_entity_pool_t* const pool,
                                                     const laplace_entity_handle_t handle,
                                                     const laplace_branch_handle_t branch) {
    if (!laplace_entity_pool_is_alive(pool, handle)) {
        return false;
    }

    const laplace_entity_branch_meta_t branch_meta = laplace_entity_pool_get_branch_meta(pool, handle);
    if ((branch_meta.flags & LAPLACE_ENTITY_BRANCH_FLAG_LOCAL) == 0u) {
        return true;
    }

    if (!laplace__exact_branch_handle_valid(branch)) {
        return false;
    }

    return branch_meta.branch_id == branch.id && branch_meta.branch_generation == branch.generation;
}

static bool laplace__exact_constant_registered(const laplace_exact_store_t* const store,
                                                const laplace_entity_id_t entity_id) {
    laplace_entity_handle_t handle;
    if (!laplace__exact_entity_alive(store->entity_pool, entity_id, &handle)) {
        return false;
    }

    const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(store->entity_pool, handle);
    return meta.role == LAPLACE_ENTITY_EXACT_ROLE_CONSTANT;
}

static laplace_exact_rule_validation_result_t laplace__exact_rule_validation_make(
    const laplace_exact_rule_validation_error_t error,
    const uint32_t literal_index,
    const uint32_t term_index) {
    const laplace_exact_rule_validation_result_t result = {
        .error = error,
        .literal_index = literal_index,
        .term_index = term_index
    };
    return result;
}

laplace_error_t laplace_exact_store_init(laplace_exact_store_t* const store,
                                          laplace_arena_t* const arena,
                                          laplace_entity_pool_t* const entity_pool) {
    LAPLACE_ASSERT(store != NULL);
    LAPLACE_ASSERT(arena != NULL);
    LAPLACE_ASSERT(entity_pool != NULL);

    if (store == NULL || arena == NULL || entity_pool == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(store, 0, sizeof(*store));
    store->entity_pool = entity_pool;
    store->next_tick = 1u;

    store->predicate_declared = (uint8_t*)laplace_arena_alloc(arena,
                                                              (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint8_t),
                                                              _Alignof(uint8_t));
    store->predicate_arities = (uint8_t*)laplace_arena_alloc(arena,
                                                             (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint8_t),
                                                             _Alignof(uint8_t));
    store->predicate_flags = (uint32_t*)laplace_arena_alloc(arena,
                                                            (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t),
                                                            _Alignof(uint32_t));
    store->predicate_fact_capacities = (uint32_t*)laplace_arena_alloc(arena,
                                                                      (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t),
                                                                      _Alignof(uint32_t));
    store->predicate_row_offsets = (uint32_t*)laplace_arena_alloc(arena,
                                                                  (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t),
                                                                  _Alignof(uint32_t));
    store->predicate_row_counts = (uint32_t*)laplace_arena_alloc(arena,
                                                                 (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t),
                                                                 _Alignof(uint32_t));
    store->predicate_rows = (laplace_exact_fact_row_t*)laplace_arena_alloc(arena,
                                                                           (size_t)LAPLACE_EXACT_MAX_FACTS * sizeof(laplace_exact_fact_row_t),
                                                                           _Alignof(laplace_exact_fact_row_t));
    store->facts = (laplace_exact_fact_t*)laplace_arena_alloc(arena,
                                                              (size_t)(LAPLACE_EXACT_MAX_FACTS + 1u) * sizeof(laplace_exact_fact_t),
                                                              _Alignof(laplace_exact_fact_t));
    store->fact_hashes = (uint32_t*)laplace_arena_alloc(arena,
                                                        (size_t)LAPLACE_EXACT_FACT_INTERN_SLOTS * sizeof(uint32_t),
                                                        _Alignof(uint32_t));
    store->fact_slots = (laplace_exact_fact_row_t*)laplace_arena_alloc(arena,
                                                                       (size_t)LAPLACE_EXACT_FACT_INTERN_SLOTS * sizeof(laplace_exact_fact_row_t),
                                                                       _Alignof(laplace_exact_fact_row_t));
    store->rules = (laplace_exact_rule_t*)laplace_arena_alloc(arena,
                                                              (size_t)(LAPLACE_EXACT_MAX_RULES + 1u) * sizeof(laplace_exact_rule_t),
                                                              _Alignof(laplace_exact_rule_t));
    store->rule_literals = (laplace_exact_literal_t*)laplace_arena_alloc(arena,
                                                                          (size_t)LAPLACE_EXACT_MAX_RULE_LITERALS_TOTAL * sizeof(laplace_exact_literal_t),
                                                                          _Alignof(laplace_exact_literal_t));
    store->provenance_records = (laplace_exact_provenance_record_t*)laplace_arena_alloc(arena,
                                                                                         (size_t)(LAPLACE_EXACT_MAX_PROVENANCE_RECORDS + 1u) * sizeof(laplace_exact_provenance_record_t),
                                                                                         _Alignof(laplace_exact_provenance_record_t));
    store->provenance_parents = (laplace_entity_id_t*)laplace_arena_alloc(arena,
                                                                           (size_t)LAPLACE_EXACT_MAX_PROVENANCE_PARENTS_TOTAL * sizeof(laplace_entity_id_t),
                                                                           _Alignof(laplace_entity_id_t));

    if (store->predicate_declared == NULL ||
        store->predicate_arities == NULL ||
        store->predicate_flags == NULL ||
        store->predicate_fact_capacities == NULL ||
        store->predicate_row_offsets == NULL ||
        store->predicate_row_counts == NULL ||
        store->predicate_rows == NULL ||
        store->facts == NULL ||
        store->fact_hashes == NULL ||
        store->fact_slots == NULL ||
        store->rules == NULL ||
        store->rule_literals == NULL ||
        store->provenance_records == NULL ||
        store->provenance_parents == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    memset(store->predicate_declared, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint8_t));
    memset(store->predicate_arities, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint8_t));
    memset(store->predicate_flags, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t));
    memset(store->predicate_fact_capacities, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t));
    memset(store->predicate_row_offsets, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t));
    memset(store->predicate_row_counts, 0, (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u) * sizeof(uint32_t));
    memset(store->predicate_rows, 0, (size_t)LAPLACE_EXACT_MAX_FACTS * sizeof(laplace_exact_fact_row_t));
    memset(store->facts, 0, (size_t)(LAPLACE_EXACT_MAX_FACTS + 1u) * sizeof(laplace_exact_fact_t));
    memset(store->fact_hashes, 0, (size_t)LAPLACE_EXACT_FACT_INTERN_SLOTS * sizeof(uint32_t));
    memset(store->fact_slots, 0, (size_t)LAPLACE_EXACT_FACT_INTERN_SLOTS * sizeof(laplace_exact_fact_row_t));
    memset(store->rules, 0, (size_t)(LAPLACE_EXACT_MAX_RULES + 1u) * sizeof(laplace_exact_rule_t));
    memset(store->rule_literals, 0, (size_t)LAPLACE_EXACT_MAX_RULE_LITERALS_TOTAL * sizeof(laplace_exact_literal_t));
    memset(store->provenance_records, 0, (size_t)(LAPLACE_EXACT_MAX_PROVENANCE_RECORDS + 1u) * sizeof(laplace_exact_provenance_record_t));
    memset(store->provenance_parents, 0, (size_t)LAPLACE_EXACT_MAX_PROVENANCE_PARENTS_TOTAL * sizeof(laplace_entity_id_t));

    return LAPLACE_OK;
}

laplace_error_t laplace_exact_register_predicate(laplace_exact_store_t* const store,
                                                  const laplace_predicate_id_t predicate_id,
                                                  const laplace_exact_predicate_desc_t* const desc) {
    LAPLACE_ASSERT(store != NULL);

    if (store == NULL || desc == NULL || !laplace__exact_valid_predicate_id(predicate_id)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->arity == 0u || desc->arity > LAPLACE_EXACT_MAX_ARITY || desc->fact_capacity == 0u) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (store->predicate_declared[predicate_id] != 0u) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    if (store->predicate_scan_rows_used > LAPLACE_EXACT_MAX_FACTS - desc->fact_capacity) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    store->predicate_declared[predicate_id] = 1u;
    store->predicate_arities[predicate_id] = desc->arity;
    store->predicate_flags[predicate_id] = desc->flags;
    store->predicate_fact_capacities[predicate_id] = desc->fact_capacity;
    store->predicate_row_offsets[predicate_id] = store->predicate_scan_rows_used;
    store->predicate_row_counts[predicate_id] = 0u;
    store->predicate_scan_rows_used += desc->fact_capacity;

    if ((uint32_t)predicate_id > store->predicate_count) {
        store->predicate_count = (uint32_t)predicate_id;
    }

    return LAPLACE_OK;
}

bool laplace_exact_predicate_is_declared(const laplace_exact_store_t* const store,
                                          const laplace_predicate_id_t predicate_id) {
    if (store == NULL || !laplace__exact_valid_predicate_id(predicate_id)) {
        return false;
    }

    return store->predicate_declared[predicate_id] != 0u;
}

uint8_t laplace_exact_predicate_arity(const laplace_exact_store_t* const store,
                                       const laplace_predicate_id_t predicate_id) {
    if (!laplace_exact_predicate_is_declared(store, predicate_id)) {
        return 0u;
    }

    return store->predicate_arities[predicate_id];
}

laplace_error_t laplace_exact_register_constant(laplace_exact_store_t* const store,
                                                 const laplace_entity_handle_t constant,
                                                 const laplace_exact_type_id_t type_id,
                                                 const uint32_t flags) {
    if (store == NULL || type_id == LAPLACE_EXACT_TYPE_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_entity_pool_is_alive(store->entity_pool, constant)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const laplace_entity_exact_meta_t existing = laplace_entity_pool_get_exact_meta(store->entity_pool, constant);
    if (existing.role == LAPLACE_ENTITY_EXACT_ROLE_FACT) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const laplace_entity_exact_meta_t meta = {
        .role = LAPLACE_ENTITY_EXACT_ROLE_CONSTANT,
        .type_id = type_id,
        .fact_row = LAPLACE_EXACT_FACT_ROW_INVALID,
        .flags = flags
    };

    return laplace_entity_pool_set_exact_meta(store->entity_pool, constant, &meta);
}

bool laplace_exact_is_constant_entity(const laplace_exact_store_t* const store,
                                       const laplace_entity_id_t entity_id) {
    if (store == NULL) {
        return false;
    }

    return laplace__exact_constant_registered(store, entity_id);
}

laplace_error_t laplace_exact_insert_provenance(laplace_exact_store_t* const store,
                                                 const laplace_exact_provenance_desc_t* const desc,
                                                 laplace_provenance_id_t* const out_provenance_id) {
    if (store == NULL || desc == NULL || out_provenance_id == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->kind != LAPLACE_EXACT_PROVENANCE_ASSERTED && desc->kind != LAPLACE_EXACT_PROVENANCE_DERIVED) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (store->provenance_count >= LAPLACE_EXACT_MAX_PROVENANCE_RECORDS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    if (desc->parent_count > LAPLACE_EXACT_MAX_PROVENANCE_PARENTS) {
        return LAPLACE_ERR_OUT_OF_RANGE;
    }

    if (store->provenance_parent_count > LAPLACE_EXACT_MAX_PROVENANCE_PARENTS_TOTAL - desc->parent_count) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    if (desc->kind == LAPLACE_EXACT_PROVENANCE_ASSERTED) {
        if (desc->parent_count != 0u || desc->source_rule_id != LAPLACE_RULE_ID_INVALID) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    } else {
        if (desc->source_rule_id == LAPLACE_RULE_ID_INVALID || desc->parent_count == 0u) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    const laplace_provenance_id_t provenance_id = (laplace_provenance_id_t)(store->provenance_count + 1u);
    laplace_exact_provenance_record_t* const record = &store->provenance_records[provenance_id];
    record->kind = desc->kind;
    record->source_rule_id = desc->source_rule_id;
    record->parent_offset = store->provenance_parent_count;
    record->parent_count = desc->parent_count;
    record->tick = store->next_tick++;
    record->reserved_epoch = desc->reserved_epoch;
    record->reserved_branch = desc->reserved_branch;

    for (uint32_t i = 0u; i < desc->parent_count; ++i) {
        laplace_entity_handle_t parent_handle;
        if (!laplace__exact_entity_alive(store->entity_pool, desc->parent_facts[i], &parent_handle)) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
        const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(store->entity_pool, parent_handle);
        if (meta.role != LAPLACE_ENTITY_EXACT_ROLE_FACT) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
        store->provenance_parents[store->provenance_parent_count + i] = desc->parent_facts[i];
    }

    store->provenance_parent_count += desc->parent_count;
    store->provenance_count += 1u;
    *out_provenance_id = provenance_id;
    return LAPLACE_OK;
}

const laplace_exact_provenance_record_t* laplace_exact_get_provenance(const laplace_exact_store_t* const store,
                                                                       const laplace_provenance_id_t provenance_id) {
    if (store == NULL || provenance_id == LAPLACE_PROVENANCE_ID_INVALID || provenance_id > store->provenance_count) {
        return NULL;
    }

    return &store->provenance_records[provenance_id];
}

const laplace_entity_id_t* laplace_exact_get_provenance_parents(const laplace_exact_store_t* const store,
                                                                 const laplace_provenance_id_t provenance_id,
                                                                 uint32_t* const out_parent_count) {
    if (out_parent_count != NULL) {
        *out_parent_count = 0u;
    }

    const laplace_exact_provenance_record_t* const record = laplace_exact_get_provenance(store, provenance_id);
    if (record == NULL) {
        return NULL;
    }

    if (out_parent_count != NULL) {
        *out_parent_count = record->parent_count;
    }

    return &store->provenance_parents[record->parent_offset];
}

laplace_exact_fact_row_t laplace_exact_find_fact(const laplace_exact_store_t* const store,
                                                  const laplace_predicate_id_t predicate_id,
                                                  const laplace_entity_id_t* const args,
                                                  const uint32_t arg_count) {
    const laplace_branch_handle_t committed_branch = {
        .id = LAPLACE_BRANCH_ID_INVALID,
        .generation = LAPLACE_BRANCH_GENERATION_INVALID
    };
    return laplace_exact_find_fact_in_branch(store, committed_branch, predicate_id, args, arg_count);
}

laplace_exact_fact_row_t laplace_exact_find_fact_in_branch(const laplace_exact_store_t* const store,
                                                            const laplace_branch_handle_t branch,
                                                            const laplace_predicate_id_t predicate_id,
                                                            const laplace_entity_id_t* const args,
                                                            const uint32_t arg_count) {
    if (store == NULL || args == NULL || !laplace_exact_predicate_is_declared(store, predicate_id)) {
        return LAPLACE_EXACT_FACT_ROW_INVALID;
    }

    if (arg_count != store->predicate_arities[predicate_id]) {
        return LAPLACE_EXACT_FACT_ROW_INVALID;
    }

    const uint32_t hash = laplace__exact_hash_tuple(predicate_id, args, arg_count);
    uint32_t slot = hash % LAPLACE_EXACT_FACT_INTERN_SLOTS;

    for (uint32_t probe = 0u; probe < LAPLACE_EXACT_FACT_INTERN_SLOTS; ++probe) {
        if (store->fact_slots[slot] == LAPLACE_EXACT_FACT_ROW_INVALID) {
            return LAPLACE_EXACT_FACT_ROW_INVALID;
        }

        if (store->fact_hashes[slot] == hash) {
            const laplace_exact_fact_row_t candidate_row = store->fact_slots[slot];
            const laplace_exact_fact_t* const fact = &store->facts[candidate_row];
            if (laplace__exact_fact_matches(fact, predicate_id, args, arg_count) &&
                laplace__exact_fact_visible_to_branch(fact, branch)) {
                return candidate_row;
            }
        }

        slot = (slot + 1u) % LAPLACE_EXACT_FACT_INTERN_SLOTS;
    }

    return LAPLACE_EXACT_FACT_ROW_INVALID;
}

laplace_error_t laplace_exact_assert_fact(laplace_exact_store_t* const store,
                                           const laplace_predicate_id_t predicate_id,
                                           const laplace_entity_handle_t* const args,
                                           const uint32_t arg_count,
                                           const laplace_provenance_id_t provenance_id,
                                           const uint32_t flags,
                                           laplace_exact_fact_row_t* const out_fact_row,
                                           laplace_entity_handle_t* const out_fact_entity,
                                           bool* const out_inserted) {
    const laplace_branch_handle_t committed_branch = {
        .id = LAPLACE_BRANCH_ID_INVALID,
        .generation = LAPLACE_BRANCH_GENERATION_INVALID
    };
    return laplace_exact_assert_fact_in_branch(store,
                                               committed_branch,
                                               LAPLACE_EPOCH_ID_INVALID,
                                               predicate_id,
                                               args,
                                               arg_count,
                                               provenance_id,
                                               flags,
                                               out_fact_row,
                                               out_fact_entity,
                                               out_inserted);
}

laplace_error_t laplace_exact_assert_fact_in_branch(laplace_exact_store_t* const store,
                                                     const laplace_branch_handle_t branch,
                                                     const laplace_epoch_id_t create_epoch,
                                                     const laplace_predicate_id_t predicate_id,
                                                     const laplace_entity_handle_t* const args,
                                                     const uint32_t arg_count,
                                                     const laplace_provenance_id_t provenance_id,
                                                     const uint32_t flags,
                                                     laplace_exact_fact_row_t* const out_fact_row,
                                                     laplace_entity_handle_t* const out_fact_entity,
                                                     bool* const out_inserted) {
    if (store == NULL || args == NULL || out_fact_row == NULL || out_fact_entity == NULL || out_inserted == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_exact_predicate_is_declared(store, predicate_id) || arg_count != store->predicate_arities[predicate_id]) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (provenance_id == LAPLACE_PROVENANCE_ID_INVALID || provenance_id > store->provenance_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_entity_id_t arg_ids[LAPLACE_EXACT_MAX_ARITY] = {0};
    for (uint32_t i = 0u; i < arg_count; ++i) {
        if (!laplace__exact_entity_visible_to_branch(store->entity_pool, args[i], branch)) {
            return LAPLACE_ERR_INVALID_STATE;
        }

        const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(store->entity_pool, args[i]);
        if (meta.role != LAPLACE_ENTITY_EXACT_ROLE_CONSTANT) {
            return LAPLACE_ERR_INVALID_STATE;
        }

        arg_ids[i] = args[i].id;
    }

    const laplace_exact_fact_row_t existing = laplace_exact_find_fact_in_branch(store, branch, predicate_id, arg_ids, arg_count);
    if (existing != LAPLACE_EXACT_FACT_ROW_INVALID) {
        const laplace_exact_fact_t* const fact = &store->facts[existing];
        laplace_entity_handle_t fact_handle;
        if (!laplace__exact_entity_alive(store->entity_pool, fact->entity, &fact_handle)) {
            return LAPLACE_ERR_INTERNAL;
        }

        *out_fact_row = existing;
        *out_fact_entity = fact_handle;
        *out_inserted = false;

        if (store->observe != NULL) {
            laplace_observe_trace_fact_duplicate(store->observe, predicate_id,
                branch.id, branch.generation, store->next_tick);
        }

        return LAPLACE_OK;
    }

    if (store->fact_count >= LAPLACE_EXACT_MAX_FACTS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t predicate_row_count = store->predicate_row_counts[predicate_id];
    const uint32_t predicate_capacity = store->predicate_fact_capacities[predicate_id];
    if (predicate_row_count >= predicate_capacity) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const laplace_entity_handle_t fact_entity = laplace_entity_pool_alloc(store->entity_pool);
    if (fact_entity.id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    laplace_error_t rc = laplace_entity_pool_set_state(store->entity_pool, fact_entity, LAPLACE_STATE_READY);
    if (rc != LAPLACE_OK) {
        (void)laplace_entity_pool_free(store->entity_pool, fact_entity);
        return rc;
    }

    const laplace_exact_fact_row_t fact_row = (laplace_exact_fact_row_t)(store->fact_count + 1u);
    laplace_exact_fact_t* const fact = &store->facts[fact_row];
    memset(fact, 0, sizeof(*fact));
    fact->entity = fact_entity.id;
    fact->predicate = predicate_id;
    fact->arity = (uint8_t)arg_count;
    fact->provenance = provenance_id;
    fact->branch_id = branch.id;
    fact->branch_generation = branch.generation;
    fact->create_epoch = create_epoch;
    fact->retire_epoch = LAPLACE_EPOCH_ID_INVALID;
    fact->flags = flags;
    if (laplace__exact_branch_handle_valid(branch)) {
        fact->flags |= LAPLACE_EXACT_FACT_FLAG_BRANCH_LOCAL;
        fact->flags &= ~(uint32_t)LAPLACE_EXACT_FACT_FLAG_COMMITTED;
    } else {
        fact->flags |= LAPLACE_EXACT_FACT_FLAG_COMMITTED;
        fact->flags &= ~(uint32_t)LAPLACE_EXACT_FACT_FLAG_BRANCH_LOCAL;
    }
    for (uint32_t i = 0u; i < arg_count; ++i) {
        fact->args[i] = arg_ids[i];
    }

    const uint32_t hash = laplace__exact_hash_tuple(predicate_id, arg_ids, arg_count);
    uint32_t slot = hash % LAPLACE_EXACT_FACT_INTERN_SLOTS;
    uint32_t probe_count = 0u;
    while (store->fact_slots[slot] != LAPLACE_EXACT_FACT_ROW_INVALID) {
        slot = (slot + 1u) % LAPLACE_EXACT_FACT_INTERN_SLOTS;
        ++probe_count;
        if (probe_count >= LAPLACE_EXACT_FACT_INTERN_SLOTS) {
            (void)laplace_entity_pool_free(store->entity_pool, fact_entity);
            return LAPLACE_ERR_CAPACITY_EXHAUSTED;
        }
    }

    const laplace_entity_exact_meta_t meta = {
        .role = LAPLACE_ENTITY_EXACT_ROLE_FACT,
        .type_id = LAPLACE_EXACT_TYPE_ID_INVALID,
        .fact_row = fact_row,
        .flags = fact->flags
    };
    rc = laplace_entity_pool_set_exact_meta(store->entity_pool, fact_entity, &meta);
    if (rc != LAPLACE_OK) {
        (void)laplace_entity_pool_free(store->entity_pool, fact_entity);
        memset(fact, 0, sizeof(*fact));
        return rc;
    }

    const laplace_entity_branch_meta_t branch_meta = {
        .branch_id = branch.id,
        .branch_generation = branch.generation,
        .create_epoch = create_epoch,
        .retire_epoch = LAPLACE_EPOCH_ID_INVALID,
        .flags = laplace__exact_branch_handle_valid(branch) ? LAPLACE_ENTITY_BRANCH_FLAG_LOCAL : LAPLACE_ENTITY_BRANCH_FLAG_NONE
    };
    rc = laplace_entity_pool_set_branch_meta(store->entity_pool, fact_entity, &branch_meta);
    if (rc != LAPLACE_OK) {
        (void)laplace_entity_pool_free(store->entity_pool, fact_entity);
        memset(fact, 0, sizeof(*fact));
        return rc;
    }

    store->fact_hashes[slot] = hash;
    store->fact_slots[slot] = fact_row;

    const uint32_t predicate_offset = store->predicate_row_offsets[predicate_id];
    store->predicate_rows[predicate_offset + predicate_row_count] = fact_row;
    store->predicate_row_counts[predicate_id] = predicate_row_count + 1u;
    store->fact_count += 1u;

    *out_fact_row = fact_row;
    *out_fact_entity = fact_entity;
    *out_inserted = true;

    if (store->observe != NULL) {
        const uint32_t f = flags;
        if ((f & LAPLACE_EXACT_FACT_FLAG_DERIVED) != 0u) {
            laplace_observe_trace_fact_derived(store->observe, fact_row, predicate_id,
                fact_entity.id, provenance_id, LAPLACE_RULE_ID_INVALID,
                branch.id, branch.generation, create_epoch, store->next_tick);
        } else {
            laplace_observe_trace_fact_asserted(store->observe, fact_row, predicate_id,
                fact_entity.id, provenance_id,
                branch.id, branch.generation, create_epoch, store->next_tick);
        }
    }

    return LAPLACE_OK;
}

const laplace_exact_fact_t* laplace_exact_get_fact(const laplace_exact_store_t* const store,
                                                    const laplace_exact_fact_row_t fact_row) {
    if (store == NULL || !laplace__exact_valid_fact_row(fact_row, store->fact_count)) {
        return NULL;
    }

    return &store->facts[fact_row];
}

bool laplace_exact_fact_is_active(const laplace_exact_fact_t* const fact) {
    return laplace__exact_fact_active(fact);
}

bool laplace_exact_fact_is_committed(const laplace_exact_fact_t* const fact) {
    return laplace__exact_fact_committed(fact);
}

bool laplace_exact_fact_visible_to_branch(const laplace_exact_fact_t* const fact,
                                           const laplace_branch_handle_t branch) {
    return laplace__exact_fact_visible_to_branch(fact, branch);
}

laplace_error_t laplace_exact_promote_fact(laplace_exact_store_t* const store,
                                            const laplace_exact_fact_row_t fact_row) {
    if (store == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_exact_fact_t* const fact = (laplace_exact_fact_t*)laplace_exact_get_fact(store, fact_row);
    if (fact == NULL || !laplace__exact_fact_active(fact)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    fact->branch_id = LAPLACE_BRANCH_ID_INVALID;
    fact->branch_generation = LAPLACE_BRANCH_GENERATION_INVALID;
    fact->create_epoch = LAPLACE_EPOCH_ID_INVALID;
    fact->retire_epoch = LAPLACE_EPOCH_ID_INVALID;
    fact->flags &= ~(uint32_t)LAPLACE_EXACT_FACT_FLAG_BRANCH_LOCAL;
    fact->flags &= ~(uint32_t)LAPLACE_EXACT_FACT_FLAG_RETIRED;
    fact->flags |= LAPLACE_EXACT_FACT_FLAG_COMMITTED;

    laplace_entity_handle_t handle;
    if (!laplace__exact_entity_alive(store->entity_pool, fact->entity, &handle)) {
        return LAPLACE_ERR_INTERNAL;
    }

    const laplace_entity_branch_meta_t branch_meta = {
        .branch_id = LAPLACE_BRANCH_ID_INVALID,
        .branch_generation = LAPLACE_BRANCH_GENERATION_INVALID,
        .create_epoch = LAPLACE_EPOCH_ID_INVALID,
        .retire_epoch = LAPLACE_EPOCH_ID_INVALID,
        .flags = LAPLACE_ENTITY_BRANCH_FLAG_NONE
    };
    laplace_error_t rc = laplace_entity_pool_set_branch_meta(store->entity_pool, handle, &branch_meta);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(store->entity_pool, handle);
    meta.flags = fact->flags;
    return laplace_entity_pool_set_exact_meta(store->entity_pool, handle, &meta);
}

laplace_error_t laplace_exact_retire_fact(laplace_exact_store_t* const store,
                                           const laplace_exact_fact_row_t fact_row,
                                           const laplace_epoch_id_t retire_epoch) {
    if (store == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_exact_fact_t* const fact = (laplace_exact_fact_t*)laplace_exact_get_fact(store, fact_row);
    if (fact == NULL || !laplace__exact_fact_active(fact)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    fact->retire_epoch = retire_epoch;
    fact->flags |= LAPLACE_EXACT_FACT_FLAG_RETIRED;
    fact->flags &= ~(uint32_t)LAPLACE_EXACT_FACT_FLAG_COMMITTED;

    laplace_entity_handle_t handle;
    if (!laplace__exact_entity_alive(store->entity_pool, fact->entity, &handle)) {
        return LAPLACE_ERR_INTERNAL;
    }

    laplace_error_t rc = laplace_entity_pool_mark_dead(store->entity_pool, handle);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    return laplace_entity_pool_mark_retired(store->entity_pool, handle, retire_epoch);
}

laplace_exact_predicate_view_t laplace_exact_predicate_rows(const laplace_exact_store_t* const store,
                                                             const laplace_predicate_id_t predicate_id) {
    laplace_exact_predicate_view_t view = {0};
    if (!laplace_exact_predicate_is_declared(store, predicate_id)) {
        return view;
    }

    view.rows = &store->predicate_rows[store->predicate_row_offsets[predicate_id]];
    view.count = store->predicate_row_counts[predicate_id];
    return view;
}

laplace_exact_rule_validation_result_t laplace_exact_validate_rule(const laplace_exact_store_t* const store,
                                                                    const laplace_exact_rule_desc_t* const rule_desc) {
    if (store == NULL || rule_desc == NULL) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_NULL_RULE, 0u, 0u);
    }

    if (rule_desc->body_count == 0u) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_BODY_REQUIRED, 0u, 0u);
    }

    if (rule_desc->body_literals == NULL) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_BODY_LITERALS_NULL, 0u, 0u);
    }

    if (rule_desc->body_count > LAPLACE_EXACT_MAX_RULE_BODY_LITERALS) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_BODY_TOO_LARGE, 0u, 0u);
    }

    if (!laplace_exact_predicate_is_declared(store, rule_desc->head.predicate)) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_HEAD_PREDICATE_UNDECLARED, 0u, 0u);
    }

    if (rule_desc->head.arity != laplace_exact_predicate_arity(store, rule_desc->head.predicate)) {
        return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_ARITY_MISMATCH, 0u, 0u);
    }

    bool body_variables[LAPLACE_EXACT_MAX_RULE_VARIABLES] = {false};

    for (uint32_t literal_index = 0u; literal_index < rule_desc->body_count; ++literal_index) {
        const laplace_exact_literal_t* const literal = &rule_desc->body_literals[literal_index];
        if (!laplace_exact_predicate_is_declared(store, literal->predicate)) {
            return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_BODY_PREDICATE_UNDECLARED,
                                                       literal_index,
                                                       0u);
        }

        if (literal->arity != laplace_exact_predicate_arity(store, literal->predicate)) {
            return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_ARITY_MISMATCH,
                                                       literal_index,
                                                       0u);
        }

        for (uint32_t term_index = 0u; term_index < literal->arity; ++term_index) {
            const laplace_exact_term_t* const term = &literal->terms[term_index];
            if (term->kind == LAPLACE_EXACT_TERM_VARIABLE) {
                if (term->value.variable == LAPLACE_EXACT_VAR_ID_INVALID ||
                    term->value.variable >= LAPLACE_EXACT_MAX_RULE_VARIABLES) {
                    return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_VARIABLE_OUT_OF_RANGE,
                                                               literal_index,
                                                               term_index);
                }
                body_variables[term->value.variable] = true;
            } else if (term->kind == LAPLACE_EXACT_TERM_CONSTANT) {
                if (!laplace__exact_entity_alive(store->entity_pool, term->value.constant, NULL)) {
                    return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_ALIVE,
                                                               literal_index,
                                                               term_index);
                }
                if (!laplace__exact_constant_registered(store, term->value.constant)) {
                    return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_CONSTANT,
                                                               literal_index,
                                                               term_index);
                }
            } else {
                return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_TERM_KIND_INVALID,
                                                           literal_index,
                                                           term_index);
            }
        }
    }

    for (uint32_t term_index = 0u; term_index < rule_desc->head.arity; ++term_index) {
        const laplace_exact_term_t* const term = &rule_desc->head.terms[term_index];
        if (term->kind == LAPLACE_EXACT_TERM_VARIABLE) {
            if (term->value.variable == LAPLACE_EXACT_VAR_ID_INVALID ||
                term->value.variable >= LAPLACE_EXACT_MAX_RULE_VARIABLES) {
                return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_VARIABLE_OUT_OF_RANGE,
                                                           UINT32_MAX,
                                                           term_index);
            }
            if (!body_variables[term->value.variable]) {
                return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_HEAD_VARIABLE_MISSING_FROM_BODY,
                                                           UINT32_MAX,
                                                           term_index);
            }
        } else if (term->kind == LAPLACE_EXACT_TERM_CONSTANT) {
            if (!laplace__exact_entity_alive(store->entity_pool, term->value.constant, NULL)) {
                return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_ALIVE,
                                                           UINT32_MAX,
                                                           term_index);
            }
            if (!laplace__exact_constant_registered(store, term->value.constant)) {
                return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_CONSTANT,
                                                           UINT32_MAX,
                                                           term_index);
            }
        } else {
            return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_TERM_KIND_INVALID,
                                                       UINT32_MAX,
                                                       term_index);
        }
    }

    return laplace__exact_rule_validation_make(LAPLACE_EXACT_RULE_VALIDATION_OK, 0u, 0u);
}

laplace_error_t laplace_exact_add_rule(laplace_exact_store_t* const store,
                                        const laplace_exact_rule_desc_t* const rule_desc,
                                        laplace_rule_id_t* const out_rule_id,
                                        laplace_exact_rule_validation_result_t* const out_validation) {
    if (store == NULL || rule_desc == NULL || out_rule_id == NULL || out_validation == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    *out_validation = laplace_exact_validate_rule(store, rule_desc);
    if (out_validation->error != LAPLACE_EXACT_RULE_VALIDATION_OK) {
        if (store->observe != NULL) {
            laplace_observe_trace_rule_rejected(store->observe,
                LAPLACE_RULE_ID_INVALID, (uint32_t)out_validation->error,
                store->next_tick);
        }
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (store->rule_count >= LAPLACE_EXACT_MAX_RULES) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    if (store->rule_literal_count > LAPLACE_EXACT_MAX_RULE_LITERALS_TOTAL - rule_desc->body_count) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const laplace_rule_id_t rule_id = (laplace_rule_id_t)(store->rule_count + 1u);
    laplace_exact_rule_t* const rule = &store->rules[rule_id];
    memset(rule, 0, sizeof(*rule));
    rule->id = rule_id;
    rule->head = rule_desc->head;
    rule->body_offset = store->rule_literal_count;
    rule->body_count = rule_desc->body_count;
    rule->status = LAPLACE_EXACT_RULE_STATUS_VALID;

    for (uint32_t i = 0u; i < rule_desc->body_count; ++i) {
        store->rule_literals[store->rule_literal_count + i] = rule_desc->body_literals[i];
    }

    store->rule_literal_count += rule_desc->body_count;
    store->rule_count += 1u;
    *out_rule_id = rule_id;

    if (store->observe != NULL) {
        laplace_observe_trace_rule_accepted(store->observe, rule_id, store->next_tick);
    }

    return LAPLACE_OK;
}

const laplace_exact_rule_t* laplace_exact_get_rule(const laplace_exact_store_t* const store,
                                                    const laplace_rule_id_t rule_id) {
    if (store == NULL || rule_id == LAPLACE_RULE_ID_INVALID || rule_id > store->rule_count) {
        return NULL;
    }

    return &store->rules[rule_id];
}

const laplace_exact_literal_t* laplace_exact_rule_body_literals(const laplace_exact_store_t* const store,
                                                                 const laplace_exact_rule_t* const rule,
                                                                 uint32_t* const out_count) {
    if (out_count != NULL) {
        *out_count = 0u;
    }

    if (store == NULL || rule == NULL) {
        return NULL;
    }

    if (out_count != NULL) {
        *out_count = rule->body_count;
    }

    return &store->rule_literals[rule->body_offset];
}

const char* laplace_exact_rule_validation_error_string(const laplace_exact_rule_validation_error_t error) {
    switch (error) {
        case LAPLACE_EXACT_RULE_VALIDATION_OK:
            return "LAPLACE_EXACT_RULE_VALIDATION_OK";
        case LAPLACE_EXACT_RULE_VALIDATION_NULL_RULE:
            return "LAPLACE_EXACT_RULE_VALIDATION_NULL_RULE";
        case LAPLACE_EXACT_RULE_VALIDATION_BODY_REQUIRED:
            return "LAPLACE_EXACT_RULE_VALIDATION_BODY_REQUIRED";
        case LAPLACE_EXACT_RULE_VALIDATION_BODY_TOO_LARGE:
            return "LAPLACE_EXACT_RULE_VALIDATION_BODY_TOO_LARGE";
        case LAPLACE_EXACT_RULE_VALIDATION_HEAD_PREDICATE_UNDECLARED:
            return "LAPLACE_EXACT_RULE_VALIDATION_HEAD_PREDICATE_UNDECLARED";
        case LAPLACE_EXACT_RULE_VALIDATION_BODY_PREDICATE_UNDECLARED:
            return "LAPLACE_EXACT_RULE_VALIDATION_BODY_PREDICATE_UNDECLARED";
        case LAPLACE_EXACT_RULE_VALIDATION_ARITY_MISMATCH:
            return "LAPLACE_EXACT_RULE_VALIDATION_ARITY_MISMATCH";
        case LAPLACE_EXACT_RULE_VALIDATION_TERM_KIND_INVALID:
            return "LAPLACE_EXACT_RULE_VALIDATION_TERM_KIND_INVALID";
        case LAPLACE_EXACT_RULE_VALIDATION_VARIABLE_OUT_OF_RANGE:
            return "LAPLACE_EXACT_RULE_VALIDATION_VARIABLE_OUT_OF_RANGE";
        case LAPLACE_EXACT_RULE_VALIDATION_HEAD_VARIABLE_MISSING_FROM_BODY:
            return "LAPLACE_EXACT_RULE_VALIDATION_HEAD_VARIABLE_MISSING_FROM_BODY";
        case LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_REGISTERED:
            return "LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_REGISTERED";
        case LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_ALIVE:
            return "LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_ALIVE";
        case LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_CONSTANT:
            return "LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_CONSTANT";
        case LAPLACE_EXACT_RULE_VALIDATION_BODY_LITERALS_NULL:
            return "LAPLACE_EXACT_RULE_VALIDATION_BODY_LITERALS_NULL";
        default:
            return "LAPLACE_EXACT_RULE_VALIDATION_UNKNOWN";
    }
}

void laplace_exact_bind_observe(laplace_exact_store_t* const store,
                                laplace_observe_context_t* const observe) {
    if (store != NULL) {
        store->observe = observe;
    }
}