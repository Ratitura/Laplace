#include "laplace/adapter_verify.h"

#include <string.h>

laplace_adapter_status_t laplace_adapter_verify_fact_exists(
    const laplace_exact_store_t* const store,
    const laplace_adapter_verify_fact_query_t* const query,
    laplace_adapter_verify_fact_result_t* const result)
{
    if (result == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));

    if (store == NULL || query == NULL) {
        result->status         = LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
        result->correlation_id = (query != NULL) ? query->correlation_id : 0;
        return result->status;
    }

    result->correlation_id = query->correlation_id;

    if (query->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        result->status = LAPLACE_ADAPTER_ERR_INVALID_VERSION;
        return result->status;
    }

    const laplace_predicate_id_t pred =
        (laplace_predicate_id_t)query->predicate_id;
    if (!laplace_exact_predicate_is_declared(store, pred)) {
        result->status = LAPLACE_ADAPTER_ERR_PREDICATE_INVALID;
        return result->status;
    }

    const uint8_t declared_arity = laplace_exact_predicate_arity(store, pred);
    if (query->arg_count != declared_arity) {
        result->status = LAPLACE_ADAPTER_ERR_ARITY_MISMATCH;
        return result->status;
    }

    laplace_entity_id_t args[LAPLACE_EXACT_MAX_ARITY];
    for (uint32_t i = 0; i < query->arg_count; ++i) {
        args[i] = (laplace_entity_id_t)query->args[i];
    }

    const laplace_exact_fact_row_t row = laplace_exact_find_fact(
        store, pred, args, query->arg_count);

    if (row == LAPLACE_EXACT_FACT_ROW_INVALID) {
        result->status = LAPLACE_ADAPTER_OK;
        result->found  = false;
        return LAPLACE_ADAPTER_OK;
    }

    const laplace_exact_fact_t* const fact = laplace_exact_get_fact(store, row);
    if (fact == NULL) {
        result->status = LAPLACE_ADAPTER_ERR_INTERNAL;
        return result->status;
    }

    result->status        = LAPLACE_ADAPTER_OK;
    result->found         = true;
    result->fact_row      = (uint32_t)row;
    result->fact_flags    = fact->flags;
    result->provenance_id = (uint32_t)fact->provenance;
    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_verify_provenance(
    const laplace_exact_store_t* const store,
    const laplace_adapter_verify_provenance_query_t* const query,
    laplace_adapter_verify_provenance_result_t* const result)
{
    if (result == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));

    if (store == NULL || query == NULL) {
        result->status         = LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
        result->correlation_id = (query != NULL) ? query->correlation_id : 0;
        return result->status;
    }

    result->correlation_id = query->correlation_id;

    if (query->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        result->status = LAPLACE_ADAPTER_ERR_INVALID_VERSION;
        return result->status;
    }

    const laplace_provenance_id_t prov_id =
        (laplace_provenance_id_t)query->provenance_id;
    const laplace_exact_provenance_record_t* const prov =
        laplace_exact_get_provenance(store, prov_id);

    if (prov == NULL) {
        result->status = LAPLACE_ADAPTER_OK;
        result->found  = false;
        return LAPLACE_ADAPTER_OK;
    }

    result->status         = LAPLACE_ADAPTER_OK;
    result->found          = true;
    result->kind           = (uint8_t)prov->kind;
    result->source_rule_id = (uint32_t)prov->source_rule_id;
    result->tick           = (uint64_t)prov->tick;

    uint32_t parent_count = 0;
    const laplace_entity_id_t* const parents =
        laplace_exact_get_provenance_parents(store, prov_id, &parent_count);

    if (parent_count > LAPLACE_EXACT_MAX_PROVENANCE_PARENTS) {
        parent_count = LAPLACE_EXACT_MAX_PROVENANCE_PARENTS;
    }
    result->parent_count = parent_count;

    if (parents != NULL) {
        for (uint32_t i = 0; i < parent_count; ++i) {
            result->parent_entity_ids[i] = (uint32_t)parents[i];
        }
    }

    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_verify_rule(
    const laplace_exact_store_t* const store,
    const laplace_adapter_verify_rule_query_t* const query,
    laplace_adapter_verify_rule_result_t* const result)
{
    if (result == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));

    if (store == NULL || query == NULL) {
        result->status         = LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
        result->correlation_id = (query != NULL) ? query->correlation_id : 0;
        return result->status;
    }

    result->correlation_id = query->correlation_id;

    if (query->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        result->status = LAPLACE_ADAPTER_ERR_INVALID_VERSION;
        return result->status;
    }

    const laplace_rule_id_t rule_id = (laplace_rule_id_t)query->rule_id;
    const laplace_exact_rule_t* const rule =
        laplace_exact_get_rule(store, rule_id);

    if (rule == NULL) {
        result->status = LAPLACE_ADAPTER_OK;
        result->found  = false;
        return LAPLACE_ADAPTER_OK;
    }

    result->status            = LAPLACE_ADAPTER_OK;
    result->found             = true;
    result->rule_status       = (uint8_t)rule->status;
    result->rule_id           = (uint32_t)rule->id;
    result->head_predicate_id = (uint16_t)rule->head.predicate;
    result->head_arity        = rule->head.arity;
    result->body_count        = (uint8_t)rule->body_count;
    return LAPLACE_ADAPTER_OK;
}
