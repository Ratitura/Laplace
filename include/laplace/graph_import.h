#ifndef LAPLACE_GRAPH_IMPORT_H
#define LAPLACE_GRAPH_IMPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/graph_artifact.h"
#include "laplace/exact.h"
#include "laplace/exec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t laplace_graph_import_status_t;

enum {
    LAPLACE_GRAPH_IMPORT_OK                       = 0u,
    LAPLACE_GRAPH_IMPORT_ERR_NULL                  = 1u,
    LAPLACE_GRAPH_IMPORT_ERR_VALIDATION_FAILED     = 2u,
    LAPLACE_GRAPH_IMPORT_ERR_PREDICATE_CAPACITY    = 3u,
    LAPLACE_GRAPH_IMPORT_ERR_ENTITY_CAPACITY       = 4u,
    LAPLACE_GRAPH_IMPORT_ERR_FACT_CAPACITY         = 5u,
    LAPLACE_GRAPH_IMPORT_ERR_RULE_CAPACITY         = 6u,
    LAPLACE_GRAPH_IMPORT_ERR_PROVENANCE_CAPACITY   = 7u,
    LAPLACE_GRAPH_IMPORT_ERR_INTERNAL              = 8u,
    LAPLACE_GRAPH_IMPORT_STATUS_COUNT_             = 9u
};

typedef struct laplace_graph_import_result {
    laplace_graph_import_status_t   status;              /*  0 -  3 */
    uint32_t                        predicates_imported;  /*  4 -  7 */
    uint32_t                        entities_imported;    /*  8 - 11 */
    uint32_t                        facts_imported;       /* 12 - 15 */
    uint32_t                        facts_deduplicated;   /* 16 - 19 */
    uint32_t                        rules_imported;       /* 20 - 23 */
    uint32_t                        rules_rejected;       /* 24 - 27 */
    uint32_t                        failure_record_index; /* 28 - 31 */
    laplace_graph_artifact_status_t validation_status;    /* 32 - 35 */
    uint32_t                        reserved[5];          /* 36 - 55 */
} laplace_graph_import_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_import_result_t) == 56u,
                       "import result must be exactly 56 bytes");

typedef struct laplace_graph_import_context {
    laplace_exact_store_t*    store;
    laplace_entity_pool_t*    entity_pool;
    laplace_exec_context_t*   exec;   /**< optional, may be NULL */
} laplace_graph_import_context_t;

void laplace_graph_import_context_init(
    laplace_graph_import_context_t* ctx,
    laplace_exact_store_t*          store,
    laplace_entity_pool_t*          entity_pool,
    laplace_exec_context_t*         exec);

laplace_graph_import_status_t
laplace_graph_import(
    laplace_graph_import_context_t*  ctx,
    const laplace_graph_artifact_t*  artifact,
    laplace_graph_import_result_t*   out_result);

bool laplace_graph_import_supports_profile(
    laplace_graph_profile_id_t profile_id);

const char*
laplace_graph_import_status_string(laplace_graph_import_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_GRAPH_IMPORT_H */
