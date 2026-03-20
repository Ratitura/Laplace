#include "laplace/proof_search.h"

#include <string.h>

static laplace_proof_search_index_key_t search_make_key(
    const laplace_proof_store_t* store,
    laplace_proof_assertion_id_t assertion_id) {

    laplace_proof_search_index_key_t key;
    memset(&key, 0, sizeof(key));

    const laplace_proof_assertion_t* assn =
        laplace_proof_get_assertion(store, assertion_id);
    if (!assn) return key;

    const laplace_proof_expr_t* concl =
        laplace_proof_get_expr(store, assn->conclusion_id);
    if (!concl) return key;

    key.typecode         = concl->typecode;
    key.body_token_count = concl->token_count;
    key.mand_var_count   = assn->mand_float_count;

    if (concl->token_count > 0u) {
        key.first_body_token = store->token_pool[concl->token_offset];
    }

    return key;
}

laplace_error_t laplace_proof_search_init(
    laplace_proof_search_context_t* ctx,
    const laplace_proof_store_t* store,
    laplace_proof_search_index_t* index,
    laplace_proof_search_scratch_t* scratch) {

    if (!ctx || !store || !index || !scratch) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }
    if (!store->initialized) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->store       = store;
    ctx->index       = index;
    ctx->scratch     = scratch;
    ctx->initialized = true;

    index->count = 0u;
    index->built = false;

    scratch->subst_count = 0u;
    scratch->token_used  = 0u;

    return LAPLACE_OK;
}

void laplace_proof_search_reset(laplace_proof_search_context_t* ctx) {
    if (!ctx) return;
    if (ctx->index) {
        ctx->index->count = 0u;
        ctx->index->built = false;
    }
    if (ctx->scratch) {
        ctx->scratch->subst_count = 0u;
        ctx->scratch->token_used  = 0u;
    }
}

