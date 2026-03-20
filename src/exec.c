#include "laplace/exec.h"

#include <string.h>

#include "laplace/observe.h"

static inline uint32_t laplace__exec_entity_slot(const laplace_entity_id_t entity_id) {
    LAPLACE_ASSERT(entity_id != LAPLACE_ENTITY_ID_INVALID);
    return entity_id - 1u;
}

static inline bool laplace__exec_entity_is_ready_fact(const laplace_exec_context_t* const ctx,
                                                       const laplace_entity_id_t entity_id) {
    if (entity_id == LAPLACE_ENTITY_ID_INVALID) {
        return false;
    }

    const uint32_t slot = laplace__exec_entity_slot(entity_id);
    if (slot >= ctx->entity_pool->capacity) {
        return false;
    }

    if (ctx->entity_pool->states[slot] != LAPLACE_STATE_READY) {
        return false;
    }

    if ((laplace_entity_exact_role_t)ctx->entity_pool->exact_roles[slot] != LAPLACE_ENTITY_EXACT_ROLE_FACT) {
        return false;
    }

    const laplace_entity_handle_t handle = {
        .id = entity_id,
        .generation = ctx->entity_pool->generations[slot]
    };
    const laplace_entity_branch_meta_t branch_meta = laplace_entity_pool_get_branch_meta(ctx->entity_pool, handle);
    if ((branch_meta.flags & LAPLACE_ENTITY_BRANCH_FLAG_LOCAL) != 0u) {
        if (ctx->active_branch.id == LAPLACE_BRANCH_ID_INVALID ||
            branch_meta.branch_id != ctx->active_branch.id ||
            branch_meta.branch_generation != ctx->active_branch.generation) {
            return false;
        }
    }

    return true;
}

static bool laplace__exec_fact_visible(const laplace_exec_context_t* const ctx,
                                        const laplace_exact_fact_t* const fact) {
    return laplace_exact_fact_visible_to_branch(fact, ctx->active_branch);
}

static bool laplace__exec_queue_push(laplace_exec_context_t* const ctx,
                                      const laplace_entity_id_t entity_id) {
    if (ctx->ready_queue_count >= LAPLACE_EXEC_READY_QUEUE_CAPACITY) {
        return false;
    }

    ctx->ready_queue[ctx->ready_queue_tail] = entity_id;
    ctx->ready_queue_tail = (ctx->ready_queue_tail + 1u) % LAPLACE_EXEC_READY_QUEUE_CAPACITY;
    ctx->ready_queue_count += 1u;
    return true;
}

static laplace_entity_id_t laplace__exec_queue_pop(laplace_exec_context_t* const ctx) {
    if (ctx->ready_queue_count == 0u) {
        return LAPLACE_ENTITY_ID_INVALID;
    }

    const laplace_entity_id_t entity_id = ctx->ready_queue[ctx->ready_queue_head];
    ctx->ready_queue_head = (ctx->ready_queue_head + 1u) % LAPLACE_EXEC_READY_QUEUE_CAPACITY;
    ctx->ready_queue_count -= 1u;
    return entity_id;
}

static void laplace__exec_clear_ready(laplace_exec_context_t* const ctx) {
    laplace_bitset_clear_all(&ctx->ready_bitset);
    ctx->ready_queue_head = 0u;
    ctx->ready_queue_tail = 0u;
    ctx->ready_queue_count = 0u;
}

static laplace_entity_handle_t laplace__exec_make_handle(const laplace_entity_pool_t* const pool,
                                                          const laplace_entity_id_t entity_id) {
    const uint32_t slot = laplace__exec_entity_slot(entity_id);
    const laplace_entity_handle_t handle = {
        .id = entity_id,
        .generation = pool->generations[slot]
    };
    return handle;
}

static laplace_exact_fact_row_t laplace__exec_entity_fact_row(const laplace_exec_context_t* const ctx,
                                                               const laplace_entity_id_t entity_id) {
    if (entity_id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_EXACT_FACT_ROW_INVALID;
    }

    const uint32_t slot = laplace__exec_entity_slot(entity_id);
    if (slot >= ctx->entity_pool->capacity) {
        return LAPLACE_EXACT_FACT_ROW_INVALID;
    }

    if ((laplace_entity_exact_role_t)ctx->entity_pool->exact_roles[slot] != LAPLACE_ENTITY_EXACT_ROLE_FACT) {
        return LAPLACE_EXACT_FACT_ROW_INVALID;
    }

    return ctx->entity_pool->exact_fact_rows[slot];
}

