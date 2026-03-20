#include "laplace/derive.h"

#include <string.h>

#include "laplace/adapter.h"
#include "laplace/branch.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/hv.h"
#include "laplace/observe.h"
#include "laplace/proof.h"
#include "laplace/proof_search.h"
#include "laplace/proof_verify.h"
#include "laplace/trace.h"
#include "laplace/version.h"

static laplace_derive_status_t derive_map_error(laplace_error_t err) {
    switch (err) {
        case LAPLACE_OK:                     return LAPLACE_DERIVE_STATUS_OK;
        case LAPLACE_ERR_INVALID_ARGUMENT:   return LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        case LAPLACE_ERR_CAPACITY_EXHAUSTED: return LAPLACE_DERIVE_STATUS_RESOURCE_LIMIT;
        case LAPLACE_ERR_OUT_OF_RANGE:       return LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        case LAPLACE_ERR_INVALID_STATE:      return LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        case LAPLACE_ERR_GENERATION_MISMATCH:return LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        case LAPLACE_ERR_NOT_SUPPORTED:      return LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
        default:                             return LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
    }
}

static void derive_init_result(
    laplace_derive_result_t*       result,
    const laplace_derive_action_t* action)
{
    memset(result, 0, sizeof(*result));
    result->api_version       = LAPLACE_DERIVE_API_VERSION;
    result->kernel            = action->kernel;
    result->action            = action->action;
    result->correlation_id    = action->correlation_id;
    result->branch_id         = action->branch_id;
    result->branch_generation = action->branch_generation;
    result->epoch_id          = action->epoch_id;
}

static void derive_emit_trace(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    const laplace_derive_result_t* result)
{
    if (ctx->observe == NULL) {
        return;
    }

    const laplace_observe_level_t level = laplace_observe_get_level(ctx->observe);
    if (level < LAPLACE_OBSERVE_AUDIT) {
        return;
    }

    const uint32_t mask = laplace_observe_get_mask(ctx->observe);
    if ((mask & LAPLACE_OBSERVE_MASK_DERIVE) == 0u) {
        return;
    }

    laplace_trace_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind           = (uint16_t)LAPLACE_TRACE_KIND_DERIVE_DISPATCH;
    rec.subsystem      = (uint8_t)LAPLACE_TRACE_SUBSYSTEM_DERIVE;
    rec.status         = result->status;
    rec.branch_id      = action->branch_id;
    rec.branch_gen     = action->branch_generation;
    rec.epoch_id       = action->epoch_id;
    rec.correlation_id = action->correlation_id;

    laplace_trace_derive_payload_t dp;
    dp.kernel        = action->kernel;
    dp.action        = action->action;
    dp.result_kind   = result->result_kind;
    dp.derive_status = result->status;
    dp.detail        = 0;
    dp.reserved      = 0;
    memcpy(rec.payload.raw, &dp, sizeof(dp));

    laplace_observe_emit(ctx->observe, &rec);
}

static void derive_dispatch_capabilities(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    (void)action;
    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_CAPABILITY;

    laplace_derive_capability_result_t* cap = &result->payload.capability;
    cap->abi_version          = LAPLACE_DERIVE_API_VERSION;
    cap->kernel_version_major = laplace_version_major();
    cap->kernel_version_minor = laplace_version_minor();
    cap->kernel_version_patch = laplace_version_patch();
    cap->hv_dimension         = LAPLACE_HV_DIM;
    cap->hv_words             = LAPLACE_HV_WORDS;
    cap->max_predicates       = LAPLACE_EXACT_MAX_PREDICATES;
    cap->max_facts            = LAPLACE_EXACT_MAX_FACTS;
    cap->max_rules            = LAPLACE_EXACT_MAX_RULES;
    cap->max_arity            = LAPLACE_EXACT_MAX_ARITY;
    cap->max_rule_body_literals = LAPLACE_EXACT_MAX_RULE_BODY_LITERALS;
    cap->max_branches         = LAPLACE_BRANCH_MAX_BRANCHES;
    cap->supported_kernels    = (1u << LAPLACE_KERNEL_RELATIONAL);
    if (ctx->proof_store != NULL) {
        cap->supported_kernels |= (1u << LAPLACE_KERNEL_PROOF);
    }

    (void)ctx;
}

