#include "laplace/adapter_facts.h"

#include <string.h>

static laplace_entity_handle_t ref_to_handle(
    const laplace_adapter_entity_ref_t* const ref)
{
    const laplace_entity_handle_t h = {
        .id         = (laplace_entity_id_t)ref->id,
        .generation = (laplace_generation_t)ref->generation
    };
    return h;
}

laplace_adapter_status_t laplace_adapter_validate_fact_request(
    const laplace_exact_store_t* const store,
    const laplace_entity_pool_t* const entity_pool,
    const laplace_adapter_fact_request_t* const request)
{
    if (store == NULL || entity_pool == NULL || request == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    if (request->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        return LAPLACE_ADAPTER_ERR_INVALID_VERSION;
    }

    const laplace_predicate_id_t pred =
        (laplace_predicate_id_t)request->predicate_id;
    if (!laplace_exact_predicate_is_declared(store, pred)) {
        return LAPLACE_ADAPTER_ERR_PREDICATE_INVALID;
    }

    const uint8_t declared_arity = laplace_exact_predicate_arity(store, pred);
    if (request->arg_count != declared_arity) {
        return LAPLACE_ADAPTER_ERR_ARITY_MISMATCH;
    }

    for (uint32_t i = 0; i < request->arg_count; ++i) {
        const laplace_entity_handle_t h = ref_to_handle(&request->args[i]);
        if (!laplace_entity_pool_is_alive(entity_pool, h)) {
            return LAPLACE_ADAPTER_ERR_ENTITY_INVALID;
        }
    }

    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_inject_fact(
    laplace_exact_store_t* const store,
    const laplace_adapter_fact_request_t* const request,
    laplace_adapter_fact_response_t* const response)
{
    if (response == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    memset(response, 0, sizeof(*response));

    if (store == NULL || request == NULL) {
        response->status         = LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
        response->correlation_id = (request != NULL) ? request->correlation_id : 0;
        return response->status;
    }

    response->correlation_id = request->correlation_id;

    const laplace_adapter_status_t val_status =
        laplace_adapter_validate_fact_request(store, store->entity_pool, request);
    if (val_status != LAPLACE_ADAPTER_OK) {
        response->status = val_status;
        return response->status;
    }

    laplace_exact_provenance_desc_t prov_desc;
    memset(&prov_desc, 0, sizeof(prov_desc));
    prov_desc.kind           = LAPLACE_EXACT_PROVENANCE_ASSERTED;
    prov_desc.source_rule_id = LAPLACE_RULE_ID_INVALID;
    prov_desc.parent_facts   = NULL;
    prov_desc.parent_count   = 0;

    laplace_provenance_id_t prov_id = LAPLACE_PROVENANCE_ID_INVALID;
    laplace_error_t err = laplace_exact_insert_provenance(
        store, &prov_desc, &prov_id);
    if (err != LAPLACE_OK) {
        response->status = (err == LAPLACE_ERR_CAPACITY_EXHAUSTED)
                         ? LAPLACE_ADAPTER_ERR_CAPACITY_EXHAUSTED
                         : LAPLACE_ADAPTER_ERR_INTERNAL;
        return response->status;
    }

    const uint32_t arity = request->arg_count;
    laplace_entity_handle_t handles[LAPLACE_EXACT_MAX_ARITY];
    for (uint32_t i = 0; i < arity; ++i) {
        handles[i] = ref_to_handle(&request->args[i]);
    }

    const uint32_t fact_flags =
        LAPLACE_EXACT_FACT_FLAG_ASSERTED | LAPLACE_EXACT_FACT_FLAG_COMMITTED;

    laplace_exact_fact_row_t  fact_row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t   fact_entity = {0, 0};
    bool                      inserted = false;

    err = laplace_exact_assert_fact(
        store,
        (laplace_predicate_id_t)request->predicate_id,
        handles,
        arity,
        prov_id,
        fact_flags,
        &fact_row,
        &fact_entity,
        &inserted);

    if (err != LAPLACE_OK) {
        response->status = (err == LAPLACE_ERR_CAPACITY_EXHAUSTED)
                         ? LAPLACE_ADAPTER_ERR_CAPACITY_EXHAUSTED
                         : LAPLACE_ADAPTER_ERR_INTERNAL;
        return response->status;
    }

    response->status        = LAPLACE_ADAPTER_OK;
    response->inserted      = inserted;
    response->fact_row      = (uint32_t)fact_row;
    response->entity_id     = (uint32_t)fact_entity.id;
    response->generation    = (uint32_t)fact_entity.generation;
    response->provenance_id = (uint32_t)prov_id;
    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_inject_facts_batch(
    laplace_exact_store_t* const store,
    const laplace_adapter_fact_request_t* const requests,
    const uint32_t count,
    laplace_adapter_fact_response_t* const responses)
{
    if (store == NULL || requests == NULL || responses == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    if (count == 0) {
        return LAPLACE_ADAPTER_OK;
    }

    if (count > LAPLACE_ADAPTER_FACT_BATCH_MAX) {
        return LAPLACE_ADAPTER_ERR_BATCH_OVERFLOW;
    }

    for (uint32_t i = 0; i < count; ++i) {
        (void)laplace_adapter_inject_fact(store, &requests[i], &responses[i]);
    }

    return LAPLACE_ADAPTER_OK;
}