laplace_error_t laplace_exec_init(laplace_exec_context_t* const ctx,
                                   laplace_arena_t* const arena,
                                   laplace_exact_store_t* const store,
                                   laplace_entity_pool_t* const entity_pool) {
    LAPLACE_ASSERT(ctx != NULL);
    LAPLACE_ASSERT(arena != NULL);
    LAPLACE_ASSERT(store != NULL);
    LAPLACE_ASSERT(entity_pool != NULL);

    if (ctx == NULL || arena == NULL || store == NULL || entity_pool == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->store = store;
    ctx->entity_pool = entity_pool;
    ctx->mode = LAPLACE_EXEC_MODE_DENSE;
    ctx->active_branch.id = LAPLACE_BRANCH_ID_INVALID;
    ctx->active_branch.generation = LAPLACE_BRANCH_GENERATION_INVALID;
    ctx->entity_capacity = entity_pool->capacity;
    ctx->max_steps = LAPLACE_EXEC_MAX_STEPS_PER_RUN;
    ctx->max_derivations = LAPLACE_EXEC_MAX_DERIVATIONS_PER_RUN;

    const uint32_t bitset_word_count = (uint32_t)LAPLACE_BITSET_WORDS_FOR_BITS(entity_pool->capacity);
    uint64_t* const bitset_words = (uint64_t*)laplace_arena_alloc(
        arena,
        (size_t)bitset_word_count * sizeof(uint64_t),
        _Alignof(uint64_t));
    if (bitset_words == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    laplace_bitset_init(&ctx->ready_bitset, bitset_words, entity_pool->capacity, bitset_word_count);

    ctx->ready_queue = (uint32_t*)laplace_arena_alloc(
        arena,
        (size_t)LAPLACE_EXEC_READY_QUEUE_CAPACITY * sizeof(uint32_t),
        _Alignof(uint32_t));
    if (ctx->ready_queue == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    memset(ctx->ready_queue, 0, (size_t)LAPLACE_EXEC_READY_QUEUE_CAPACITY * sizeof(uint32_t));

    const size_t pred_array_size = (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u);

    ctx->trigger_entries = (laplace_exec_trigger_entry_t*)laplace_arena_alloc(
        arena,
        (size_t)LAPLACE_EXEC_MAX_TRIGGER_ENTRIES * sizeof(laplace_exec_trigger_entry_t),
        _Alignof(laplace_exec_trigger_entry_t));
    if (ctx->trigger_entries == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    ctx->trigger_offsets = (uint32_t*)laplace_arena_alloc(
        arena,
        pred_array_size * sizeof(uint32_t),
        _Alignof(uint32_t));
    if (ctx->trigger_offsets == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    ctx->trigger_counts = (uint32_t*)laplace_arena_alloc(
        arena,
        pred_array_size * sizeof(uint32_t),
        _Alignof(uint32_t));
    if (ctx->trigger_counts == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    memset(ctx->trigger_entries, 0, (size_t)LAPLACE_EXEC_MAX_TRIGGER_ENTRIES * sizeof(laplace_exec_trigger_entry_t));
    memset(ctx->trigger_offsets, 0, pred_array_size * sizeof(uint32_t));
    memset(ctx->trigger_counts, 0, pred_array_size * sizeof(uint32_t));
    ctx->trigger_entry_count = 0u;
    ctx->trigger_index_built = false;

    ctx->frontier_starts = (uint32_t*)laplace_arena_alloc(
        arena,
        pred_array_size * sizeof(uint32_t),
        _Alignof(uint32_t));
    if (ctx->frontier_starts == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    memset(ctx->frontier_starts, 0, pred_array_size * sizeof(uint32_t));
    ctx->frontier_fact_count = 0u;
    ctx->semi_naive = false;

    return LAPLACE_OK;
}

void laplace_exec_reset(laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return;
    }

    laplace__exec_clear_ready(ctx);
    memset(&ctx->stats, 0, sizeof(ctx->stats));

    const size_t pred_array_size = (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u);
    memset(ctx->trigger_entries, 0, (size_t)LAPLACE_EXEC_MAX_TRIGGER_ENTRIES * sizeof(laplace_exec_trigger_entry_t));
    memset(ctx->trigger_offsets, 0, pred_array_size * sizeof(uint32_t));
    memset(ctx->trigger_counts, 0, pred_array_size * sizeof(uint32_t));
    ctx->trigger_entry_count = 0u;
    ctx->trigger_index_built = false;

    if (ctx->frontier_starts != NULL) {
        memset(ctx->frontier_starts, 0, pred_array_size * sizeof(uint32_t));
    }
    ctx->frontier_fact_count = 0u;
}

void laplace_exec_set_mode(laplace_exec_context_t* const ctx, const laplace_exec_mode_t mode) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx != NULL) {
        ctx->mode = mode;
    }
}

void laplace_exec_set_max_steps(laplace_exec_context_t* const ctx, const uint32_t max_steps) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx != NULL) {
        ctx->max_steps = (max_steps == 0u) ? LAPLACE_EXEC_MAX_STEPS_PER_RUN : max_steps;
    }
}

void laplace_exec_set_max_derivations(laplace_exec_context_t* const ctx, const uint32_t max_derivations) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx != NULL) {
        ctx->max_derivations = (max_derivations == 0u) ? LAPLACE_EXEC_MAX_DERIVATIONS_PER_RUN : max_derivations;
    }
}