static void derive_dispatch_stats(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    (void)action;

    if (ctx->store == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_STATS;

    laplace_derive_stats_result_t* s = &result->payload.stats;
    s->predicate_count    = ctx->store->predicate_count;
    s->fact_count         = ctx->store->fact_count;
    s->rule_count         = ctx->store->rule_count;

    if (ctx->store->entity_pool != NULL) {
        s->entity_alive_count = ctx->store->entity_pool->alive_count;
        s->entity_capacity    = ctx->store->entity_pool->capacity;
    }

    if (ctx->exec != NULL) {
        const laplace_exec_stats_t* es = laplace_exec_get_stats(ctx->exec);
        s->exec_steps              = es->steps_executed;
        s->exec_facts_derived      = es->facts_derived;
        s->exec_facts_deduplicated = es->facts_deduplicated;
        s->exec_fixpoint_rounds    = es->fixpoint_rounds;
    }
}

static void derive_rel_assert_fact(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->store == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_derive_rel_assert_fact_payload_t* p =
        &action->payload.rel_assert_fact;

    if (p->arg_count > LAPLACE_DERIVE_MAX_FACT_ARGS) {
        result->status      = LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    laplace_entity_handle_t handles[LAPLACE_DERIVE_MAX_FACT_ARGS];
    for (uint32_t i = 0; i < p->arg_count; ++i) {
        handles[i].id         = (laplace_entity_id_t)p->args[i].id;
        handles[i].generation = (laplace_generation_t)p->args[i].generation;
    }

    laplace_exact_provenance_desc_t prov_desc;
    memset(&prov_desc, 0, sizeof(prov_desc));
    prov_desc.kind           = LAPLACE_EXACT_PROVENANCE_ASSERTED;
    prov_desc.source_rule_id = LAPLACE_RULE_ID_INVALID;
    prov_desc.reserved_epoch  = action->epoch_id;
    prov_desc.reserved_branch = action->branch_id;

    laplace_provenance_id_t prov_id = LAPLACE_PROVENANCE_ID_INVALID;
    laplace_error_t err = laplace_exact_insert_provenance(
        ctx->store, &prov_desc, &prov_id);
    if (err != LAPLACE_OK) {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const uint32_t fact_flags =
        p->flags | LAPLACE_EXACT_FACT_FLAG_ASSERTED | LAPLACE_EXACT_FACT_FLAG_COMMITTED;

    laplace_exact_fact_row_t fact_row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t  fact_entity = {LAPLACE_ENTITY_ID_INVALID,
                                             LAPLACE_GENERATION_INVALID};
    bool inserted = false;

    err = laplace_exact_assert_fact(
        ctx->store,
        (laplace_predicate_id_t)p->predicate_id,
        handles, p->arg_count,
        prov_id, fact_flags,
        &fact_row, &fact_entity, &inserted);

    if (err != LAPLACE_OK) {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_REL_FACT;

    laplace_derive_rel_fact_result_t* fr = &result->payload.rel_fact;
    fr->fact_row      = (uint32_t)fact_row;
    fr->entity_id     = (uint32_t)fact_entity.id;
    fr->generation    = (uint32_t)fact_entity.generation;
    fr->provenance_id = (uint32_t)prov_id;
    fr->inserted      = inserted ? 1u : 0u;
}

static void derive_rel_lookup_fact(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->store == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_derive_rel_lookup_fact_payload_t* p =
        &action->payload.rel_lookup_fact;

    if (p->arg_count > LAPLACE_DERIVE_MAX_FACT_ARGS) {
        result->status      = LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    laplace_entity_id_t ids[LAPLACE_DERIVE_MAX_FACT_ARGS];
    for (uint32_t i = 0; i < p->arg_count; ++i) {
        ids[i] = (laplace_entity_id_t)p->args[i].id;
    }

    const laplace_exact_fact_row_t row = laplace_exact_find_fact(
        ctx->store,
        (laplace_predicate_id_t)p->predicate_id,
        ids, p->arg_count);

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_REL_LOOKUP;

    laplace_derive_rel_lookup_result_t* lr = &result->payload.rel_lookup;
    lr->fact_row = (uint32_t)row;
    lr->found    = (row != LAPLACE_EXACT_FACT_ROW_INVALID) ? 1u : 0u;
}

static void derive_convert_literal(
    laplace_exact_literal_t*         out,
    const laplace_derive_literal_t*  in)
{
    memset(out, 0, sizeof(*out));
    out->predicate = (laplace_predicate_id_t)in->predicate_id;
    out->arity     = in->arity;
    for (uint8_t i = 0; i < in->arity && i < LAPLACE_DERIVE_MAX_ARITY; ++i) {
        out->terms[i].kind = (laplace_exact_term_kind_t)in->terms[i].kind;
        if (in->terms[i].kind == (uint32_t)LAPLACE_EXACT_TERM_VARIABLE) {
            out->terms[i].value.variable = (laplace_exact_var_id_t)in->terms[i].value;
        } else if (in->terms[i].kind == (uint32_t)LAPLACE_EXACT_TERM_CONSTANT) {
            out->terms[i].value.constant = (laplace_entity_id_t)in->terms[i].value;
        }
    }
}

static void derive_rel_add_rule(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->store == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_derive_rel_add_rule_payload_t* p =
        &action->payload.rel_add_rule;

    if (p->body_count == 0u || p->body_count > LAPLACE_DERIVE_MAX_RULE_BODY) {
        result->status      = LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    laplace_exact_literal_t head;
    derive_convert_literal(&head, &p->head);

    laplace_exact_literal_t body[LAPLACE_DERIVE_MAX_RULE_BODY];
    for (uint32_t i = 0; i < p->body_count; ++i) {
        derive_convert_literal(&body[i], &p->body[i]);
    }

    laplace_exact_rule_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.head          = head;
    desc.body_literals = body;
    desc.body_count    = p->body_count;

    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t validation;
    memset(&validation, 0, sizeof(validation));

    const laplace_error_t err = laplace_exact_add_rule(
        ctx->store, &desc, &rule_id, &validation);

    laplace_derive_rel_rule_result_t* rr = &result->payload.rel_rule;
    rr->rule_id          = (uint32_t)rule_id;
    rr->validation_error = (uint32_t)validation.error;
    rr->literal_index    = validation.literal_index;
    rr->term_index       = validation.term_index;

    if (err == LAPLACE_OK &&
        validation.error == LAPLACE_EXACT_RULE_VALIDATION_OK) {
        result->status      = LAPLACE_DERIVE_STATUS_OK;
        result->result_kind = LAPLACE_DERIVE_RESULT_REL_RULE;
    } else if (validation.error != LAPLACE_EXACT_RULE_VALIDATION_OK) {
        result->status      = LAPLACE_DERIVE_STATUS_VALIDATION_FAILED;
        result->result_kind = LAPLACE_DERIVE_RESULT_REL_RULE;
    } else {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
    }
}

static void derive_rel_build_trigger_index(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    (void)action;

    if (ctx->exec == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_error_t err = laplace_exec_build_trigger_index(ctx->exec);
    if (err == LAPLACE_OK) {
        result->status      = LAPLACE_DERIVE_STATUS_OK;
        result->result_kind = LAPLACE_DERIVE_RESULT_ACK;
    } else {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
    }
}

static void derive_rel_exec_step(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    (void)action;

    if (ctx->exec == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_error_t err = laplace_exec_step(ctx->exec);

    const laplace_exec_stats_t* es = laplace_exec_get_stats(ctx->exec);

    result->result_kind = LAPLACE_DERIVE_RESULT_REL_EXEC;
    result->status      = (err == LAPLACE_OK)
                        ? LAPLACE_DERIVE_STATUS_OK
                        : derive_map_error(err);

    laplace_derive_rel_exec_result_t* er = &result->payload.rel_exec;
    er->run_status          = 0u;
    er->steps_executed      = es->steps_executed;
    er->facts_derived       = es->facts_derived;
    er->facts_deduplicated  = es->facts_deduplicated;
    er->fixpoint_rounds     = es->fixpoint_rounds;
}

static void derive_rel_exec_run(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->exec == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    const laplace_derive_rel_exec_run_payload_t* p =
        &action->payload.rel_exec_run;

    if (p->max_steps > 0u) {
        laplace_exec_set_max_steps(ctx->exec, p->max_steps);
    }
    if (p->max_derivations > 0u) {
        laplace_exec_set_max_derivations(ctx->exec, p->max_derivations);
    }
    if (p->mode <= (uint8_t)LAPLACE_EXEC_MODE_SPARSE) {
        laplace_exec_set_mode(ctx->exec, (laplace_exec_mode_t)p->mode);
    }
    laplace_exec_set_semi_naive(ctx->exec, p->semi_naive != 0u);

    laplace_exec_mark_all_facts_ready(ctx->exec);
    if (!ctx->exec->trigger_index_built) {
        const laplace_error_t build_err =
            laplace_exec_build_trigger_index(ctx->exec);
        if (build_err != LAPLACE_OK) {
            result->status      = derive_map_error(build_err);
            result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
            return;
        }
    }

    const laplace_exec_run_status_t run_status = laplace_exec_run(ctx->exec);
    const laplace_exec_stats_t* es = laplace_exec_get_stats(ctx->exec);

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_REL_EXEC;

    laplace_derive_rel_exec_result_t* er = &result->payload.rel_exec;
    er->run_status          = (uint32_t)run_status;
    er->steps_executed      = es->steps_executed;
    er->facts_derived       = es->facts_derived;
    er->facts_deduplicated  = es->facts_deduplicated;
    er->fixpoint_rounds     = es->fixpoint_rounds;
}

static void derive_dispatch_relational(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    switch (action->action) {
        case LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT:
            derive_rel_assert_fact(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_REL_LOOKUP_FACT:
            derive_rel_lookup_fact(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_REL_ADD_RULE:
            derive_rel_add_rule(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_REL_BUILD_TRIGGER_IDX:
            derive_rel_build_trigger_index(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_REL_EXEC_STEP:
            derive_rel_exec_step(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_REL_EXEC_RUN:
            derive_rel_exec_run(ctx, action, result);
            break;
        default:
            result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
            result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION;
            break;
    }
}

static void derive_proof_verify(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->proof_store == NULL || ctx->proof_verifier == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL;
        result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL;
        return;
    }

    const laplace_derive_proof_verify_payload_t* p =
        &action->payload.proof_verify;

    if (p->theorem_id == 0u) {
        result->status      = LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    laplace_proof_verify_result_t vr;
    const laplace_error_t err = laplace_proof_verify_theorem(
        (laplace_proof_verify_context_t*)ctx->proof_verifier,
        p->theorem_id, &vr);

    if (err != LAPLACE_OK) {
        result->status      = LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->result_kind = LAPLACE_DERIVE_RESULT_PROOF_VERIFY;
    result->status      = (vr.status == LAPLACE_PROOF_VERIFY_OK)
                        ? LAPLACE_DERIVE_STATUS_OK
                        : LAPLACE_DERIVE_STATUS_VALIDATION_FAILED;

    laplace_derive_proof_verify_result_t* pr = &result->payload.proof_verify;
    pr->verify_status     = (uint32_t)vr.status;
    pr->failure_step      = vr.failure_step;
    pr->failure_assertion = vr.failure_assertion;
    pr->failure_hyp       = vr.failure_hyp;
    pr->detail_a          = vr.detail_a;
    pr->detail_b          = vr.detail_b;
    pr->steps_processed   = vr.steps_processed;
}

static void derive_proof_build_index(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    (void)action;
    if (ctx->proof_search == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
        result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION;
        return;
    }

    const laplace_error_t err = laplace_proof_search_build_index(
        (laplace_proof_search_context_t*)ctx->proof_search);
    if (err != LAPLACE_OK) {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_ACK;
}

static void derive_proof_query_candidates(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->proof_search == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
        result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION;
        return;
    }

    const laplace_derive_proof_search_candidates_payload_t* p =
        &action->payload.proof_search_candidates;

    laplace_proof_search_candidate_buf_t buf;
    const laplace_error_t err = laplace_proof_search_query_candidates(
        (laplace_proof_search_context_t*)ctx->proof_search,
        p->goal_expr_id, &buf);

    if (err != LAPLACE_OK) {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->status      = LAPLACE_DERIVE_STATUS_OK;
    result->result_kind = LAPLACE_DERIVE_RESULT_PROOF_CANDIDATES;

    laplace_derive_proof_candidates_result_t* cr = &result->payload.proof_candidates;
    cr->candidate_count = buf.count;
    cr->total_matches   = buf.total_matches;
    cr->truncated       = buf.truncated ? 1u : 0u;
}

static void derive_proof_try_assertion_search(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->proof_search == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
        result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION;
        return;
    }

    const laplace_derive_proof_search_try_payload_t* p =
        &action->payload.proof_search_try;

    laplace_proof_search_try_result_t tr;
    const laplace_error_t err = laplace_proof_search_try_assertion(
        (laplace_proof_search_context_t*)ctx->proof_search,
        p->goal_expr_id, p->assertion_id, &tr);

    if (err != LAPLACE_OK) {
        result->status      = derive_map_error(err);
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        return;
    }

    result->result_kind = LAPLACE_DERIVE_RESULT_PROOF_SEARCH;
    result->status      = (tr.status == LAPLACE_PROOF_SEARCH_SUCCESS)
                        ? LAPLACE_DERIVE_STATUS_OK
                        : LAPLACE_DERIVE_STATUS_VALIDATION_FAILED;

    laplace_derive_proof_search_result_t* sr = &result->payload.proof_search;
    sr->search_status = (uint32_t)tr.status;
    sr->subgoal_count = tr.subgoal_count;
    sr->assertion_id  = tr.assertion_id;
    sr->detail_a      = tr.detail_a;
    sr->detail_b      = tr.detail_b;
    for (uint32_t i = 0u; i < tr.subgoal_count && i < 16u; ++i) {
        sr->subgoal_expr_ids[i] = tr.subgoals[i].expr_id;
    }
}

static void derive_dispatch_proof(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    if (ctx->proof_store == NULL) {
        result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL;
        result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL;
        return;
    }

    switch (action->action) {
        case LAPLACE_DERIVE_ACTION_PROOF_VERIFY:
            derive_proof_verify(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_PROOF_BUILD_INDEX:
            derive_proof_build_index(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_PROOF_QUERY_CANDIDATES:
            derive_proof_query_candidates(ctx, action, result);
            break;
        case LAPLACE_DERIVE_ACTION_PROOF_TRY_ASSERTION:
            derive_proof_try_assertion_search(ctx, action, result);
            break;
        default:
            result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION;
            result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION;
            break;
    }
}

void laplace_derive_context_init(
    laplace_derive_context_t*            ctx,
    struct laplace_exact_store*           store,
    struct laplace_exec_context*          exec,
    struct laplace_branch_system*         branch,
    struct laplace_observe_context*       observe,
    struct laplace_proof_store*           proof_store,
    struct laplace_proof_verify_context*  proof_verifier,
    struct laplace_proof_search_context*  proof_search)
{
    if (ctx == NULL) { return; }
    memset(ctx, 0, sizeof(*ctx));
    ctx->store          = store;
    ctx->exec           = exec;
    ctx->branch         = branch;
    ctx->observe        = observe;
    ctx->proof_store    = proof_store;
    ctx->proof_verifier = proof_verifier;
    ctx->proof_search   = proof_search;
}

void laplace_derive_dispatch(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result)
{
    LAPLACE_ASSERT(ctx != NULL);
    LAPLACE_ASSERT(action != NULL);
    LAPLACE_ASSERT(result != NULL);

    derive_init_result(result, action);

    if (action->api_version != LAPLACE_DERIVE_API_VERSION) {
        result->status      = LAPLACE_DERIVE_STATUS_INVALID_VERSION;
        result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
        derive_emit_trace(ctx, action, result);
        return;
    }

    if (action->action == LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES) {
        derive_dispatch_capabilities(ctx, action, result);
        derive_emit_trace(ctx, action, result);
        return;
    }
    if (action->action == LAPLACE_DERIVE_ACTION_QUERY_STATS) {
        derive_dispatch_stats(ctx, action, result);
        derive_emit_trace(ctx, action, result);
        return;
    }

    switch (action->kernel) {
        case LAPLACE_KERNEL_RELATIONAL:
            derive_dispatch_relational(ctx, action, result);
            break;

        case LAPLACE_KERNEL_PROOF:
            derive_dispatch_proof(ctx, action, result);
            break;

        case LAPLACE_KERNEL_BITVECTOR:
            result->status      = LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL;
            result->result_kind = LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL;
            break;

        case LAPLACE_KERNEL_INVALID:
        default:
            result->status      = LAPLACE_DERIVE_STATUS_INVALID_KERNEL;
            result->result_kind = LAPLACE_DERIVE_RESULT_REJECT;
            break;
    }

    derive_emit_trace(ctx, action, result);
}

static const char* const derive_status_names[] = {
    "ok",
    "invalid_version",
    "invalid_kernel",
    "unsupported_kernel",
    "unsupported_action",
    "invalid_argument",
    "invalid_layout",
    "validation_failed",
    "not_found",
    "conflict",
    "resource_limit",
    "budget_exhausted",
    "internal_invariant"
};

const char* laplace_derive_status_string(laplace_derive_status_t status) {
    if (status >= LAPLACE_DERIVE_STATUS_COUNT_) {
        return "unknown";
    }
    return derive_status_names[status];
}

const char* laplace_derive_action_string(laplace_derive_action_kind_t action) {
    switch (action) {
        case LAPLACE_DERIVE_ACTION_INVALID:               return "invalid";
        case LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES:    return "query_capabilities";
        case LAPLACE_DERIVE_ACTION_QUERY_STATS:           return "query_stats";
        case LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT:       return "rel_assert_fact";
        case LAPLACE_DERIVE_ACTION_REL_LOOKUP_FACT:       return "rel_lookup_fact";
        case LAPLACE_DERIVE_ACTION_REL_ADD_RULE:          return "rel_add_rule";
        case LAPLACE_DERIVE_ACTION_REL_BUILD_TRIGGER_IDX: return "rel_build_trigger_idx";
        case LAPLACE_DERIVE_ACTION_REL_EXEC_STEP:         return "rel_exec_step";
        case LAPLACE_DERIVE_ACTION_REL_EXEC_RUN:          return "rel_exec_run";
        case LAPLACE_DERIVE_ACTION_PROOF_IMPORT:          return "proof_import";
        case LAPLACE_DERIVE_ACTION_PROOF_TRY_ASSERTION:   return "proof_try_assertion";
        case LAPLACE_DERIVE_ACTION_PROOF_STEP:            return "proof_step";
        case LAPLACE_DERIVE_ACTION_PROOF_VERIFY:          return "proof_verify";
        case LAPLACE_DERIVE_ACTION_PROOF_BUILD_INDEX:      return "proof_build_index";
        case LAPLACE_DERIVE_ACTION_PROOF_QUERY_CANDIDATES: return "proof_query_candidates";
        case LAPLACE_DERIVE_ACTION_PROOF_EXPAND_STATE:     return "proof_expand_state";
        case LAPLACE_DERIVE_ACTION_BV_IMPORT:             return "bv_import";
        case LAPLACE_DERIVE_ACTION_BV_ASSERT_CONSTRAINT:  return "bv_assert_constraint";
        case LAPLACE_DERIVE_ACTION_BV_VALIDATE_WITNESS:   return "bv_validate_witness";
        case LAPLACE_DERIVE_ACTION_BV_VERIFY_CERTIFICATE: return "bv_verify_certificate";
        default:                                           return "unknown";
    }
}

const char* laplace_derive_result_kind_string(laplace_derive_result_kind_t kind) {
    switch (kind) {
        case LAPLACE_DERIVE_RESULT_INVALID:            return "invalid";
        case LAPLACE_DERIVE_RESULT_ACK:                return "ack";
        case LAPLACE_DERIVE_RESULT_REJECT:             return "reject";
        case LAPLACE_DERIVE_RESULT_CAPABILITY:         return "capability";
        case LAPLACE_DERIVE_RESULT_STATS:              return "stats";
        case LAPLACE_DERIVE_RESULT_REL_FACT:           return "rel_fact";
        case LAPLACE_DERIVE_RESULT_REL_LOOKUP:         return "rel_lookup";
        case LAPLACE_DERIVE_RESULT_REL_RULE:           return "rel_rule";
        case LAPLACE_DERIVE_RESULT_REL_EXEC:           return "rel_exec";
        case LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL: return "unsupported_kernel";
        case LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION: return "unsupported_action";
        case LAPLACE_DERIVE_RESULT_PROOF_VERIFY:       return "proof_verify";
        case LAPLACE_DERIVE_RESULT_PROOF_SEARCH:        return "proof_search";
        case LAPLACE_DERIVE_RESULT_PROOF_CANDIDATES:    return "proof_candidates";
        default:                                        return "unknown";
    }
}
