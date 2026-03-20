#include "laplace/graph_import.h"
#include "laplace/graph_artifact.h"
#include "laplace/graph_profile.h"
#include "laplace/exact.h"
#include "laplace/entity.h"
#include "laplace/state.h"

#include <stddef.h>
#include <string.h>

void laplace_graph_import_context_init(
    laplace_graph_import_context_t* const ctx,
    laplace_exact_store_t* const          store,
    laplace_entity_pool_t* const          entity_pool,
    laplace_exec_context_t* const         exec)
{
    if (ctx == NULL) { return; }
    ctx->store       = store;
    ctx->entity_pool = entity_pool;
    ctx->exec        = exec;
}

bool laplace_graph_import_supports_profile(
    const laplace_graph_profile_id_t profile_id)
{
    return profile_id == LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES ||
           profile_id == LAPLACE_GRAPH_PROFILE_HORN_CLOSURE;
}

typedef struct import_pred_map {
    uint16_t              local_id;
    laplace_predicate_id_t kernel_id;
} import_pred_map_t;

typedef struct import_entity_map {
    uint32_t              local_id;
    laplace_entity_handle_t kernel_handle;
} import_entity_map_t;

static laplace_predicate_id_t
find_kernel_pred(const import_pred_map_t* const map,
                 const uint32_t count,
                 const uint16_t local_id)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (map[i].local_id == local_id) {
            return map[i].kernel_id;
        }
    }
    return LAPLACE_PREDICATE_ID_INVALID;
}

static laplace_entity_handle_t
find_kernel_entity(const import_entity_map_t* const map,
                   const uint32_t count,
                   const uint32_t local_id)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (map[i].local_id == local_id) {
            return map[i].kernel_handle;
        }
    }
    const laplace_entity_handle_t invalid = {
        .id = LAPLACE_ENTITY_ID_INVALID,
        .generation = LAPLACE_GENERATION_INVALID
    };
    return invalid;
}

