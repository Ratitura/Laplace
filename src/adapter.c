#include "laplace/adapter.h"

#include "laplace/branch.h"
#include "laplace/exact.h"
#include "laplace/hv.h"
#include "laplace/version.h"

#include <string.h>

void laplace_adapter_query_capability(laplace_adapter_capability_t* const out) {
    LAPLACE_ASSERT(out != NULL);
    if (out == NULL) { return; }

    memset(out, 0, sizeof(*out));
    out->abi_version              = LAPLACE_ADAPTER_ABI_VERSION;
    out->kernel_version_major     = LAPLACE_VERSION_MAJOR;
    out->kernel_version_minor     = LAPLACE_VERSION_MINOR;
    out->kernel_version_patch     = LAPLACE_VERSION_PATCH;
    out->hv_dimension             = LAPLACE_HV_DIM;
    out->hv_words                 = LAPLACE_HV_WORDS;
    out->max_predicates           = LAPLACE_EXACT_MAX_PREDICATES;
    out->max_facts                = LAPLACE_EXACT_MAX_FACTS;
    out->max_rules                = LAPLACE_EXACT_MAX_RULES;
    out->max_arity                = LAPLACE_EXACT_MAX_ARITY;
    out->max_rule_body_literals   = LAPLACE_EXACT_MAX_RULE_BODY_LITERALS;
    out->max_rule_variables       = LAPLACE_EXACT_MAX_RULE_VARIABLES;
    out->max_provenance_parents   = LAPLACE_EXACT_MAX_PROVENANCE_PARENTS;
    out->max_branches             = LAPLACE_BRANCH_MAX_BRANCHES;
    out->supported_artifact_kinds = LAPLACE_ADAPTER_ARTIFACT_RULE
                                  | LAPLACE_ADAPTER_ARTIFACT_HV
                                  | LAPLACE_ADAPTER_ARTIFACT_FACT
                                  | LAPLACE_ADAPTER_ARTIFACT_VERIFY;
}

bool laplace_adapter_check_version(const uint32_t abi_version) {
    return abi_version == LAPLACE_ADAPTER_ABI_VERSION;
}

const char* laplace_adapter_status_string(const laplace_adapter_status_t status) {
    switch (status) {
        case LAPLACE_ADAPTER_OK:                      return "OK";
        case LAPLACE_ADAPTER_ERR_NULL_ARGUMENT:       return "NULL_ARGUMENT";
        case LAPLACE_ADAPTER_ERR_INVALID_VERSION:     return "INVALID_VERSION";
        case LAPLACE_ADAPTER_ERR_INVALID_FORMAT:      return "INVALID_FORMAT";
        case LAPLACE_ADAPTER_ERR_VALIDATION_FAILED:   return "VALIDATION_FAILED";
        case LAPLACE_ADAPTER_ERR_CAPACITY_EXHAUSTED:  return "CAPACITY_EXHAUSTED";
        case LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH:  return "DIMENSION_MISMATCH";
        case LAPLACE_ADAPTER_ERR_PREDICATE_INVALID:   return "PREDICATE_INVALID";
        case LAPLACE_ADAPTER_ERR_ARITY_MISMATCH:      return "ARITY_MISMATCH";
        case LAPLACE_ADAPTER_ERR_ENTITY_INVALID:      return "ENTITY_INVALID";
        case LAPLACE_ADAPTER_ERR_UNSUPPORTED_QUERY:   return "UNSUPPORTED_QUERY";
        case LAPLACE_ADAPTER_ERR_NOT_FOUND:           return "NOT_FOUND";
        case LAPLACE_ADAPTER_ERR_INTERNAL:            return "INTERNAL";
        case LAPLACE_ADAPTER_ERR_DUPLICATE:           return "DUPLICATE";
        case LAPLACE_ADAPTER_ERR_BATCH_OVERFLOW:      return "BATCH_OVERFLOW";
        default:                                      return "UNKNOWN";
    }
}
