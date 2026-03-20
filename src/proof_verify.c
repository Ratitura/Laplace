#include "laplace/proof_verify.h"

#include <string.h>

laplace_error_t laplace_proof_verify_init(
    laplace_proof_verify_context_t* ctx,
    const laplace_proof_store_t* store) {

    if (!ctx || !store) return LAPLACE_ERR_INVALID_ARGUMENT;
    if (!store->initialized) return LAPLACE_ERR_INVALID_STATE;

    memset(ctx, 0, sizeof(*ctx));
    ctx->store       = store;
    ctx->initialized = true;
    return LAPLACE_OK;
}

void laplace_proof_verify_reset(laplace_proof_verify_context_t* ctx) {
    if (!ctx) return;
    const laplace_proof_store_t* store = ctx->store;

    ctx->stack_depth       = 0u;
    ctx->subst_count       = 0u;
    ctx->scratch_token_used = 0u;

    ctx->store = store;
}

static laplace_proof_verify_status_t verify_stack_push(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_expr_id_t expr_id) {

    if (ctx->stack_depth >= LAPLACE_PROOF_MAX_STACK_DEPTH) {
        return LAPLACE_PROOF_VERIFY_STACK_OVERFLOW;
    }
    ctx->stack[ctx->stack_depth++] = expr_id;
    return LAPLACE_PROOF_VERIFY_OK;
}