laplace_graph_import_status_t
laplace_graph_import(
    laplace_graph_import_context_t* const  ctx,
    const laplace_graph_artifact_t* const  artifact,
    laplace_graph_import_result_t* const   out_result)
{
    if (out_result == NULL) {
        return LAPLACE_GRAPH_IMPORT_ERR_NULL;
    }
    memset(out_result, 0, sizeof(*out_result));

    if (ctx == NULL || artifact == NULL ||
        ctx->store == NULL || ctx->entity_pool == NULL) {
        out_result->status = LAPLACE_GRAPH_IMPORT_ERR_NULL;
        return out_result->status;
    }

    laplace_graph_artifact_validation_t val_detail;
    memset(&val_detail, 0, sizeof(val_detail));
    const laplace_graph_artifact_status_t val_status =
        laplace_graph_artifact_validate(artifact, &val_detail);
    if (val_status != LAPLACE_GRAPH_ARTIFACT_OK) {
        out_result->status = LAPLACE_GRAPH_IMPORT_ERR_VALIDATION_FAILED;
        out_result->validation_status = val_status;
        out_result->failure_record_index = val_detail.record_index;
        return out_result->status;
    }

    const laplace_graph_artifact_header_t* const hdr = &artifact->header;

    import_pred_map_t pred_map[LAPLACE_GRAPH_ARTIFACT_MAX_PREDICATES];
    memset(pred_map, 0, sizeof(pred_map));

    for (uint32_t i = 0u; i < hdr->predicate_count; ++i) {
        const laplace_graph_artifact_predicate_t* const ap = &artifact->predicates[i];

        /* Kernel predicate ID = local_id + 1 (avoid INVALID=0) */
        const laplace_predicate_id_t kernel_pid = (laplace_predicate_id_t)(ap->local_id + 1u);

        /* If already declared, check arity match; otherwise register */
        if (laplace_exact_predicate_is_declared(ctx->store, kernel_pid)) {
            const uint8_t existing_arity = laplace_exact_predicate_arity(ctx->store, kernel_pid);
            if (existing_arity != ap->arity) {
                out_result->status = LAPLACE_GRAPH_IMPORT_ERR_PREDICATE_CAPACITY;
                out_result->failure_record_index = i;
                return out_result->status;
            }
        } else {
            const laplace_exact_predicate_desc_t desc = {
                .arity = ap->arity,
                .flags = 0u,
                .fact_capacity = 256u
            };
            const laplace_error_t err =
                laplace_exact_register_predicate(ctx->store, kernel_pid, &desc);
            if (err != LAPLACE_OK) {
                out_result->status = LAPLACE_GRAPH_IMPORT_ERR_PREDICATE_CAPACITY;
                out_result->failure_record_index = i;
                return out_result->status;
            }
        }

        pred_map[i].local_id  = ap->local_id;
        pred_map[i].kernel_id = kernel_pid;
        out_result->predicates_imported++;
    }

    import_entity_map_t entity_map[LAPLACE_GRAPH_ARTIFACT_MAX_ENTITIES];
    memset(entity_map, 0, sizeof(entity_map));

    for (uint32_t i = 0u; i < hdr->entity_count; ++i) {
        const laplace_graph_artifact_entity_t* const ae = &artifact->entities[i];

        const laplace_entity_handle_t h = laplace_entity_pool_alloc(ctx->entity_pool);
        if (h.id == LAPLACE_ENTITY_ID_INVALID) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_ENTITY_CAPACITY;
            out_result->failure_record_index = i;
            return out_result->status;
        }

        /* Set to READY state */
        laplace_error_t err = laplace_entity_pool_set_state(
            ctx->entity_pool, h, LAPLACE_STATE_READY);
        if (err != LAPLACE_OK) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_INTERNAL;
            out_result->failure_record_index = i;
            return out_result->status;
        }

        /* Register as constant */
        err = laplace_exact_register_constant(ctx->store, h, 1u, 0u);
        if (err != LAPLACE_OK) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_ENTITY_CAPACITY;
            out_result->failure_record_index = i;
            return out_result->status;
        }

        entity_map[i].local_id      = ae->local_id;
        entity_map[i].kernel_handle = h;
        out_result->entities_imported++;
    }

    /* Create a single provenance record for all imported asserted facts */
    laplace_exact_provenance_desc_t prov_desc;
    memset(&prov_desc, 0, sizeof(prov_desc));
    prov_desc.kind           = LAPLACE_EXACT_PROVENANCE_ASSERTED;
    prov_desc.source_rule_id = LAPLACE_RULE_ID_INVALID;
    prov_desc.parent_facts   = NULL;
    prov_desc.parent_count   = 0u;

    laplace_provenance_id_t import_prov = LAPLACE_PROVENANCE_ID_INVALID;
    {
        const laplace_error_t err = laplace_exact_insert_provenance(
            ctx->store, &prov_desc, &import_prov);
        if (err != LAPLACE_OK) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_PROVENANCE_CAPACITY;
            return out_result->status;
        }
    }

    for (uint32_t i = 0u; i < hdr->fact_count; ++i) {
        const laplace_graph_artifact_fact_t* const af = &artifact->facts[i];

        const laplace_predicate_id_t kpred =
            find_kernel_pred(pred_map, hdr->predicate_count, af->predicate_local_id);
        if (kpred == LAPLACE_PREDICATE_ID_INVALID) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_INTERNAL;
            out_result->failure_record_index = i;
            return out_result->status;
        }

        laplace_entity_handle_t args[LAPLACE_EXACT_MAX_ARITY];
        memset(args, 0, sizeof(args));
        for (uint32_t a = 0u; a < af->arg_count; ++a) {
            args[a] = find_kernel_entity(
                entity_map, hdr->entity_count, af->arg_entity_local_ids[a]);
            if (args[a].id == LAPLACE_ENTITY_ID_INVALID) {
                out_result->status = LAPLACE_GRAPH_IMPORT_ERR_INTERNAL;
                out_result->failure_record_index = i;
                return out_result->status;
            }
        }

        laplace_exact_fact_row_t fact_row = LAPLACE_EXACT_FACT_ROW_INVALID;
        laplace_entity_handle_t fact_entity = {0};
        bool inserted = false;

        const uint32_t fact_flags =
            LAPLACE_EXACT_FACT_FLAG_ASSERTED | LAPLACE_EXACT_FACT_FLAG_COMMITTED;

        const laplace_error_t err = laplace_exact_assert_fact(
            ctx->store, kpred, args, af->arg_count,
            import_prov, fact_flags, &fact_row, &fact_entity, &inserted);

        if (err != LAPLACE_OK) {
            out_result->status = LAPLACE_GRAPH_IMPORT_ERR_FACT_CAPACITY;
            out_result->failure_record_index = i;
            return out_result->status;
        }

        if (inserted) {
            out_result->facts_imported++;
        } else {
            out_result->facts_deduplicated++;
        }
    }

    if (hdr->rule_count > 0u && artifact->rules != NULL) {
        for (uint32_t i = 0u; i < hdr->rule_count; ++i) {
            const laplace_graph_artifact_rule_t* const ar = &artifact->rules[i];

            /* Convert head literal */
            laplace_exact_literal_t head;
            memset(&head, 0, sizeof(head));
            head.predicate = find_kernel_pred(
                pred_map, hdr->predicate_count, ar->head.predicate_local_id);
            head.arity = ar->head.arity;
            for (uint8_t t = 0u; t < ar->head.arity; ++t) {
                if (ar->head.terms[t].kind == 1u) { /* VARIABLE */
                    head.terms[t].kind = LAPLACE_EXACT_TERM_VARIABLE;
                    head.terms[t].value.variable = (laplace_exact_var_id_t)ar->head.terms[t].value;
                } else { /* CONSTANT */
                    head.terms[t].kind = LAPLACE_EXACT_TERM_CONSTANT;
                    const laplace_entity_handle_t eh = find_kernel_entity(
                        entity_map, hdr->entity_count, ar->head.terms[t].value);
                    head.terms[t].value.constant = eh.id;
                }
            }

            /* Convert body literals */
            laplace_exact_literal_t body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
            memset(body, 0, sizeof(body));
            for (uint32_t b = 0u; b < ar->body_count; ++b) {
                body[b].predicate = find_kernel_pred(
                    pred_map, hdr->predicate_count, ar->body[b].predicate_local_id);
                body[b].arity = ar->body[b].arity;
                for (uint8_t t = 0u; t < ar->body[b].arity; ++t) {
                    if (ar->body[b].terms[t].kind == 1u) { /* VARIABLE */
                        body[b].terms[t].kind = LAPLACE_EXACT_TERM_VARIABLE;
                        body[b].terms[t].value.variable =
                            (laplace_exact_var_id_t)ar->body[b].terms[t].value;
                    } else { /* CONSTANT */
                        body[b].terms[t].kind = LAPLACE_EXACT_TERM_CONSTANT;
                        const laplace_entity_handle_t eh = find_kernel_entity(
                            entity_map, hdr->entity_count, ar->body[b].terms[t].value);
                        body[b].terms[t].value.constant = eh.id;
                    }
                }
            }

            const laplace_exact_rule_desc_t rule_desc = {
                .head = head,
                .body_literals = body,
                .body_count = ar->body_count
            };

            laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
            laplace_exact_rule_validation_result_t rule_val;
            memset(&rule_val, 0, sizeof(rule_val));

            const laplace_error_t err = laplace_exact_add_rule(
                ctx->store, &rule_desc, &rule_id, &rule_val);

            if (err == LAPLACE_OK &&
                rule_val.error == LAPLACE_EXACT_RULE_VALIDATION_OK) {
                out_result->rules_imported++;
            } else {
                out_result->rules_rejected++;
            }
        }
    }

    if (ctx->exec != NULL && out_result->rules_imported > 0u) {
        (void)laplace_exec_build_trigger_index(ctx->exec);
    }

    out_result->status = LAPLACE_GRAPH_IMPORT_OK;
    return LAPLACE_GRAPH_IMPORT_OK;
}

const char*
laplace_graph_import_status_string(const laplace_graph_import_status_t status)
{
    switch (status) {
        case LAPLACE_GRAPH_IMPORT_OK:                     return "OK";
        case LAPLACE_GRAPH_IMPORT_ERR_NULL:               return "NULL_ARGUMENT";
        case LAPLACE_GRAPH_IMPORT_ERR_VALIDATION_FAILED:  return "VALIDATION_FAILED";
        case LAPLACE_GRAPH_IMPORT_ERR_PREDICATE_CAPACITY: return "PREDICATE_CAPACITY";
        case LAPLACE_GRAPH_IMPORT_ERR_ENTITY_CAPACITY:    return "ENTITY_CAPACITY";
        case LAPLACE_GRAPH_IMPORT_ERR_FACT_CAPACITY:      return "FACT_CAPACITY";
        case LAPLACE_GRAPH_IMPORT_ERR_RULE_CAPACITY:      return "RULE_CAPACITY";
        case LAPLACE_GRAPH_IMPORT_ERR_PROVENANCE_CAPACITY:return "PROVENANCE_CAPACITY";
        case LAPLACE_GRAPH_IMPORT_ERR_INTERNAL:           return "INTERNAL";
        default:                                           return "UNKNOWN";
    }
}