laplace_error_t laplace_proof_search_build_index(
    laplace_proof_search_context_t* ctx) {

    if (!ctx || !ctx->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;

    laplace_proof_search_index_t* idx = ctx->index;
    idx->count = 0u;
    idx->built = false;

    const laplace_proof_store_t* store = ctx->store;
    const uint32_t assn_count = store->assertion_count;

    for (uint32_t i = 1u; i <= assn_count; ++i) {
        if (idx->count >= LAPLACE_PROOF_SEARCH_MAX_INDEX_ENTRIES) {
            break;
        }

        laplace_proof_search_index_entry_t* entry = &idx->entries[idx->count];
        entry->key          = search_make_key(store, i);
        entry->assertion_id = i;
        idx->count++;
    }

    idx->built = true;
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_search_query_candidates(
    laplace_proof_search_context_t* ctx,
    laplace_proof_expr_id_t goal_expr_id,
    laplace_proof_search_candidate_buf_t* buf) {

    if (!ctx || !ctx->initialized || !buf) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    buf->count         = 0u;
    buf->total_matches = 0u;
    buf->truncated     = false;

    const laplace_proof_expr_t* goal_expr =
        laplace_proof_get_expr(ctx->store, goal_expr_id);
    if (!goal_expr) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_symbol_id_t goal_typecode = goal_expr->typecode;
    const laplace_proof_search_index_t* idx = ctx->index;

    if (!idx->built) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    for (uint32_t i = 0u; i < idx->count; ++i) {
        if (idx->entries[i].key.typecode != goal_typecode) {
            continue;
        }

        buf->total_matches++;

        if (buf->count < LAPLACE_PROOF_SEARCH_MAX_CANDIDATES) {
            buf->ids[buf->count++] = idx->entries[i].assertion_id;
        } else {
            buf->truncated = true;
        }
    }

    return LAPLACE_OK;
}

laplace_error_t laplace_proof_search_query_candidates_typecode(
    laplace_proof_search_context_t* ctx,
    laplace_proof_symbol_id_t typecode,
    laplace_proof_search_candidate_buf_t* buf) {

    if (!ctx || !ctx->initialized || !buf) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    buf->count         = 0u;
    buf->total_matches = 0u;
    buf->truncated     = false;

    const laplace_proof_search_index_t* idx = ctx->index;

    if (!idx->built) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    for (uint32_t i = 0u; i < idx->count; ++i) {
        if (idx->entries[i].key.typecode != typecode) {
            continue;
        }

        buf->total_matches++;

        if (buf->count < LAPLACE_PROOF_SEARCH_MAX_CANDIDATES) {
            buf->ids[buf->count++] = idx->entries[i].assertion_id;
        } else {
            buf->truncated = true;
        }
    }

    return LAPLACE_OK;
}

static void search_scratch_reset(laplace_proof_search_scratch_t* scratch) {
    scratch->subst_count = 0u;
    scratch->token_used  = 0u;
}

static uint32_t search_find_subst(
    const laplace_proof_search_scratch_t* scratch,
    laplace_proof_symbol_id_t variable) {

    for (uint32_t i = 0u; i < scratch->subst_count; ++i) {
        if (scratch->subst[i].variable == variable) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool search_tokens_equal(
    const laplace_proof_symbol_id_t* a, uint32_t a_count,
    const laplace_proof_symbol_id_t* b, uint32_t b_count) {

    if (a_count != b_count) return false;
    return memcmp(a, b, a_count * sizeof(laplace_proof_symbol_id_t)) == 0;
}

static laplace_proof_search_status_t search_instantiate_expr(
    const laplace_proof_store_t* store,
    laplace_proof_search_scratch_t* scratch,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_symbol_id_t* out_typecode,
    uint32_t* out_offset,
    uint32_t* out_count) {

    const laplace_proof_expr_t* expr = laplace_proof_get_expr(store, expr_id);
    if (!expr) return LAPLACE_PROOF_SEARCH_INVALID_GOAL;

    *out_typecode = expr->typecode;
    const uint32_t start = scratch->token_used;
    *out_offset = start;

    const laplace_proof_symbol_id_t* tokens =
        &store->token_pool[expr->token_offset];
    uint32_t written = 0u;

    for (uint32_t i = 0u; i < expr->token_count; ++i) {
        const laplace_proof_symbol_id_t tok = tokens[i];

        if (laplace_proof_symbol_is_variable(store, tok)) {
            const uint32_t si = search_find_subst(scratch, tok);
            if (si != UINT32_MAX) {
                const laplace_proof_search_subst_entry_t* s = &scratch->subst[si];
                if (start + written + s->token_count >
                    LAPLACE_PROOF_SEARCH_MAX_INST_TOKENS) {
                    return LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT;
                }
                memcpy(&scratch->tokens[start + written],
                       &scratch->tokens[s->token_offset],
                       s->token_count * sizeof(laplace_proof_symbol_id_t));
                written += s->token_count;
                continue;
            }
        }

        if (start + written >= LAPLACE_PROOF_SEARCH_MAX_INST_TOKENS) {
            return LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT;
        }
        scratch->tokens[start + written] = tok;
        written++;
    }

    *out_count = written;
    scratch->token_used = start + written;
    return LAPLACE_PROOF_SEARCH_SUCCESS;
}

static void search_collect_vars(
    const laplace_proof_store_t* store,
    const laplace_proof_symbol_id_t* tokens,
    uint32_t count,
    laplace_proof_symbol_id_t* vars,
    uint32_t max_vars,
    uint32_t* out_count) {

    *out_count = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        if (!laplace_proof_symbol_is_variable(store, tokens[i])) continue;

        bool found = false;
        for (uint32_t j = 0u; j < *out_count; ++j) {
            if (vars[j] == tokens[i]) { found = true; break; }
        }
        if (!found && *out_count < max_vars) {
            vars[(*out_count)++] = tokens[i];
        }
    }
}

static laplace_proof_search_status_t search_check_dv(
    const laplace_proof_store_t* store,
    laplace_proof_search_scratch_t* scratch,
    const laplace_proof_assertion_t* assn,
    laplace_proof_search_try_result_t* result) {

    for (uint32_t d = 0u; d < assn->dv_count; ++d) {
        const laplace_proof_dv_pair_t* dv =
            &store->dv_pairs[assn->dv_offset + d];

        const uint32_t si_a = search_find_subst(scratch, dv->var_a);
        const uint32_t si_b = search_find_subst(scratch, dv->var_b);
        if (si_a == UINT32_MAX || si_b == UINT32_MAX) continue;

        const laplace_proof_search_subst_entry_t* sa = &scratch->subst[si_a];
        const laplace_proof_search_subst_entry_t* sb = &scratch->subst[si_b];

        laplace_proof_symbol_id_t vars_a[64];
        laplace_proof_symbol_id_t vars_b[64];
        uint32_t count_a = 0u, count_b = 0u;

        search_collect_vars(store,
            &scratch->tokens[sa->token_offset], sa->token_count,
            vars_a, 64u, &count_a);
        search_collect_vars(store,
            &scratch->tokens[sb->token_offset], sb->token_count,
            vars_b, 64u, &count_b);

        for (uint32_t i = 0u; i < count_a; ++i) {
            for (uint32_t j = 0u; j < count_b; ++j) {
                if (vars_a[i] == vars_b[j]) {
                    result->status   = LAPLACE_PROOF_SEARCH_DV_VIOLATION;
                    result->detail_a = dv->var_a;
                    result->detail_b = dv->var_b;
                    return LAPLACE_PROOF_SEARCH_DV_VIOLATION;
                }
            }
        }
    }

    return LAPLACE_PROOF_SEARCH_SUCCESS;
}

static bool search_unify_conclusion(
    const laplace_proof_store_t* store,
    laplace_proof_search_scratch_t* scratch,
    const laplace_proof_expr_t* goal_expr,
    const laplace_proof_expr_t* concl_expr,
    const laplace_proof_symbol_id_t* goal_tokens,
    const laplace_proof_symbol_id_t* concl_tokens) {

    if (goal_expr->typecode != concl_expr->typecode) return false;

    uint32_t gi = 0u;
    uint32_t ci = 0u;

    while (ci < concl_expr->token_count) {
        const laplace_proof_symbol_id_t ct = concl_tokens[ci];

        if (laplace_proof_symbol_is_variable(store, ct)) {
            const uint32_t existing = search_find_subst(scratch, ct);

            if (existing != UINT32_MAX) {
                const laplace_proof_search_subst_entry_t* s = &scratch->subst[existing];
                if (gi + s->token_count > goal_expr->token_count) return false;
                if (!search_tokens_equal(
                        &goal_tokens[gi], s->token_count,
                        &scratch->tokens[s->token_offset], s->token_count)) {
                    return false;
                }
                gi += s->token_count;
                ci++;
                continue;
            }

            uint32_t match_end = gi;

            if (ci + 1u < concl_expr->token_count &&
                !laplace_proof_symbol_is_variable(store, concl_tokens[ci + 1u])) {
                const laplace_proof_symbol_id_t next_const = concl_tokens[ci + 1u];
                match_end = gi;
                while (match_end < goal_expr->token_count) {
                    if (goal_tokens[match_end] == next_const) break;
                    match_end++;
                }
                if (match_end > goal_expr->token_count) return false;
            } else if (ci + 1u >= concl_expr->token_count) {
                match_end = goal_expr->token_count;
            } else {
                if (gi >= goal_expr->token_count) return false;
                match_end = gi + 1u;
            }

            const uint32_t var_token_count = match_end - gi;

            if (scratch->subst_count >= LAPLACE_PROOF_SEARCH_MAX_SUBST_VARS) return false;
            if (scratch->token_used + var_token_count > LAPLACE_PROOF_SEARCH_MAX_INST_TOKENS) return false;

            laplace_proof_search_subst_entry_t* ns =
                &scratch->subst[scratch->subst_count++];
            ns->variable     = ct;
            ns->typecode     = 0u;
            ns->token_offset = scratch->token_used;
            ns->token_count  = var_token_count;

            if (var_token_count > 0u) {
                memcpy(&scratch->tokens[scratch->token_used],
                       &goal_tokens[gi],
                       var_token_count * sizeof(laplace_proof_symbol_id_t));
            }
            scratch->token_used += var_token_count;
            gi += var_token_count;
            ci++;
        } else {
            if (gi >= goal_expr->token_count) return false;
            if (goal_tokens[gi] != ct) return false;
            gi++;
            ci++;
        }
    }

    return gi == goal_expr->token_count;
}

laplace_error_t laplace_proof_search_try_assertion(
    laplace_proof_search_context_t* ctx,
    laplace_proof_expr_id_t goal_expr_id,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_search_try_result_t* result) {

    if (!ctx || !ctx->initialized || !result) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));
    result->assertion_id = assertion_id;

    const laplace_proof_store_t* store = ctx->store;
    laplace_proof_search_scratch_t* scratch = ctx->scratch;

    const laplace_proof_expr_t* goal_expr =
        laplace_proof_get_expr(store, goal_expr_id);
    if (!goal_expr) {
        result->status = LAPLACE_PROOF_SEARCH_INVALID_GOAL;
        return LAPLACE_OK;
    }

    const laplace_proof_assertion_t* assn =
        laplace_proof_get_assertion(store, assertion_id);
    if (!assn) {
        result->status = LAPLACE_PROOF_SEARCH_INVALID_ASSERTION;
        return LAPLACE_OK;
    }

    const laplace_proof_expr_t* concl =
        laplace_proof_get_expr(store, assn->conclusion_id);
    if (!concl) {
        result->status = LAPLACE_PROOF_SEARCH_INTERNAL_INVARIANT;
        return LAPLACE_OK;
    }

    search_scratch_reset(scratch);

    if (goal_expr->typecode != concl->typecode) {
        result->status = LAPLACE_PROOF_SEARCH_TYPECODE_MISMATCH;
        return LAPLACE_OK;
    }

    const laplace_proof_symbol_id_t* goal_tokens =
        &store->token_pool[goal_expr->token_offset];
    const laplace_proof_symbol_id_t* concl_tokens =
        &store->token_pool[concl->token_offset];

    if (!search_unify_conclusion(store, scratch, goal_expr, concl,
                                  goal_tokens, concl_tokens)) {
        result->status = LAPLACE_PROOF_SEARCH_UNIFICATION_CONFLICT;
        return LAPLACE_OK;
    }

    for (uint32_t fi = 0u; fi < assn->mand_float_count; ++fi) {
        const laplace_proof_hyp_id_t hyp_id = assn->mand_float_offset + fi;
        const laplace_proof_hyp_t* hyp = laplace_proof_get_hyp(store, hyp_id);
        if (!hyp) {
            result->status = LAPLACE_PROOF_SEARCH_INTERNAL_INVARIANT;
            return LAPLACE_OK;
        }

        const laplace_proof_expr_t* fexpr =
            laplace_proof_get_expr(store, hyp->expr_id);
        if (!fexpr || fexpr->token_count != 1u) {
            result->status = LAPLACE_PROOF_SEARCH_INTERNAL_INVARIANT;
            return LAPLACE_OK;
        }

        const laplace_proof_symbol_id_t variable =
            store->token_pool[fexpr->token_offset];
        const laplace_proof_symbol_id_t required_typecode = fexpr->typecode;

        const uint32_t si = search_find_subst(scratch, variable);
        if (si != UINT32_MAX) {
            scratch->subst[si].typecode = required_typecode;
        }
    }

    if (assn->dv_count > 0u) {
        const laplace_proof_search_status_t dv_status =
            search_check_dv(store, scratch, assn, result);
        if (dv_status != LAPLACE_PROOF_SEARCH_SUCCESS) {
            return LAPLACE_OK;
        }
    }

    result->subgoal_count = 0u;

    for (uint32_t ei = 0u; ei < assn->mand_ess_count; ++ei) {
        const laplace_proof_hyp_id_t hyp_id = assn->mand_ess_offset + ei;
        const laplace_proof_hyp_t* hyp = laplace_proof_get_hyp(store, hyp_id);
        if (!hyp || hyp->kind != LAPLACE_PROOF_HYP_ESSENTIAL) {
            result->status = LAPLACE_PROOF_SEARCH_INTERNAL_INVARIANT;
            return LAPLACE_OK;
        }

        laplace_proof_symbol_id_t inst_typecode;
        uint32_t inst_offset, inst_count;
        const laplace_proof_search_status_t inst_status =
            search_instantiate_expr(store, scratch, hyp->expr_id,
                                     &inst_typecode, &inst_offset, &inst_count);
        if (inst_status != LAPLACE_PROOF_SEARCH_SUCCESS) {
            result->status = inst_status;
            return LAPLACE_OK;
        }

        laplace_proof_expr_id_t subgoal_expr = LAPLACE_PROOF_EXPR_ID_INVALID;

        for (uint32_t eid = 1u; eid <= store->expr_count; ++eid) {
            const laplace_proof_expr_t* se = laplace_proof_get_expr(store, eid);
            if (!se) continue;
            if (se->typecode != inst_typecode) continue;
            if (se->token_count != inst_count) continue;
            if (search_tokens_equal(
                    &store->token_pool[se->token_offset], se->token_count,
                    &scratch->tokens[inst_offset], inst_count)) {
                subgoal_expr = eid;
                break;
            }
        }

        if (subgoal_expr == LAPLACE_PROOF_EXPR_ID_INVALID) {
            result->status   = LAPLACE_PROOF_SEARCH_ESSENTIAL_HYP_MISMATCH;
            result->detail_a = hyp_id;
            return LAPLACE_OK;
        }

        if (result->subgoal_count < LAPLACE_PROOF_SEARCH_MAX_SUBGOALS) {
            result->subgoals[result->subgoal_count].expr_id = subgoal_expr;
            result->subgoal_count++;
        } else {
            result->status = LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT;
            return LAPLACE_OK;
        }
    }

    result->status = LAPLACE_PROOF_SEARCH_SUCCESS;
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_search_state_init(
    laplace_proof_search_state_t* state,
    laplace_proof_expr_id_t root_goal_expr_id) {

    if (!state) return LAPLACE_ERR_INVALID_ARGUMENT;

    memset(state, 0, sizeof(*state));
    state->root_goal.expr_id = root_goal_expr_id;
    state->status            = LAPLACE_PROOF_SEARCH_STATE_OPEN;
    state->parent_state_id   = LAPLACE_PROOF_SEARCH_STATE_ID_INVALID;

    state->obligations[0].goal.expr_id    = root_goal_expr_id;
    state->obligations[0].depth           = 0u;
    state->obligations[0].source_assertion = LAPLACE_PROOF_ASSERTION_ID_INVALID;
    state->obligations[0].source_hyp_index = 0u;
    state->obligation_count = 1u;

    return LAPLACE_OK;
}

laplace_error_t laplace_proof_search_state_expand(
    laplace_proof_search_context_t* ctx,
    laplace_proof_search_state_t* state,
    uint32_t obligation_index,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_search_try_result_t* result) {

    if (!ctx || !ctx->initialized || !state || !result) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (state->status != LAPLACE_PROOF_SEARCH_STATE_OPEN) {
        result->status = LAPLACE_PROOF_SEARCH_INVALID_GOAL;
        return LAPLACE_OK;
    }

    if (obligation_index >= state->obligation_count) {
        result->status = LAPLACE_PROOF_SEARCH_INVALID_GOAL;
        return LAPLACE_OK;
    }

    const laplace_proof_search_obligation_t* obl =
        &state->obligations[obligation_index];

    laplace_error_t err = laplace_proof_search_try_assertion(
        ctx, obl->goal.expr_id, assertion_id, result);
    if (err != LAPLACE_OK) return err;

    if (result->status != LAPLACE_PROOF_SEARCH_SUCCESS) {
        return LAPLACE_OK;
    }

    const uint32_t new_depth = obl->depth + 1u;

    if (new_depth > LAPLACE_PROOF_SEARCH_MAX_DEPTH) {
        result->status = LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT;
        return LAPLACE_OK;
    }

    const uint32_t old_count = state->obligation_count;
    const uint32_t new_obligations = result->subgoal_count;
    const int32_t delta = (int32_t)new_obligations - 1;
    const uint32_t new_count = (uint32_t)((int32_t)old_count + delta);

    if (new_count > LAPLACE_PROOF_SEARCH_MAX_OBLIGATIONS) {
        result->status = LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT;
        return LAPLACE_OK;
    }

    if (delta > 0 && obligation_index + 1u < old_count) {
        memmove(&state->obligations[obligation_index + new_obligations],
                &state->obligations[obligation_index + 1u],
                (old_count - obligation_index - 1u) *
                sizeof(laplace_proof_search_obligation_t));
    } else if (delta < 0 && obligation_index + 1u < old_count) {
        memmove(&state->obligations[obligation_index + new_obligations],
                &state->obligations[obligation_index + 1u],
                (old_count - obligation_index - 1u) *
                sizeof(laplace_proof_search_obligation_t));
    }

    for (uint32_t i = 0u; i < new_obligations; ++i) {
        laplace_proof_search_obligation_t* new_obl =
            &state->obligations[obligation_index + i];
        new_obl->goal             = result->subgoals[i];
        new_obl->depth            = new_depth;
        new_obl->source_assertion = assertion_id;
        new_obl->source_hyp_index = i;
    }

    state->obligation_count = new_count;
    state->step_count++;
    state->last_assertion = assertion_id;
    state->depth = new_depth > state->depth ? new_depth : state->depth;

    if (state->obligation_count == 0u) {
        state->status = LAPLACE_PROOF_SEARCH_STATE_PROVED;
    }

    return LAPLACE_OK;
}

uint32_t laplace_proof_search_index_count(
    const laplace_proof_search_context_t* ctx) {
    if (!ctx || !ctx->index) return 0u;
    return ctx->index->count;
}

static const char* const search_status_names[] = {
    "success",
    "no_candidate_match",
    "unification_conflict",
    "typecode_mismatch",
    "essential_hyp_mismatch",
    "dv_violation",
    "invalid_goal",
    "invalid_assertion",
    "resource_limit",
    "unsupported_action",
    "internal_invariant"
};

const char* laplace_proof_search_status_string(
    laplace_proof_search_status_t status) {
    if (status >= LAPLACE_PROOF_SEARCH_STATUS_COUNT_) {
        return "unknown";
    }
    return search_status_names[status];
}