static uint32_t verify_find_subst(
    const laplace_proof_verify_context_t* ctx,
    laplace_proof_symbol_id_t variable) {

    for (uint32_t i = 0u; i < ctx->subst_count; ++i) {
        if (ctx->subst[i].variable == variable) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool verify_expr_equal(
    const laplace_proof_store_t* store,
    laplace_proof_expr_id_t a,
    laplace_proof_expr_id_t b) {

    if (a == b) return true;

    const laplace_proof_expr_t* ea = laplace_proof_get_expr(store, a);
    const laplace_proof_expr_t* eb = laplace_proof_get_expr(store, b);
    if (!ea || !eb) return false;

    if (ea->typecode != eb->typecode) return false;
    if (ea->token_count != eb->token_count) return false;

    const laplace_proof_symbol_id_t* ta = &store->token_pool[ea->token_offset];
    const laplace_proof_symbol_id_t* tb = &store->token_pool[eb->token_offset];
    return memcmp(ta, tb, ea->token_count * sizeof(laplace_proof_symbol_id_t)) == 0;
}

static bool verify_scratch_expr_equal_store(
    const laplace_proof_verify_context_t* ctx,
    laplace_proof_symbol_id_t scratch_typecode,
    uint32_t scratch_offset,
    uint32_t scratch_count,
    laplace_proof_expr_id_t store_expr_id) {

    const laplace_proof_expr_t* se =
        laplace_proof_get_expr(ctx->store, store_expr_id);
    if (!se) return false;

    if (scratch_typecode != se->typecode) return false;
    if (scratch_count != se->token_count) return false;

    const laplace_proof_symbol_id_t* st = &ctx->scratch_tokens[scratch_offset];
    const laplace_proof_symbol_id_t* et =
        &ctx->store->token_pool[se->token_offset];
    return memcmp(st, et, scratch_count * sizeof(laplace_proof_symbol_id_t)) == 0;
}

static laplace_proof_verify_status_t verify_instantiate_expr(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_symbol_id_t* out_typecode,
    uint32_t* out_offset,
    uint32_t* out_count) {

    const laplace_proof_store_t* store = ctx->store;
    const laplace_proof_expr_t* expr = laplace_proof_get_expr(store, expr_id);
    if (!expr) return LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;

    *out_typecode = expr->typecode;
    const uint32_t start = ctx->scratch_token_used;
    *out_offset = start;

    const laplace_proof_symbol_id_t* tokens =
        &store->token_pool[expr->token_offset];
    uint32_t written = 0u;

    for (uint32_t i = 0u; i < expr->token_count; ++i) {
        const laplace_proof_symbol_id_t tok = tokens[i];

        if (laplace_proof_symbol_is_variable(store, tok)) {
            const uint32_t si = verify_find_subst(ctx, tok);
            if (si != UINT32_MAX) {
                const laplace_proof_verify_subst_t* s = &ctx->subst[si];
                if (start + written + s->token_count >
                    LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS) {
                    return LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT;
                }
                memcpy(&ctx->scratch_tokens[start + written],
                       &ctx->scratch_tokens[s->token_offset],
                       s->token_count * sizeof(laplace_proof_symbol_id_t));
                written += s->token_count;
                continue;
            }
        }

        if (start + written >= LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS) {
            return LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT;
        }
        ctx->scratch_tokens[start + written] = tok;
        written++;
    }

    *out_count = written;
    ctx->scratch_token_used = start + written;
    return LAPLACE_PROOF_VERIFY_OK;
}

static void verify_collect_scratch_vars(
    const laplace_proof_verify_context_t* ctx,
    const laplace_proof_store_t* store,
    uint32_t offset,
    uint32_t count,
    laplace_proof_symbol_id_t* vars,
    uint32_t max_vars,
    uint32_t* out_count) {

    *out_count = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        const laplace_proof_symbol_id_t tok = ctx->scratch_tokens[offset + i];
        if (!laplace_proof_symbol_is_variable(store, tok)) continue;

        bool found = false;
        for (uint32_t j = 0u; j < *out_count; ++j) {
            if (vars[j] == tok) { found = true; break; }
        }
        if (!found && *out_count < max_vars) {
            vars[(*out_count)++] = tok;
        }
    }
}

static laplace_proof_verify_status_t verify_check_dv(
    laplace_proof_verify_context_t* ctx,
    const laplace_proof_assertion_t* assn,
    laplace_proof_verify_result_t* result,
    uint32_t step_index) {

    const laplace_proof_store_t* store = ctx->store;

    for (uint32_t d = 0u; d < assn->dv_count; ++d) {
        const laplace_proof_dv_pair_t* dv =
            &store->dv_pairs[assn->dv_offset + d];

        const uint32_t si_a = verify_find_subst(ctx, dv->var_a);
        const uint32_t si_b = verify_find_subst(ctx, dv->var_b);
        if (si_a == UINT32_MAX || si_b == UINT32_MAX) {
            continue;
        }

        const laplace_proof_verify_subst_t* sa = &ctx->subst[si_a];
        const laplace_proof_verify_subst_t* sb = &ctx->subst[si_b];

        laplace_proof_symbol_id_t vars_a[64];
        laplace_proof_symbol_id_t vars_b[64];
        uint32_t count_a = 0u, count_b = 0u;

        verify_collect_scratch_vars(ctx, store,
            sa->token_offset, sa->token_count, vars_a, 64u, &count_a);
        verify_collect_scratch_vars(ctx, store,
            sb->token_offset, sb->token_count, vars_b, 64u, &count_b);

        for (uint32_t i = 0u; i < count_a; ++i) {
            for (uint32_t j = 0u; j < count_b; ++j) {
                if (vars_a[i] == vars_b[j]) {
                    result->status           = LAPLACE_PROOF_VERIFY_DV_VIOLATION;
                    result->failure_step     = step_index;
                    result->failure_assertion = 0u;
                    result->detail_a         = dv->var_a;
                    result->detail_b         = dv->var_b;
                    return LAPLACE_PROOF_VERIFY_DV_VIOLATION;
                }
            }
        }
    }

    return LAPLACE_PROOF_VERIFY_OK;
}

laplace_error_t laplace_proof_verify_apply_assertion(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_verify_result_t* result,
    uint32_t step_index) {

    if (!ctx || !ctx->initialized || !result) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_store_t* store = ctx->store;
    const laplace_proof_assertion_t* assn =
        laplace_proof_get_assertion(store, assertion_id);
    if (!assn) {
        result->status       = LAPLACE_PROOF_VERIFY_INVALID_STEP_REF;
        result->failure_step = step_index;
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t total_hyps =
        assn->mand_float_count + assn->mand_ess_count;

    if (ctx->stack_depth < total_hyps) {
        result->status       = LAPLACE_PROOF_VERIFY_STACK_UNDERFLOW;
        result->failure_step = step_index;
        result->failure_assertion = assertion_id;
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const uint32_t base = ctx->stack_depth - total_hyps;

    ctx->subst_count = 0u;

    const uint32_t saved_scratch = ctx->scratch_token_used;

    for (uint32_t fi = 0u; fi < assn->mand_float_count; ++fi) {
        const laplace_proof_hyp_id_t hyp_id =
            assn->mand_float_offset + fi;
        const laplace_proof_hyp_t* hyp =
            laplace_proof_get_hyp(store, hyp_id);
        if (!hyp || hyp->kind != LAPLACE_PROOF_HYP_FLOATING) {
            result->status       = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
            result->failure_step = step_index;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }

        const laplace_proof_expr_t* fexpr =
            laplace_proof_get_expr(store, hyp->expr_id);
        if (!fexpr || fexpr->token_count != 1u) {
            result->status       = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
            result->failure_step = step_index;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }

        const laplace_proof_symbol_id_t variable =
            store->token_pool[fexpr->token_offset];
        const laplace_proof_symbol_id_t required_typecode = fexpr->typecode;

        const laplace_proof_expr_id_t candidate_id = ctx->stack[base + fi];
        const laplace_proof_expr_t* candidate =
            laplace_proof_get_expr(store, candidate_id);
        if (!candidate) {
            result->status       = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
            result->failure_step = step_index;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }

        if (candidate->typecode != required_typecode) {
            result->status       = LAPLACE_PROOF_VERIFY_TYPECODE_MISMATCH;
            result->failure_step = step_index;
            result->failure_hyp  = hyp_id;
            result->failure_assertion = assertion_id;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }

        const uint32_t existing = verify_find_subst(ctx, variable);
        if (existing != UINT32_MAX) {
            const laplace_proof_verify_subst_t* es = &ctx->subst[existing];
            if (!verify_scratch_expr_equal_store(
                    ctx, required_typecode,
                    es->token_offset, es->token_count,
                    candidate_id)) {
                if (es->token_count != candidate->token_count ||
                    memcmp(&ctx->scratch_tokens[es->token_offset],
                           &store->token_pool[candidate->token_offset],
                           es->token_count * sizeof(laplace_proof_symbol_id_t)) != 0) {
                    result->status       = LAPLACE_PROOF_VERIFY_SUBSTITUTION_CONFLICT;
                    result->failure_step = step_index;
                    result->failure_hyp  = hyp_id;
                    result->failure_assertion = assertion_id;
                    ctx->scratch_token_used = saved_scratch;
                    return LAPLACE_ERR_INVALID_ARGUMENT;
                }
            }
        } else {
            if (ctx->subst_count >= LAPLACE_PROOF_VERIFY_MAX_SUBST_VARS) {
                result->status       = LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT;
                result->failure_step = step_index;
                ctx->scratch_token_used = saved_scratch;
                return LAPLACE_ERR_INTERNAL;
            }
            if (ctx->scratch_token_used + candidate->token_count >
                LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS) {
                result->status       = LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT;
                result->failure_step = step_index;
                ctx->scratch_token_used = saved_scratch;
                return LAPLACE_ERR_INTERNAL;
            }

            laplace_proof_verify_subst_t* ns =
                &ctx->subst[ctx->subst_count++];
            ns->variable     = variable;
            ns->typecode     = required_typecode;
            ns->token_offset = ctx->scratch_token_used;
            ns->token_count  = candidate->token_count;

            memcpy(&ctx->scratch_tokens[ctx->scratch_token_used],
                   &store->token_pool[candidate->token_offset],
                   candidate->token_count * sizeof(laplace_proof_symbol_id_t));
            ctx->scratch_token_used += candidate->token_count;
        }
    }

    for (uint32_t ei = 0u; ei < assn->mand_ess_count; ++ei) {
        const laplace_proof_hyp_id_t hyp_id =
            assn->mand_ess_offset + ei;
        const laplace_proof_hyp_t* hyp =
            laplace_proof_get_hyp(store, hyp_id);
        if (!hyp || hyp->kind != LAPLACE_PROOF_HYP_ESSENTIAL) {
            result->status       = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
            result->failure_step = step_index;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }

        laplace_proof_symbol_id_t inst_typecode;
        uint32_t inst_offset, inst_count;
        const laplace_proof_verify_status_t inst_status =
            verify_instantiate_expr(ctx, hyp->expr_id,
                                     &inst_typecode, &inst_offset, &inst_count);
        if (inst_status != LAPLACE_PROOF_VERIFY_OK) {
            result->status       = inst_status;
            result->failure_step = step_index;
            result->failure_hyp  = hyp_id;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INTERNAL;
        }

        const uint32_t stack_pos = base + assn->mand_float_count + ei;
        const laplace_proof_expr_id_t candidate_id = ctx->stack[stack_pos];

        if (!verify_scratch_expr_equal_store(
                ctx, inst_typecode, inst_offset, inst_count, candidate_id)) {
            result->status           = LAPLACE_PROOF_VERIFY_ESSENTIAL_HYP_MISMATCH;
            result->failure_step     = step_index;
            result->failure_hyp      = hyp_id;
            result->failure_assertion = assertion_id;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    if (assn->dv_count > 0u) {
        const laplace_proof_verify_status_t dv_status =
            verify_check_dv(ctx, assn, result, step_index);
        if (dv_status != LAPLACE_PROOF_VERIFY_OK) {
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    laplace_proof_symbol_id_t conc_typecode;
    uint32_t conc_offset, conc_count;
    const laplace_proof_verify_status_t conc_status =
        verify_instantiate_expr(ctx, assn->conclusion_id,
                                 &conc_typecode, &conc_offset, &conc_count);
    if (conc_status != LAPLACE_PROOF_VERIFY_OK) {
        result->status       = conc_status;
        result->failure_step = step_index;
        ctx->scratch_token_used = saved_scratch;
        return LAPLACE_ERR_INTERNAL;
    }

    ctx->stack_depth = base;

    laplace_proof_expr_id_t result_expr_id =
        LAPLACE_PROOF_EXPR_ID_INVALID;

    for (uint32_t eid = 1u; eid <= store->expr_count; ++eid) {
        if (verify_scratch_expr_equal_store(
                ctx, conc_typecode, conc_offset, conc_count, eid)) {
            result_expr_id = eid;
            break;
        }
    }

    if (result_expr_id == LAPLACE_PROOF_EXPR_ID_INVALID) {

        result_expr_id = 0x80000000u | conc_offset;

        if (ctx->scratch_token_used + 2u > LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS) {
            result->status       = LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT;
            result->failure_step = step_index;
            ctx->scratch_token_used = saved_scratch;
            return LAPLACE_ERR_INTERNAL;
        }
        ctx->scratch_tokens[ctx->scratch_token_used++] = conc_typecode;
        ctx->scratch_tokens[ctx->scratch_token_used++] = conc_count;
    }

    const laplace_proof_verify_status_t push_status =
        verify_stack_push(ctx, result_expr_id);
    if (push_status != LAPLACE_PROOF_VERIFY_OK) {
        result->status       = push_status;
        result->failure_step = step_index;
        ctx->scratch_token_used = saved_scratch;
        return LAPLACE_ERR_INTERNAL;
    }

    ctx->total_assertions_applied++;
    return LAPLACE_OK;
}

static laplace_proof_verify_status_t verify_final_check(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_expr_id_t conclusion_id) {

    if (ctx->stack_depth != 1u) {
        return LAPLACE_PROOF_VERIFY_FINAL_MISMATCH;
    }

    const laplace_proof_expr_id_t top = ctx->stack[0];

    if (top == conclusion_id) {
        return LAPLACE_PROOF_VERIFY_OK;
    }

    if ((top & 0x80000000u) == 0u) {
        if (verify_expr_equal(ctx->store, top, conclusion_id)) {
            return LAPLACE_PROOF_VERIFY_OK;
        }
        return LAPLACE_PROOF_VERIFY_FINAL_MISMATCH;
    }

    const uint32_t scratch_offset = top & 0x7FFFFFFFu;

    if (ctx->scratch_token_used < 2u) {
        return LAPLACE_PROOF_VERIFY_INTERNAL_INVARIANT;
    }

    const laplace_proof_symbol_id_t synth_typecode =
        ctx->scratch_tokens[ctx->scratch_token_used - 2u];
    const uint32_t synth_count =
        ctx->scratch_tokens[ctx->scratch_token_used - 1u];

    if (verify_scratch_expr_equal_store(
            ctx, synth_typecode, scratch_offset, synth_count,
            conclusion_id)) {
        return LAPLACE_PROOF_VERIFY_OK;
    }

    return LAPLACE_PROOF_VERIFY_FINAL_MISMATCH;
}

laplace_error_t laplace_proof_verify_theorem(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_theorem_id_t theorem_id,
    laplace_proof_verify_result_t* result) {

    if (!ctx || !ctx->initialized || !result) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));
    const laplace_proof_store_t* store = ctx->store;

    const laplace_proof_theorem_t* thm =
        laplace_proof_get_theorem(store, theorem_id);
    if (!thm) {
        result->status = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
        return LAPLACE_OK;
    }

    const laplace_proof_assertion_t* assn =
        laplace_proof_get_assertion(store, thm->assertion_id);
    if (!assn) {
        result->status = LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT;
        return LAPLACE_OK;
    }

    laplace_proof_verify_reset(ctx);

    const laplace_proof_step_t* steps;
    uint32_t step_count;
    if (thm->step_count == 0u) {
        steps = NULL;
        step_count = 0u;
    } else {
        steps = &store->proof_steps[thm->step_offset];
        step_count = thm->step_count;
    }

    for (uint32_t si = 0u; si < step_count; ++si) {
        const laplace_proof_step_t* step = &steps[si];

        switch (step->kind) {
        case LAPLACE_PROOF_STEP_HYP: {
            const laplace_proof_hyp_t* hyp =
                laplace_proof_get_hyp(store, step->ref_id);
            if (!hyp) {
                result->status       = LAPLACE_PROOF_VERIFY_INVALID_STEP_REF;
                result->failure_step = si;
                result->steps_processed = si;
                return LAPLACE_OK;
            }

            const laplace_proof_verify_status_t push_status =
                verify_stack_push(ctx, hyp->expr_id);
            if (push_status != LAPLACE_PROOF_VERIFY_OK) {
                result->status       = push_status;
                result->failure_step = si;
                result->steps_processed = si;
                return LAPLACE_OK;
            }
            ctx->total_hyps_pushed++;
            break;
        }

        case LAPLACE_PROOF_STEP_ASSERTION: {
            const laplace_error_t err =
                laplace_proof_verify_apply_assertion(
                    ctx, step->ref_id, result, si);
            if (err != LAPLACE_OK) {
                result->steps_processed = si;
                return LAPLACE_OK;
            }
            break;
        }

        default:
            result->status       = LAPLACE_PROOF_VERIFY_INVALID_STEP_REF;
            result->failure_step = si;
            result->steps_processed = si;
            return LAPLACE_OK;
        }

        ctx->total_steps++;
    }

    result->steps_processed = step_count;

    result->status = verify_final_check(ctx, assn->conclusion_id);
    return LAPLACE_OK;
}

static const char* const verify_status_names[] = {
    "ok",
    "invalid_artifact",
    "invalid_step_ref",
    "stack_underflow",
    "stack_overflow",
    "substitution_conflict",
    "typecode_mismatch",
    "essential_hyp_mismatch",
    "dv_violation",
    "final_mismatch",
    "resource_limit",
    "internal_invariant"
};

const char* laplace_proof_verify_status_string(
    laplace_proof_verify_status_t status) {
    if (status >= LAPLACE_PROOF_VERIFY_STATUS_COUNT_) {
        return "unknown";
    }
    return verify_status_names[status];
}
