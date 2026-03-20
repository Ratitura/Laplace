#ifndef LAPLACE_EXEC_H
#define LAPLACE_EXEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/bitset.h"
#include "laplace/branch.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/types.h"

typedef struct laplace_observe_context laplace_observe_context_t;

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LAPLACE_EXEC_READY_QUEUE_CAPACITY = 4096u,

    LAPLACE_EXEC_MAX_TRIGGER_ENTRIES = LAPLACE_EXACT_MAX_RULES * LAPLACE_EXACT_MAX_RULE_BODY_LITERALS,

    LAPLACE_EXEC_MAX_DERIVATIONS_PER_RUN = 4096u,

    LAPLACE_EXEC_MAX_STEPS_PER_RUN = 8192u
};

LAPLACE_STATIC_ASSERT(LAPLACE_EXEC_READY_QUEUE_CAPACITY >= LAPLACE_EXACT_MAX_FACTS,
                       "ready queue must hold at least max facts");
LAPLACE_STATIC_ASSERT(LAPLACE_EXEC_MAX_TRIGGER_ENTRIES > 0u,
                       "trigger entry count must be non-zero");

typedef enum laplace_exec_mode {
    LAPLACE_EXEC_MODE_DENSE  = 0u,
    LAPLACE_EXEC_MODE_SPARSE = 1u
} laplace_exec_mode_t;

typedef enum laplace_exec_run_status {
    LAPLACE_EXEC_RUN_FIXPOINT    = 0u,  /* no READY entities remain */
    LAPLACE_EXEC_RUN_BUDGET      = 1u,  /* step or derivation budget exhausted */
    LAPLACE_EXEC_RUN_ERROR       = 2u   /* execution error encountered */
} laplace_exec_run_status_t;

typedef struct laplace_exec_trigger_entry {
    laplace_rule_id_t rule_id;
    uint32_t          body_position;
} laplace_exec_trigger_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_exec_trigger_entry_t) == 8u,
                       "trigger entry must be 8 bytes");

typedef struct laplace_exec_stats {
    uint64_t steps_executed;        /* number of READY entities processed */
    uint64_t rules_fired;           /* number of rule-body match attempts */
    uint64_t matches_found;         /* number of successful full-body matches */
    uint64_t facts_derived;         /* number of new facts inserted */
    uint64_t facts_deduplicated;    /* number of duplicate derivations skipped */
    uint64_t provenance_created;    /* number of provenance records created */
    uint64_t delta_joins_skipped;   /* join candidates skipped by semi-naive delta filtering */
    uint32_t fixpoint_rounds;       /* number of semi-naive rounds to reach fixpoint */
} laplace_exec_stats_t;

typedef struct laplace_exec_context {
    laplace_exact_store_t*  store;
    laplace_entity_pool_t*  entity_pool;
    laplace_branch_system_t* branch_system;

    laplace_exec_mode_t mode;
    laplace_branch_handle_t active_branch;

    laplace_bitset_t ready_bitset;

    uint32_t* ready_queue;          /* [LAPLACE_EXEC_READY_QUEUE_CAPACITY] entity IDs */
    uint32_t  ready_queue_head;     /* read position */
    uint32_t  ready_queue_tail;     /* write position */
    uint32_t  ready_queue_count;    /* current occupancy */

    laplace_exec_trigger_entry_t* trigger_entries;   /* [LAPLACE_EXEC_MAX_TRIGGER_ENTRIES] */
    uint32_t* trigger_offsets;      /* [LAPLACE_EXACT_MAX_PREDICATES + 1] */
    uint32_t* trigger_counts;       /* [LAPLACE_EXACT_MAX_PREDICATES + 1] */
    uint32_t  trigger_entry_count;  /* total populated entries */
    bool      trigger_index_built;

    uint32_t max_steps;
    uint32_t max_derivations;

    uint32_t* frontier_starts;      /* [LAPLACE_EXACT_MAX_PREDICATES + 1] per-predicate delta boundary */
    uint32_t  frontier_fact_count;   /* total store fact_count at frontier snapshot time (Phase 09 O(1) check) */
    bool semi_naive;                /* enable semi-naive evaluation (delta tracking) */

    laplace_exec_stats_t stats;

    uint32_t entity_capacity;

    laplace_observe_context_t* observe;
} laplace_exec_context_t;

laplace_error_t laplace_exec_init(laplace_exec_context_t* ctx,
                                   laplace_arena_t* arena,
                                   laplace_exact_store_t* store,
                                   laplace_entity_pool_t* entity_pool);

void laplace_exec_reset(laplace_exec_context_t* ctx);

void laplace_exec_set_mode(laplace_exec_context_t* ctx, laplace_exec_mode_t mode);

void laplace_exec_set_max_steps(laplace_exec_context_t* ctx, uint32_t max_steps);

void laplace_exec_set_max_derivations(laplace_exec_context_t* ctx, uint32_t max_derivations);

void laplace_exec_set_semi_naive(laplace_exec_context_t* ctx, bool enabled);

void laplace_exec_bind_branch_system(laplace_exec_context_t* ctx, laplace_branch_system_t* branch_system);

laplace_error_t laplace_exec_set_active_branch(laplace_exec_context_t* ctx, laplace_branch_handle_t branch);

laplace_branch_handle_t laplace_exec_get_active_branch(const laplace_exec_context_t* ctx);

laplace_error_t laplace_exec_build_trigger_index(laplace_exec_context_t* ctx);

laplace_error_t laplace_exec_mark_ready(laplace_exec_context_t* ctx,
                                         laplace_entity_id_t entity_id);

uint32_t laplace_exec_mark_all_facts_ready(laplace_exec_context_t* ctx);

uint32_t laplace_exec_ready_count(const laplace_exec_context_t* ctx);

laplace_error_t laplace_exec_step(laplace_exec_context_t* ctx);

laplace_exec_run_status_t laplace_exec_run(laplace_exec_context_t* ctx);

const laplace_exec_stats_t* laplace_exec_get_stats(const laplace_exec_context_t* ctx);

laplace_exec_mode_t laplace_exec_get_mode(const laplace_exec_context_t* ctx);

void laplace_exec_bind_observe(laplace_exec_context_t* ctx,
                               laplace_observe_context_t* observe);

#ifdef __cplusplus
}
#endif

#endif
