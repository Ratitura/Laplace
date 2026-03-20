#ifndef LAPLACE_ADAPTER_FACTS_H
#define LAPLACE_ADAPTER_FACTS_H

#include "laplace/adapter.h"
#include "laplace/entity.h"
#include "laplace/exact.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LAPLACE_ADAPTER_FACT_BATCH_MAX = 64u
};

typedef struct laplace_adapter_entity_ref {
    uint32_t id;            /* 0 - 3 */
    uint32_t generation;    /* 4 - 7 */
} laplace_adapter_entity_ref_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_entity_ref_t) == 8u,
                       "laplace_adapter_entity_ref_t must be exactly 8 bytes");
LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_entity_ref_t) == sizeof(laplace_entity_handle_t),
                       "adapter entity ref must match entity handle size");

typedef struct laplace_adapter_fact_request {
    uint32_t abi_version;       /*  0 -  3 */
    uint16_t predicate_id;      /*  4 -  5 */
    uint8_t  arg_count;         /*  6      */
    uint8_t  reserved;          /*  7      */
    uint64_t correlation_id;    /*  8 - 15 */
    laplace_adapter_entity_ref_t args[LAPLACE_EXACT_MAX_ARITY]; /* 16 - 79 */
} laplace_adapter_fact_request_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_fact_request_t) ==
                       16u + LAPLACE_EXACT_MAX_ARITY * sizeof(laplace_adapter_entity_ref_t),
                       "laplace_adapter_fact_request_t size mismatch");

typedef struct laplace_adapter_fact_response {
    laplace_adapter_status_t status;    /*  0 -  3 */
    bool     inserted;                  /*  4       (false if duplicate) */
    uint8_t  reserved[3];              /*  5 -  7 */
    uint32_t fact_row;                  /*  8 - 11 */
    uint32_t entity_id;                 /* 12 - 15 */
    uint32_t generation;                /* 16 - 19 */
    uint32_t provenance_id;             /* 20 - 23 */
    uint64_t correlation_id;            /* 24 - 31 */
} laplace_adapter_fact_response_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_fact_response_t) == 32u,
                       "laplace_adapter_fact_response_t must be exactly 32 bytes");

laplace_adapter_status_t laplace_adapter_validate_fact_request(
    const laplace_exact_store_t* store,
    const laplace_entity_pool_t* entity_pool,
    const laplace_adapter_fact_request_t* request);

laplace_adapter_status_t laplace_adapter_inject_fact(
    laplace_exact_store_t* store,
    const laplace_adapter_fact_request_t* request,
    laplace_adapter_fact_response_t* response);

laplace_adapter_status_t laplace_adapter_inject_facts_batch(
    laplace_exact_store_t* store,
    const laplace_adapter_fact_request_t* requests,
    uint32_t count,
    laplace_adapter_fact_response_t* responses);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_FACTS_H */
