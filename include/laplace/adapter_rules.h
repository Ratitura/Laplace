#ifndef LAPLACE_ADAPTER_RULES_H
#define LAPLACE_ADAPTER_RULES_H

#include "laplace/adapter.h"
#include "laplace/exact.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_adapter_term {
    uint32_t kind;      /* 0 - 3 */
    uint32_t value;     /* 4 - 7 */
} laplace_adapter_term_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_term_t) == 8u,
                       "laplace_adapter_term_t must be exactly 8 bytes");

typedef struct laplace_adapter_literal {
    uint16_t predicate_id;      /*  0 -  1 */
    uint8_t  arity;             /*  2      */
    uint8_t  reserved;          /*  3      */
    laplace_adapter_term_t terms[LAPLACE_EXACT_MAX_ARITY]; /*  4 - 67 */
} laplace_adapter_literal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_literal_t) ==
                       4u + LAPLACE_EXACT_MAX_ARITY * sizeof(laplace_adapter_term_t),
                       "laplace_adapter_literal_t size mismatch");

typedef struct laplace_adapter_rule_artifact {
    uint32_t abi_version;       /*   0 -   3 */
    uint32_t body_count;        /*   4 -   7 */
    uint32_t compiler_id;       /*   8 -  11  (optional compiler provenance) */
    uint32_t compiler_version;  /*  12 -  15  (optional compiler provenance) */
    laplace_adapter_literal_t head;
    laplace_adapter_literal_t body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
} laplace_adapter_rule_artifact_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_rule_artifact_t) ==
                       16u + sizeof(laplace_adapter_literal_t) *
                             (1u + LAPLACE_EXACT_MAX_RULE_BODY_LITERALS),
                       "laplace_adapter_rule_artifact_t size mismatch");

typedef struct laplace_adapter_rule_import_result {
    laplace_adapter_status_t status;        /*  0 -  3 */
    uint32_t rule_id;                       /*  4 -  7 */
    uint32_t validation_error;              /*  8 - 11  (laplace_exact_rule_validation_error_t) */
    uint32_t error_literal_index;           /* 12 - 15 */
    uint32_t error_term_index;              /* 16 - 19 */
    uint32_t reserved;                      /* 20 - 23 */
} laplace_adapter_rule_import_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_rule_import_result_t) == 24u,
                       "laplace_adapter_rule_import_result_t must be exactly 24 bytes");

laplace_adapter_status_t laplace_adapter_validate_rule_artifact(
    const laplace_adapter_rule_artifact_t* artifact);

laplace_adapter_status_t laplace_adapter_import_rule(
    laplace_exact_store_t* store,
    const laplace_adapter_rule_artifact_t* artifact,
    laplace_adapter_rule_import_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_RULES_H */