void laplace_exec_set_semi_naive(laplace_exec_context_t* const ctx, const bool enabled) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx != NULL) {
        ctx->semi_naive = enabled;
    }
}

void laplace_exec_bind_branch_system(laplace_exec_context_t* const ctx,
                                      laplace_branch_system_t* const branch_system) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx != NULL) {
        ctx->branch_system = branch_system;
    }
}

laplace_error_t laplace_exec_set_active_branch(laplace_exec_context_t* const ctx,
                                                const laplace_branch_handle_t branch) {
    if (ctx == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (branch.id != LAPLACE_BRANCH_ID_INVALID) {
        if (ctx->branch_system == NULL || !laplace_branch_is_active(ctx->branch_system, branch)) {
            return LAPLACE_ERR_INVALID_STATE;
        }
    }

    ctx->active_branch = branch;
    return LAPLACE_OK;
}

laplace_branch_handle_t laplace_exec_get_active_branch(const laplace_exec_context_t* const ctx) {
    if (ctx == NULL) {
        const laplace_branch_handle_t invalid = {
            .id = LAPLACE_BRANCH_ID_INVALID,
            .generation = LAPLACE_BRANCH_GENERATION_INVALID
        };
        return invalid;
    }

    return ctx->active_branch;
}

laplace_error_t laplace_exec_build_trigger_index(laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_exact_store_t* const store = ctx->store;
    const uint32_t rule_count = store->rule_count;

    const size_t pred_array_size = (size_t)(LAPLACE_EXACT_MAX_PREDICATES + 1u);
    memset(ctx->trigger_counts, 0, pred_array_size * sizeof(uint32_t));

    uint32_t total_entries = 0u;
    for (uint32_t rid = 1u; rid <= rule_count; ++rid) {
        const laplace_exact_rule_t* const rule = laplace_exact_get_rule(store, (laplace_rule_id_t)rid);
        if (rule == NULL || rule->status != LAPLACE_EXACT_RULE_STATUS_VALID) {
            continue;
        }

        uint32_t body_count = 0u;
        const laplace_exact_literal_t* const body = laplace_exact_rule_body_literals(store, rule, &body_count);
        for (uint32_t bi = 0u; bi < body_count; ++bi) {
            const laplace_predicate_id_t pred = body[bi].predicate;
            LAPLACE_ASSERT(pred != LAPLACE_PREDICATE_ID_INVALID);
            LAPLACE_ASSERT(pred <= LAPLACE_EXACT_MAX_PREDICATES);
            ctx->trigger_counts[pred] += 1u;
            total_entries += 1u;
        }
    }

    if (total_entries > LAPLACE_EXEC_MAX_TRIGGER_ENTRIES) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    uint32_t running = 0u;
    for (uint32_t p = 0u; p <= LAPLACE_EXACT_MAX_PREDICATES; ++p) {
        ctx->trigger_offsets[p] = running;
        running += ctx->trigger_counts[p];
    }

    uint32_t write_cursors[LAPLACE_EXACT_MAX_PREDICATES + 1u];
    memcpy(write_cursors, ctx->trigger_offsets, pred_array_size * sizeof(uint32_t));

    for (uint32_t rid = 1u; rid <= rule_count; ++rid) {
        const laplace_exact_rule_t* const rule = laplace_exact_get_rule(store, (laplace_rule_id_t)rid);
        if (rule == NULL || rule->status != LAPLACE_EXACT_RULE_STATUS_VALID) {
            continue;
        }

        uint32_t body_count = 0u;
        const laplace_exact_literal_t* const body = laplace_exact_rule_body_literals(store, rule, &body_count);
        for (uint32_t bi = 0u; bi < body_count; ++bi) {
            const laplace_predicate_id_t pred = body[bi].predicate;
            const uint32_t write_pos = write_cursors[pred];
            LAPLACE_ASSERT(write_pos < LAPLACE_EXEC_MAX_TRIGGER_ENTRIES);
            ctx->trigger_entries[write_pos].rule_id = (laplace_rule_id_t)rid;
            ctx->trigger_entries[write_pos].body_position = bi;
            write_cursors[pred] += 1u;
        }
    }

    ctx->trigger_entry_count = total_entries;
    ctx->trigger_index_built = true;
    return LAPLACE_OK;
}

laplace_error_t laplace_exec_mark_ready(laplace_exec_context_t* const ctx,
                                         const laplace_entity_id_t entity_id) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL || entity_id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace__exec_entity_is_ready_fact(ctx, entity_id)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const uint32_t slot = laplace__exec_entity_slot(entity_id);

    if (laplace_bitset_test(&ctx->ready_bitset, slot)) {
        return LAPLACE_OK;
    }

    laplace_bitset_set(&ctx->ready_bitset, slot);

    if (!laplace__exec_queue_push(ctx, entity_id)) {
        laplace_bitset_clear(&ctx->ready_bitset, slot);
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    return LAPLACE_OK;
}

uint32_t laplace_exec_mark_all_facts_ready(laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return 0u;
    }

    uint32_t marked = 0u;
    const uint32_t fact_count = ctx->store->fact_count;

    for (uint32_t fr = 1u; fr <= fact_count; ++fr) {
        const laplace_exact_fact_t* const fact = laplace_exact_get_fact(ctx->store, (laplace_exact_fact_row_t)fr);
        if (fact == NULL) {
            continue;
        }

        const laplace_entity_id_t eid = fact->entity;
        if (laplace_exec_mark_ready(ctx, eid) == LAPLACE_OK) {
            ++marked;
        }
    }

    return marked;
}

