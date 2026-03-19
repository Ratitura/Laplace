#ifndef LAPLACE_BRANCH_H
#define LAPLACE_BRANCH_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/epoch.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_exact_store laplace_exact_store_t;

typedef struct laplace_observe_context laplace_observe_context_t;

enum {
    LAPLACE_BRANCH_MAX_BRANCHES = 32u,
    LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH = 2048u,
    LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH = 4096u
};

typedef enum laplace_branch_status {
    LAPLACE_BRANCH_STATUS_INVALID = 0u,
    LAPLACE_BRANCH_STATUS_ACTIVE = 1u,
    LAPLACE_BRANCH_STATUS_COMMITTED = 2u,
    LAPLACE_BRANCH_STATUS_FAILED = 3u,
    LAPLACE_BRANCH_STATUS_RETIRED = 4u
} laplace_branch_status_t;

typedef struct laplace_branch_info {
    laplace_branch_handle_t parent;
    laplace_branch_status_t status;
    laplace_epoch_id_t      create_epoch;
    laplace_epoch_id_t      close_epoch;
    uint32_t                owned_fact_count;
    uint32_t                owned_entity_count;
} laplace_branch_info_t;

typedef struct laplace_branch_system {
    laplace_exact_store_t*  store;
    laplace_entity_pool_t*  entity_pool;
    laplace_epoch_id_t      current_epoch;

    /* NULL disables tracing */
    laplace_observe_context_t* observe;

    laplace_branch_generation_t* generations;
    uint8_t*                     statuses;
    laplace_branch_id_t*         parent_ids;
    laplace_branch_generation_t* parent_generations;
    laplace_epoch_id_t*          create_epochs;
    laplace_epoch_id_t*          close_epochs;
    uint32_t*                    owned_fact_counts;
    uint32_t*                    owned_entity_counts;

    laplace_exact_fact_row_t* owned_fact_rows;
    laplace_entity_handle_t*  owned_fact_promotions;
    laplace_entity_handle_t*  owned_entities;
    laplace_entity_handle_t*  owned_entity_promotions;
} laplace_branch_system_t;

laplace_error_t laplace_branch_system_init(laplace_branch_system_t* system,
                                            laplace_arena_t* arena,
                                            laplace_exact_store_t* store,
                                            laplace_entity_pool_t* entity_pool);

laplace_branch_handle_t laplace_branch_create(laplace_branch_system_t* system,
                                               laplace_branch_handle_t parent_branch,
                                               laplace_error_t* out_error);

bool laplace_branch_is_active(const laplace_branch_system_t* system,
                               laplace_branch_handle_t branch);

laplace_error_t laplace_branch_get_info(const laplace_branch_system_t* system,
                                         laplace_branch_handle_t branch,
                                         laplace_branch_info_t* out_info);

laplace_epoch_id_t laplace_branch_current_epoch(const laplace_branch_system_t* system);

laplace_error_t laplace_branch_advance_epoch(laplace_branch_system_t* system,
                                              laplace_epoch_id_t* out_epoch);

laplace_error_t laplace_branch_register_entity(laplace_branch_system_t* system,
                                                laplace_branch_handle_t branch,
                                                laplace_entity_handle_t entity);

laplace_error_t laplace_branch_register_constant(laplace_branch_system_t* system,
                                                  laplace_branch_handle_t branch,
                                                  laplace_entity_handle_t constant,
                                                  laplace_exact_type_id_t type_id,
                                                  uint32_t flags);

laplace_error_t laplace_branch_insert_asserted_provenance(laplace_branch_system_t* system,
                                                           laplace_branch_handle_t branch,
                                                           laplace_provenance_id_t* out_provenance_id);

laplace_error_t laplace_branch_assert_fact(laplace_branch_system_t* system,
                                            laplace_branch_handle_t branch,
                                            laplace_predicate_id_t predicate_id,
                                            const laplace_entity_handle_t* args,
                                            uint32_t arg_count,
                                            laplace_provenance_id_t provenance_id,
                                            uint32_t flags,
                                            laplace_exact_fact_row_t* out_fact_row,
                                            laplace_entity_handle_t* out_fact_entity,
                                            bool* out_inserted);

laplace_error_t laplace_branch_fail(laplace_branch_system_t* system,
                                     laplace_branch_handle_t branch);

laplace_error_t laplace_branch_commit(laplace_branch_system_t* system,
                                       laplace_branch_handle_t branch,
                                       uint32_t* out_promoted_facts,
                                       uint32_t* out_deduplicated_facts);

laplace_error_t laplace_branch_reclaim_closed(laplace_branch_system_t* system,
                                               uint32_t* out_reclaimed_branches,
                                               uint32_t* out_reclaimed_entities);

void laplace_branch_bind_observe(laplace_branch_system_t* system,
                                 laplace_observe_context_t* observe);

#ifdef __cplusplus
}
#endif

#endif