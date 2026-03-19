#ifndef LAPLACE_ADAPTER_VERIFY_H
#define LAPLACE_ADAPTER_VERIFY_H

#include "laplace/adapter.h"
#include "laplace/exact.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_adapter_verify_fact_query {
    uint32_t abi_version;       /*  0 -  3 */
    uint16_t predicate_id;      /*  4 -  5 */
    uint8_t  arg_count;         /*  6      */
    uint8_t  reserved;          /*  7      */
    uint64_t correlation_id;    /*  8 - 15 */
    uint32_t args[LAPLACE_EXACT_MAX_ARITY]; /* 16 - 47 */
} laplace_adapter_verify_fact_query_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_fact_query_t) ==
                       16u + LAPLACE_EXACT_MAX_ARITY * sizeof(uint32_t),
                       "laplace_adapter_verify_fact_query_t size mismatch");

typedef struct laplace_adapter_verify_fact_result {
    laplace_adapter_status_t status;    /*  0 -  3 */
    bool     found;                     /*  4      */
    uint8_t  reserved[3];              /*  5 -  7 */
    uint32_t fact_row;                  /*  8 - 11 */
    uint32_t fact_flags;                /* 12 - 15 */
    uint32_t provenance_id;             /* 16 - 19 */
    uint32_t reserved2;                 /* 20 - 23  (explicit padding) */
    uint64_t correlation_id;            /* 24 - 31 */
} laplace_adapter_verify_fact_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_fact_result_t) == 32u,
                       "laplace_adapter_verify_fact_result_t must be exactly 32 bytes");

typedef struct laplace_adapter_verify_provenance_query {
    uint32_t abi_version;       /*  0 -  3 */
    uint32_t provenance_id;     /*  4 -  7 */
    uint64_t correlation_id;    /*  8 - 15 */
} laplace_adapter_verify_provenance_query_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_provenance_query_t) == 16u,
                       "laplace_adapter_verify_provenance_query_t must be exactly 16 bytes");

typedef struct laplace_adapter_verify_provenance_result {
    laplace_adapter_status_t status;    /*  0 -  3 */
    bool     found;                     /*  4      */
    uint8_t  kind;                      /*  5       (laplace_exact_provenance_kind_t) */
    uint8_t  reserved[2];              /*  6 -  7 */
    uint32_t source_rule_id;            /*  8 - 11 */
    uint32_t parent_count;              /* 12 - 15 */
    uint64_t tick;                      /* 16 - 23 */
    uint64_t correlation_id;            /* 24 - 31 */
    uint32_t parent_entity_ids[LAPLACE_EXACT_MAX_PROVENANCE_PARENTS]; /* 32 - 63 */
} laplace_adapter_verify_provenance_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_provenance_result_t) == 64u,
                       "laplace_adapter_verify_provenance_result_t must be exactly 64 bytes (1 cache line)");

typedef struct laplace_adapter_verify_rule_query {
    uint32_t abi_version;       /*  0 -  3 */
    uint32_t rule_id;           /*  4 -  7 */
    uint64_t correlation_id;    /*  8 - 15 */
} laplace_adapter_verify_rule_query_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_rule_query_t) == 16u,
                       "laplace_adapter_verify_rule_query_t must be exactly 16 bytes");

typedef struct laplace_adapter_verify_rule_result {
    laplace_adapter_status_t status;    /*  0 -  3 */
    bool     found;                     /*  4      */
    uint8_t  rule_status;               /*  5       (laplace_exact_rule_status_t) */
    uint8_t  head_arity;                /*  6      */
    uint8_t  body_count;                /*  7      */
    uint32_t rule_id;                   /*  8 - 11 */
    uint16_t head_predicate_id;         /* 12 - 13 */
    uint8_t  reserved[2];              /* 14 - 15 */
    uint64_t correlation_id;            /* 16 - 23 */
} laplace_adapter_verify_rule_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_verify_rule_result_t) == 24u,
                       "laplace_adapter_verify_rule_result_t must be exactly 24 bytes");

/*
 * Query whether a specific fact exists in the committed store.
 * This is a read-only lookup.  Uses entity IDs (not handles) for
 * the argument matching since only existence is being checked.
 */
laplace_adapter_status_t laplace_adapter_verify_fact_exists(
    const laplace_exact_store_t* store,
    const laplace_adapter_verify_fact_query_t* query,
    laplace_adapter_verify_fact_result_t* result);

/*
 * Query provenance information for a committed provenance record.
 * Returns the provenance kind, source rule, parent fact entities,
 * and derivation tick.
 */
laplace_adapter_status_t laplace_adapter_verify_provenance(
    const laplace_exact_store_t* store,
    const laplace_adapter_verify_provenance_query_t* query,
    laplace_adapter_verify_provenance_result_t* result);

/*
 * Query the status of a registered rule.
 * Returns whether the rule exists, its validation status, head
 * predicate, arity, and body count.
 */
laplace_adapter_status_t laplace_adapter_verify_rule(
    const laplace_exact_store_t* store,
    const laplace_adapter_verify_rule_query_t* query,
    laplace_adapter_verify_rule_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_VERIFY_H */
