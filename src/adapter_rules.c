#include "laplace/adapter_rules.h"

#include <string.h>

static laplace_adapter_status_t validate_adapter_term(
    const laplace_adapter_term_t* const term)
{
    if (term->kind != (uint32_t)LAPLACE_EXACT_TERM_VARIABLE &&
        term->kind != (uint32_t)LAPLACE_EXACT_TERM_CONSTANT) {
        return LAPLACE_ADAPTER_ERR_INVALID_FORMAT;
    }

    if (term->kind == (uint32_t)LAPLACE_EXACT_TERM_VARIABLE &&
        (term->value == 0 || term->value > (uint32_t)UINT16_MAX)) {
        return LAPLACE_ADAPTER_ERR_INVALID_FORMAT;
    }

    return LAPLACE_ADAPTER_OK;
}

static laplace_adapter_status_t validate_adapter_literal(
    const laplace_adapter_literal_t* const lit)
{
    if (lit->arity > LAPLACE_EXACT_MAX_ARITY) {
        return LAPLACE_ADAPTER_ERR_ARITY_MISMATCH;
    }

    for (uint32_t i = 0; i < lit->arity; ++i) {
        const laplace_adapter_status_t s = validate_adapter_term(&lit->terms[i]);
        if (s != LAPLACE_ADAPTER_OK) { return s; }
    }

    return LAPLACE_ADAPTER_OK;
}

static void convert_term(laplace_exact_term_t* const dst,
                          const laplace_adapter_term_t* const src)
{
    dst->kind = (laplace_exact_term_kind_t)src->kind;
    if (src->kind == (uint32_t)LAPLACE_EXACT_TERM_VARIABLE) {
        dst->value.variable = (laplace_exact_var_id_t)src->value;
    } else {
        dst->value.constant = (laplace_entity_id_t)src->value;
    }
}

static void convert_literal(laplace_exact_literal_t* const dst,
                             const laplace_adapter_literal_t* const src)
{
    dst->predicate = (laplace_predicate_id_t)src->predicate_id;
    dst->arity     = src->arity;

    for (uint32_t i = 0; i < src->arity; ++i) {
        convert_term(&dst->terms[i], &src->terms[i]);
    }
    for (uint32_t i = src->arity; i < LAPLACE_EXACT_MAX_ARITY; ++i) {
        memset(&dst->terms[i], 0, sizeof(dst->terms[i]));
    }
}

laplace_adapter_status_t laplace_adapter_validate_rule_artifact(
    const laplace_adapter_rule_artifact_t* const artifact)
{
    if (artifact == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    if (artifact->abi_version != LAPLACE_ADAPTER_ABI_VERSION) {
        return LAPLACE_ADAPTER_ERR_INVALID_VERSION;
    }

    if (artifact->body_count == 0 ||
        artifact->body_count > LAPLACE_EXACT_MAX_RULE_BODY_LITERALS) {
        return LAPLACE_ADAPTER_ERR_INVALID_FORMAT;
    }

    laplace_adapter_status_t s = validate_adapter_literal(&artifact->head);
    if (s != LAPLACE_ADAPTER_OK) { return s; }

    for (uint32_t i = 0; i < artifact->body_count; ++i) {
        s = validate_adapter_literal(&artifact->body[i]);
        if (s != LAPLACE_ADAPTER_OK) { return s; }
    }

    return LAPLACE_ADAPTER_OK;
}

laplace_adapter_status_t laplace_adapter_import_rule(
    laplace_exact_store_t* const store,
    const laplace_adapter_rule_artifact_t* const artifact,
    laplace_adapter_rule_import_result_t* const result)
{
    if (result == NULL) {
        return LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));

    if (store == NULL || artifact == NULL) {
        result->status = LAPLACE_ADAPTER_ERR_NULL_ARGUMENT;
        return result->status;
    }

    const laplace_adapter_status_t fmt_status =
        laplace_adapter_validate_rule_artifact(artifact);
    if (fmt_status != LAPLACE_ADAPTER_OK) {
        result->status = fmt_status;
        return result->status;
    }

    laplace_exact_literal_t head;
    laplace_exact_literal_t body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];

    convert_literal(&head, &artifact->head);
    for (uint32_t i = 0; i < artifact->body_count; ++i) {
        convert_literal(&body[i], &artifact->body[i]);
    }

    laplace_exact_rule_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.head          = head;
    desc.body_literals = body;
    desc.body_count    = artifact->body_count;

    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t validation;
    memset(&validation, 0, sizeof(validation));

    const laplace_error_t err = laplace_exact_add_rule(
        store, &desc, &rule_id, &validation);

    if (err != LAPLACE_OK) {
        if (validation.error != LAPLACE_EXACT_RULE_VALIDATION_OK) {
            result->status              = LAPLACE_ADAPTER_ERR_VALIDATION_FAILED;
            result->validation_error    = (uint32_t)validation.error;
            result->error_literal_index = validation.literal_index;
            result->error_term_index    = validation.term_index;
        } else if (err == LAPLACE_ERR_CAPACITY_EXHAUSTED) {
            result->status = LAPLACE_ADAPTER_ERR_CAPACITY_EXHAUSTED;
        } else {
            result->status = LAPLACE_ADAPTER_ERR_INTERNAL;
        }
        return result->status;
    }

    result->status  = LAPLACE_ADAPTER_OK;
    result->rule_id = (uint32_t)rule_id;
    return LAPLACE_ADAPTER_OK;
}