uint32_t laplace_exec_ready_count(const laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return 0u;
    }

    return laplace_bitset_popcount(&ctx->ready_bitset);
}

typedef struct laplace__exec_match_state {
    laplace_exec_context_t* ctx;
    const laplace_exact_rule_t* rule;
    const laplace_exact_literal_t* body_literals;
    uint32_t body_count;
    uint32_t anchor_body_pos;
    laplace_exact_fact_row_t anchor_fact_row;

    uint32_t cursors[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    laplace_exact_fact_row_t matched_rows[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    laplace_exact_predicate_view_t views[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    laplace_entity_id_t bindings[LAPLACE_EXACT_MAX_RULE_VARIABLES];
    bool bound[LAPLACE_EXACT_MAX_RULE_VARIABLES];

    uint32_t max_var_count;

    bool anchor_is_delta;
    uint32_t scan_starts[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
} laplace__exec_match_state_t;

static bool laplace__exec_bind_term(laplace__exec_match_state_t* const state,
                                     const laplace_exact_term_t* const term,
                                     const laplace_entity_id_t value) {
    if (term->kind == LAPLACE_EXACT_TERM_CONSTANT) {
        return term->value.constant == value;
    }

    LAPLACE_ASSERT(term->kind == LAPLACE_EXACT_TERM_VARIABLE);
    const laplace_exact_var_id_t var = term->value.variable;
    LAPLACE_ASSERT(var < LAPLACE_EXACT_MAX_RULE_VARIABLES);

    if (state->bound[var]) {
        return state->bindings[var] == value;
    }

    state->bindings[var] = value;
    state->bound[var] = true;
    return true;
}

static bool laplace__exec_match_literal(laplace__exec_match_state_t* const state,
                                         const uint32_t literal_idx,
                                         const laplace_exact_fact_row_t fact_row) {
    const laplace_exact_fact_t* const fact = laplace_exact_get_fact(state->ctx->store, fact_row);
    if (fact == NULL || !laplace__exec_fact_visible(state->ctx, fact)) {
        return false;
    }

    const laplace_exact_literal_t* const literal = &state->body_literals[literal_idx];
    LAPLACE_ASSERT(fact->predicate == literal->predicate);
    LAPLACE_ASSERT(fact->arity == literal->arity);

    for (uint32_t ti = 0u; ti < literal->arity; ++ti) {
        if (!laplace__exec_bind_term(state, &literal->terms[ti], fact->args[ti])) {
            return false;
        }
    }

    return true;
}

typedef struct laplace__exec_binding_snapshot {
    laplace_entity_id_t bindings[LAPLACE_EXACT_MAX_RULE_VARIABLES];
    bool bound[LAPLACE_EXACT_MAX_RULE_VARIABLES];
} laplace__exec_binding_snapshot_t;

static void laplace__exec_save_bindings(const laplace__exec_match_state_t* const state,
                                         laplace__exec_binding_snapshot_t* const snapshot) {
    const uint32_t n = state->max_var_count;
    memcpy(snapshot->bindings, state->bindings, (size_t)n * sizeof(laplace_entity_id_t));
    memcpy(snapshot->bound, state->bound, (size_t)n * sizeof(bool));
}

static void laplace__exec_restore_bindings(laplace__exec_match_state_t* const state,
                                            const laplace__exec_binding_snapshot_t* const snapshot) {
    const uint32_t n = state->max_var_count;
    memcpy(state->bindings, snapshot->bindings, (size_t)n * sizeof(laplace_entity_id_t));
    memcpy(state->bound, snapshot->bound, (size_t)n * sizeof(bool));
}

static bool laplace__exec_derive(laplace__exec_match_state_t* const state) {
    laplace_exec_context_t* const ctx = state->ctx;
    const laplace_exact_literal_t* const head = &state->rule->head;

    laplace_entity_id_t head_args[LAPLACE_EXACT_MAX_ARITY];
    for (uint32_t ti = 0u; ti < head->arity; ++ti) {
        const laplace_exact_term_t* const term = &head->terms[ti];
        if (term->kind == LAPLACE_EXACT_TERM_CONSTANT) {
            head_args[ti] = term->value.constant;
        } else {
            LAPLACE_ASSERT(term->kind == LAPLACE_EXACT_TERM_VARIABLE);
            LAPLACE_ASSERT(state->bound[term->value.variable]);
            head_args[ti] = state->bindings[term->value.variable];
        }
    }

    const laplace_exact_fact_row_t existing = laplace_exact_find_fact_in_branch(
        ctx->store, ctx->active_branch, head->predicate, head_args, head->arity);

    if (existing != LAPLACE_EXACT_FACT_ROW_INVALID) {
        ctx->stats.facts_deduplicated += 1u;
        return false;
    }

    laplace_entity_id_t parent_entities[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    for (uint32_t bi = 0u; bi < state->body_count; ++bi) {
        const laplace_exact_fact_row_t row = state->matched_rows[bi];

        const laplace_exact_fact_t* const body_fact = laplace_exact_get_fact(ctx->store, row);
        LAPLACE_ASSERT(body_fact != NULL);
        parent_entities[bi] = body_fact->entity;
    }

    const laplace_exact_provenance_desc_t prov_desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_DERIVED,
        .source_rule_id = state->rule->id,
        .parent_facts = parent_entities,
        .parent_count = state->body_count,
        .reserved_epoch = (ctx->branch_system != NULL) ? laplace_branch_current_epoch(ctx->branch_system) : LAPLACE_EPOCH_ID_INVALID,
        .reserved_branch = ctx->active_branch.id
    };
    laplace_provenance_id_t prov_id = LAPLACE_PROVENANCE_ID_INVALID;
    const laplace_error_t prov_err = laplace_exact_insert_provenance(ctx->store, &prov_desc, &prov_id);
    if (prov_err != LAPLACE_OK) {
        return false;
    }
    ctx->stats.provenance_created += 1u;

    laplace_entity_handle_t head_handles[LAPLACE_EXACT_MAX_ARITY];
    for (uint32_t ti = 0u; ti < head->arity; ++ti) {
        head_handles[ti] = laplace__exec_make_handle(ctx->entity_pool, head_args[ti]);
    }

    laplace_exact_fact_row_t new_row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t new_entity = {0};
    bool inserted = false;
    const laplace_error_t fact_err = laplace_exact_assert_fact_in_branch(
        ctx->store,
        ctx->active_branch,
        (ctx->branch_system != NULL) ? laplace_branch_current_epoch(ctx->branch_system) : LAPLACE_EPOCH_ID_INVALID,
        head->predicate,
        head_handles,
        head->arity,
        prov_id,
        LAPLACE_EXACT_FACT_FLAG_DERIVED,
        &new_row,
        &new_entity,
        &inserted);

    if (fact_err != LAPLACE_OK || !inserted) {
        if (!inserted) {
            ctx->stats.facts_deduplicated += 1u;
        }
        return false;
    }

    ctx->stats.facts_derived += 1u;

    if (ctx->observe != NULL) {
        laplace_observe_trace_exec_derivation(ctx->observe, new_entity.id,
            state->rule->id, new_row, prov_id,
            ctx->active_branch.id, ctx->active_branch.generation,
            ctx->store->next_tick);
    }

    (void)laplace_exec_mark_ready(ctx, new_entity.id);

    return true;
}

static uint32_t laplace__exec_rule_max_var_count(const laplace_exact_rule_t* const rule,
                                                  const laplace_exact_literal_t* const body_literals,
                                                  const uint32_t body_count) {
    uint32_t max_var = 0u;

    for (uint32_t ti = 0u; ti < rule->head.arity; ++ti) {
        if (rule->head.terms[ti].kind == LAPLACE_EXACT_TERM_VARIABLE) {
            const uint32_t v = (uint32_t)rule->head.terms[ti].value.variable + 1u;
            if (v > max_var) { max_var = v; }
        }
    }

    for (uint32_t bi = 0u; bi < body_count; ++bi) {
        for (uint32_t ti = 0u; ti < body_literals[bi].arity; ++ti) {
            if (body_literals[bi].terms[ti].kind == LAPLACE_EXACT_TERM_VARIABLE) {
                const uint32_t v = (uint32_t)body_literals[bi].terms[ti].value.variable + 1u;
                if (v > max_var) { max_var = v; }
            }
        }
    }

    return max_var;
}

static uint32_t laplace__exec_match_rule(laplace_exec_context_t* const ctx,
                                          const laplace_exact_rule_t* const rule,
                                          const uint32_t anchor_body_pos,
                                          const laplace_exact_fact_row_t anchor_fact_row) {
    laplace__exec_match_state_t state;

    state.ctx = ctx;
    state.rule = rule;
    state.anchor_body_pos = anchor_body_pos;
    state.anchor_fact_row = anchor_fact_row;

    uint32_t body_count = 0u;
    state.body_literals = laplace_exact_rule_body_literals(ctx->store, rule, &body_count);
    state.body_count = body_count;
    if (state.body_literals == NULL || body_count == 0u) {
        return 0u;
    }

    state.max_var_count = laplace__exec_rule_max_var_count(rule, state.body_literals, body_count);
    LAPLACE_ASSERT(state.max_var_count <= LAPLACE_EXACT_MAX_RULE_VARIABLES);

    memset(state.bindings, 0, (size_t)state.max_var_count * sizeof(laplace_entity_id_t));
    memset(state.bound, 0, (size_t)state.max_var_count * sizeof(bool));

    for (uint32_t bi = 0u; bi < body_count; ++bi) {
        state.views[bi] = laplace_exact_predicate_rows(ctx->store, state.body_literals[bi].predicate);
        state.cursors[bi] = 0u;
        state.scan_starts[bi] = 0u;
    }

    state.anchor_is_delta = true;  /* default: treat as delta (safe fallback) */
    if (ctx->semi_naive && body_count > 1u) {
        const laplace_predicate_id_t anchor_pred = state.body_literals[anchor_body_pos].predicate;
        const uint32_t anchor_frontier = ctx->frontier_starts[anchor_pred];
        const laplace_exact_predicate_view_t* const anchor_view = &state.views[anchor_body_pos];

        if (anchor_frontier > 0u && anchor_frontier <= anchor_view->count) {
            const laplace_exact_fact_row_t last_old_row = anchor_view->rows[anchor_frontier - 1u];
            state.anchor_is_delta = (anchor_fact_row > last_old_row);
        } else if (anchor_frontier == 0u) {
            state.anchor_is_delta = true;
        }

        if (!state.anchor_is_delta) {
            for (uint32_t bi = 0u; bi < body_count; ++bi) {
                if (bi != anchor_body_pos) {
                    const laplace_predicate_id_t lit_pred = state.body_literals[bi].predicate;
                    state.scan_starts[bi] = ctx->frontier_starts[lit_pred];
                }
            }
        }
    }

    if (!laplace__exec_match_literal(&state, anchor_body_pos, anchor_fact_row)) {
        return 0u;
    }
    state.matched_rows[anchor_body_pos] = anchor_fact_row;

    if (body_count == 1u) {
        ctx->stats.matches_found += 1u;
        return laplace__exec_derive(&state) ? 1u : 0u;
    }

    uint32_t join_order[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    uint32_t join_depth = 0u;
    for (uint32_t bi = 0u; bi < body_count; ++bi) {
        if (bi != anchor_body_pos) {
            join_order[join_depth] = bi;
            ++join_depth;
        }
    }
    const uint32_t max_join_depth = join_depth;

    laplace__exec_binding_snapshot_t snapshots[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    laplace__exec_save_bindings(&state, &snapshots[0]);

    for (uint32_t jd = 0u; jd < max_join_depth; ++jd) {
        const uint32_t li = join_order[jd];
        state.cursors[li] = state.scan_starts[li];
    }

    uint32_t derived_count = 0u;
    uint32_t current_depth = 0u;

    while (current_depth < max_join_depth) {
        const uint32_t lit_idx = join_order[current_depth];
        const laplace_exact_predicate_view_t* const view = &state.views[lit_idx];
        bool advanced = false;

        laplace__exec_restore_bindings(&state, &snapshots[current_depth]);

        while (state.cursors[lit_idx] < view->count) {
            const laplace_exact_fact_row_t candidate_row = view->rows[state.cursors[lit_idx]];

            laplace__exec_restore_bindings(&state, &snapshots[current_depth]);

            if (laplace__exec_match_literal(&state, lit_idx, candidate_row)) {
                state.matched_rows[lit_idx] = candidate_row;
                state.cursors[lit_idx] += 1u;
                advanced = true;

                if (current_depth + 1u == max_join_depth) {
                    ctx->stats.matches_found += 1u;
                    if (laplace__exec_derive(&state)) {
                        derived_count += 1u;
                    }
                    advanced = false;
                    continue;
                }

                laplace__exec_save_bindings(&state, &snapshots[current_depth + 1u]);
                state.cursors[join_order[current_depth + 1u]] = state.scan_starts[join_order[current_depth + 1u]];
                current_depth += 1u;
                break;
            }

            state.cursors[lit_idx] += 1u;
        }

        if (!advanced) {
            if (current_depth == 0u) {
                break;
            }
            current_depth -= 1u;
        }
    }

    return derived_count;
}

static laplace_entity_id_t laplace__exec_pick_next(laplace_exec_context_t* const ctx) {
    if (ctx->mode == LAPLACE_EXEC_MODE_DENSE) {
        const uint32_t slot = laplace_bitset_find_first_set(&ctx->ready_bitset);
        if (slot == UINT32_MAX) {
            return LAPLACE_ENTITY_ID_INVALID;
        }
        return slot + 1u; /* slot is 0-based, entity ID is 1-based */
    }

    while (ctx->ready_queue_count > 0u) {
        const laplace_entity_id_t eid = laplace__exec_queue_pop(ctx);
        if (eid == LAPLACE_ENTITY_ID_INVALID) {
            continue;
        }
        const uint32_t slot = laplace__exec_entity_slot(eid);
        if (slot < ctx->entity_capacity && laplace_bitset_test(&ctx->ready_bitset, slot)) {
            return eid;
        }
    }

    return LAPLACE_ENTITY_ID_INVALID;
}

static laplace_error_t laplace__exec_process_entity(laplace_exec_context_t* const ctx,
                                                      const laplace_entity_id_t entity_id) {
    LAPLACE_ASSERT(ctx->trigger_index_built);

    const laplace_exact_fact_row_t fact_row = laplace__exec_entity_fact_row(ctx, entity_id);
    if (fact_row == LAPLACE_EXACT_FACT_ROW_INVALID) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const laplace_exact_fact_t* const fact = laplace_exact_get_fact(ctx->store, fact_row);
    if (fact == NULL || !laplace__exec_fact_visible(ctx, fact)) {
        return LAPLACE_ERR_INTERNAL;
    }

    const laplace_predicate_id_t pred = fact->predicate;
    if (pred == LAPLACE_PREDICATE_ID_INVALID || pred > LAPLACE_EXACT_MAX_PREDICATES) {
        return LAPLACE_ERR_INTERNAL;
    }

    const uint32_t trig_offset = ctx->trigger_offsets[pred];
    const uint32_t trig_count = ctx->trigger_counts[pred];

    for (uint32_t ti = 0u; ti < trig_count; ++ti) {
        const laplace_exec_trigger_entry_t* const entry = &ctx->trigger_entries[trig_offset + ti];
        const laplace_exact_rule_t* const rule = laplace_exact_get_rule(ctx->store, entry->rule_id);
        if (rule == NULL || rule->status != LAPLACE_EXACT_RULE_STATUS_VALID) {
            continue;
        }

        ctx->stats.rules_fired += 1u;
        (void)laplace__exec_match_rule(ctx, rule, entry->body_position, fact_row);
    }

    const uint32_t slot = laplace__exec_entity_slot(entity_id);
    const laplace_entity_handle_t handle = laplace__exec_make_handle(ctx->entity_pool, entity_id);
    const laplace_error_t state_err = laplace_entity_pool_set_state(ctx->entity_pool, handle, LAPLACE_STATE_ACTIVE);
    LAPLACE_ASSERT(state_err == LAPLACE_OK);

    laplace_bitset_clear(&ctx->ready_bitset, slot);

    ctx->stats.steps_executed += 1u;

    if (ctx->observe != NULL) {
        laplace_observe_trace_exec_step(ctx->observe, entity_id,
            (uint32_t)trig_count,
            ctx->active_branch.id, ctx->active_branch.generation,
            (ctx->branch_system != NULL) ? laplace_branch_current_epoch(ctx->branch_system) : LAPLACE_EPOCH_ID_INVALID,
            ctx->store->next_tick);
    }

    (void)state_err;
    return LAPLACE_OK;
}

laplace_error_t laplace_exec_step(laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!ctx->trigger_index_built) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const laplace_entity_id_t entity_id = laplace__exec_pick_next(ctx);
    if (entity_id == LAPLACE_ENTITY_ID_INVALID) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    return laplace__exec_process_entity(ctx, entity_id);
}

static void laplace__exec_snapshot_frontiers(laplace_exec_context_t* const ctx) {
    for (uint32_t p = 0u; p <= LAPLACE_EXACT_MAX_PREDICATES; ++p) {
        ctx->frontier_starts[p] = ctx->store->predicate_row_counts[p];
    }
    ctx->frontier_fact_count = ctx->store->fact_count;
}

static bool laplace__exec_has_new_facts(const laplace_exec_context_t* const ctx) {
    return ctx->store->fact_count > ctx->frontier_fact_count;
}

laplace_exec_run_status_t laplace_exec_run(laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return LAPLACE_EXEC_RUN_ERROR;
    }

    if (!ctx->trigger_index_built) {
        return LAPLACE_EXEC_RUN_ERROR;
    }

    const uint64_t initial_steps = ctx->stats.steps_executed;
    const uint64_t initial_derived = ctx->stats.facts_derived;

    if (ctx->semi_naive) {
        laplace__exec_snapshot_frontiers(ctx);
        ctx->stats.fixpoint_rounds = 1u;
    }

    while (true) {
        if ((ctx->stats.steps_executed - initial_steps) >= (uint64_t)ctx->max_steps) {
            return LAPLACE_EXEC_RUN_BUDGET;
        }

        if ((ctx->stats.facts_derived - initial_derived) >= (uint64_t)ctx->max_derivations) {
            return LAPLACE_EXEC_RUN_BUDGET;
        }

        const laplace_entity_id_t entity_id = laplace__exec_pick_next(ctx);
        if (entity_id == LAPLACE_ENTITY_ID_INVALID) {
            if (ctx->semi_naive && laplace__exec_has_new_facts(ctx)) {
                laplace__exec_snapshot_frontiers(ctx);
                ctx->stats.fixpoint_rounds += 1u;
                continue;  /* re-enter loop to pick newly READY facts */
            }

            if (ctx->observe != NULL) {
                laplace_observe_trace_exec_fixpoint(ctx->observe,
                    ctx->stats.fixpoint_rounds, ctx->store->next_tick);
            }

            return LAPLACE_EXEC_RUN_FIXPOINT;
        }

        const laplace_error_t err = laplace__exec_process_entity(ctx, entity_id);
        if (err != LAPLACE_OK) {
            return LAPLACE_EXEC_RUN_ERROR;
        }
    }
}

const laplace_exec_stats_t* laplace_exec_get_stats(const laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return NULL;
    }
    return &ctx->stats;
}

laplace_exec_mode_t laplace_exec_get_mode(const laplace_exec_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    if (ctx == NULL) {
        return LAPLACE_EXEC_MODE_DENSE;
    }
    return ctx->mode;
}

void laplace_exec_bind_observe(laplace_exec_context_t* const ctx,
                               laplace_observe_context_t* const observe) {
    if (ctx != NULL) {
        ctx->observe = observe;
    }
}
