#ifndef LAPLACE_ADAPTER_H
#define LAPLACE_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAPLACE_ADAPTER_ABI_VERSION 1u

enum {
    LAPLACE_ADAPTER_ARTIFACT_RULE   = 1u << 0,
    LAPLACE_ADAPTER_ARTIFACT_HV     = 1u << 1,
    LAPLACE_ADAPTER_ARTIFACT_FACT   = 1u << 2,
    LAPLACE_ADAPTER_ARTIFACT_VERIFY = 1u << 3
};

typedef enum laplace_adapter_status {
    LAPLACE_ADAPTER_OK                      = 0,
    LAPLACE_ADAPTER_ERR_NULL_ARGUMENT       = 1,
    LAPLACE_ADAPTER_ERR_INVALID_VERSION     = 2,
    LAPLACE_ADAPTER_ERR_INVALID_FORMAT      = 3,
    LAPLACE_ADAPTER_ERR_VALIDATION_FAILED   = 4,
    LAPLACE_ADAPTER_ERR_CAPACITY_EXHAUSTED  = 5,
    LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH  = 6,
    LAPLACE_ADAPTER_ERR_PREDICATE_INVALID   = 7,
    LAPLACE_ADAPTER_ERR_ARITY_MISMATCH      = 8,
    LAPLACE_ADAPTER_ERR_ENTITY_INVALID      = 9,
    LAPLACE_ADAPTER_ERR_UNSUPPORTED_QUERY   = 10,
    LAPLACE_ADAPTER_ERR_NOT_FOUND           = 11,
    LAPLACE_ADAPTER_ERR_INTERNAL            = 12,
    LAPLACE_ADAPTER_ERR_DUPLICATE           = 13,
    LAPLACE_ADAPTER_ERR_BATCH_OVERFLOW      = 14
} laplace_adapter_status_t;

typedef struct laplace_adapter_capability {
    uint32_t abi_version;               /*  0 -  3 */
    uint32_t kernel_version_major;      /*  4 -  7 */
    uint32_t kernel_version_minor;      /*  8 - 11 */
    uint32_t kernel_version_patch;      /* 12 - 15 */
    uint32_t hv_dimension;              /* 16 - 19 */
    uint32_t hv_words;                  /* 20 - 23 */
    uint32_t max_predicates;            /* 24 - 27 */
    uint32_t max_facts;                 /* 28 - 31 */
    uint32_t max_rules;                 /* 32 - 35 */
    uint32_t max_arity;                 /* 36 - 39 */
    uint32_t max_rule_body_literals;    /* 40 - 43 */
    uint32_t max_rule_variables;        /* 44 - 47 */
    uint32_t max_provenance_parents;    /* 48 - 51 */
    uint32_t max_branches;              /* 52 - 55 */
    uint32_t supported_artifact_kinds;  /* 56 - 59 */
    uint32_t reserved;                  /* 60 - 63 */
} laplace_adapter_capability_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_capability_t) == 64u,
                       "laplace_adapter_capability_t must be exactly 64 bytes (1 cache line)");

void laplace_adapter_query_capability(laplace_adapter_capability_t* out);

bool laplace_adapter_check_version(uint32_t abi_version);

const char* laplace_adapter_status_string(laplace_adapter_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_H */
