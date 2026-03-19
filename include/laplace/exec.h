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

/*
 * Execution systems — deterministic forward-chaining engine.
 *
 * Purpose:
 *   Deterministic exact forward-chaining over the positive Horn/Datalog rule
 *   subset established by the exact symbolic substrate.  The execution engine
 *   processes READY fact entities, matches rule bodies against exact fact
 *   indices, derives new facts with deduplication and provenance, and
 *   transitions entities through deterministic state changes.
 *
 * Truth authority:
 *   The exact symbolic layer remains the sole source of truth.
 *   No approximate or HV-based truth commitment occurs here.
 *
 * Execution model:
 *   Two scheduling modes are provided:
 *     - Dense mode: bitset scan for READY entities (ascending entity ID).
 *     - Sparse mode: bounded ring-buffer queue (ascending insertion order).
 *   Both modes produce deterministic results.  On equivalent workloads the
 *   committed fact sets must match.
 *
 * Provenance policy:
 *   First-proof-wins.  The provenance record attached to a newly derived fact
 *   is from the first successful derivation that inserts it.  Subsequent
 *   duplicate derivations of an already-committed fact are no-ops.
 *
 * Scope limitations:
 *   - No branch/epoch execution semantics.
 *   - No shared-memory ingestion.
 *   - No negation, disjunction, function symbols, or aggregates.
 *   - No approximate truth commitment.
 *   - Rule set is stable during execution.
 *   - No dynamic rule mutation during a run.
 */

enum {
    /*
     * Maximum fact entities that can be enqueued in the sparse ready queue
     * at any given time.  Must be >= LAPLACE_EXACT_MAX_FACTS.
     */
    LAPLACE_EXEC_READY_QUEUE_CAPACITY = 4096u,

    /*
     * Maximum trigger entries across all predicates.
     * Each rule body literal creates one trigger entry for its predicate.
     * Bounded by LAPLACE_EXACT_MAX_RULES * LAPLACE_EXACT_MAX_RULE_BODY_LITERALS.
     */
    LAPLACE_EXEC_MAX_TRIGGER_ENTRIES = LAPLACE_EXACT_MAX_RULES * LAPLACE_EXACT_MAX_RULE_BODY_LITERALS,

    /*
     * Maximum number of new facts that may be derived in a single
     * execution run before the engine pauses.  Acts as a safety budget.
     */
    LAPLACE_EXEC_MAX_DERIVATIONS_PER_RUN = 4096u,

    /*
     * Maximum number of steps (READY entity processings) in a single
     * bounded run before the engine pauses.
     */
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

/*
 * Trigger entry: maps a body predicate to the rule and body-literal position
 * where it appears, enabling frontier-driven rule firing.
 */
typedef struct laplace_exec_trigger_entry {
    laplace_rule_id_t rule_id;
    uint32_t          body_position;
} laplace_exec_trigger_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_exec_trigger_entry_t) == 8u,
                       "trigger entry must be 8 bytes");

/*
 * Execution statistics.  Counters are monotonically increasing within a run
 * and are reset on laplace_exec_reset.
 */
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

/*
 * Execution context: owns all mutable execution state.
 *
 * The context borrows the exact store and entity pool.  It does not own them.
 * All internal arrays (bitset words, queue storage, trigger index) are
 * allocated from the caller-provided arena during init.
 */
typedef struct laplace_exec_context {
    /* Borrowed references (not owned) */
    laplace_exact_store_t*  store;
    laplace_entity_pool_t*  entity_pool;
    laplace_branch_system_t* branch_system;

    /* Scheduler mode */
    laplace_exec_mode_t mode;
    laplace_branch_handle_t active_branch;

    /* Ready tracking — dense bitset over entity pool capacity */
    laplace_bitset_t ready_bitset;

    /* Ready tracking — sparse ring-buffer queue */
    uint32_t* ready_queue;          /* [LAPLACE_EXEC_READY_QUEUE_CAPACITY] entity IDs */
    uint32_t  ready_queue_head;     /* read position */
    uint32_t  ready_queue_tail;     /* write position */
    uint32_t  ready_queue_count;    /* current occupancy */

    /* Trigger index — predicate-to-rule mapping */
    laplace_exec_trigger_entry_t* trigger_entries;   /* [LAPLACE_EXEC_MAX_TRIGGER_ENTRIES] */
    uint32_t* trigger_offsets;      /* [LAPLACE_EXACT_MAX_PREDICATES + 1] */
    uint32_t* trigger_counts;       /* [LAPLACE_EXACT_MAX_PREDICATES + 1] */
    uint32_t  trigger_entry_count;  /* total populated entries */
    bool      trigger_index_built;

    /* Budgets */
    uint32_t max_steps;
    uint32_t max_derivations;

    /* Semi-naive delta frontier tracking.
     *
     * Per-predicate frontier_starts[pred] records the predicate-local row
     * index at which "new" (delta) facts begin for the current fixpoint
     * round.  Facts with predicate-local index < frontier_starts[pred]
     * are "old" and have already been fully joined in prior rounds.
     *
     * This allows the join engine to skip redundant work:
     * - If the anchor fact is new (in the delta), join against ALL facts
     *   in the other body literals (both old and new).
     * - If the anchor fact is old, join against ONLY new (delta) facts
     *   in the other body literals.
     *
     * At the start of each fixpoint round, frontier_starts is snapshot from
     * predicate_row_counts.  Facts derived during the round get indices >=
     * frontier_starts, so they are in the delta.
     *
     * This is only active when semi_naive is true (enabled via API).
     */
    uint32_t* frontier_starts;      /* [LAPLACE_EXACT_MAX_PREDICATES + 1] per-predicate delta boundary */
    uint32_t  frontier_fact_count;   /* total store fact_count at frontier snapshot time (Phase 09 O(1) check) */
    bool semi_naive;                /* enable semi-naive evaluation (delta tracking) */

    /* Statistics */
    laplace_exec_stats_t stats;

    /* Capacity of entity pool at init time (for bitset sizing) */
    uint32_t entity_capacity;

    /* NULL disables tracing */
    laplace_observe_context_t* observe;
} laplace_exec_context_t;

/*
 * Initialize an execution context.
 *
 * Allocates internal arrays (bitset, queue, trigger index) from the arena.
 * The exact store and entity pool are borrowed references.
 * Default mode is DENSE.  Default budgets are the compile-time maximums.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if arena is too small.
 */
laplace_error_t laplace_exec_init(laplace_exec_context_t* ctx,
                                   laplace_arena_t* arena,
                                   laplace_exact_store_t* store,
                                   laplace_entity_pool_t* entity_pool);

/*
 * Reset execution state (stats, ready tracking, trigger index).
 * Does not free arena memory.  The context can be reused after reset.
 */
void laplace_exec_reset(laplace_exec_context_t* ctx);

/*
 * Set the scheduler mode (DENSE or SPARSE).
 * Must be called before a run, or after reset.
 */
void laplace_exec_set_mode(laplace_exec_context_t* ctx, laplace_exec_mode_t mode);

/*
 * Set the per-run step budget.  0 means unlimited (up to compile-time max).
 */
void laplace_exec_set_max_steps(laplace_exec_context_t* ctx, uint32_t max_steps);

/*
 * Set the per-run derivation budget.  0 means unlimited (up to compile-time max).
 */
void laplace_exec_set_max_derivations(laplace_exec_context_t* ctx, uint32_t max_derivations);

/*
 * Enable or disable semi-naive evaluation.
 *
 * When enabled, the fixpoint loop tracks per-predicate delta frontiers.
 * Join operations skip redundant old × old combinations that were already
 * explored in prior rounds.  This can significantly reduce work for
 * multi-body recursive rules (e.g., transitive closure).
 *
 * Default: disabled (false).
 * Must be called before laplace_exec_run.
 */
void laplace_exec_set_semi_naive(laplace_exec_context_t* ctx, bool enabled);

void laplace_exec_bind_branch_system(laplace_exec_context_t* ctx, laplace_branch_system_t* branch_system);

laplace_error_t laplace_exec_set_active_branch(laplace_exec_context_t* ctx, laplace_branch_handle_t branch);

laplace_branch_handle_t laplace_exec_get_active_branch(const laplace_exec_context_t* ctx);

/*
 * Build the body-predicate-to-rule trigger index from the current rule set
 * in the exact store.  Must be called after all rules are added and before
 * execution begins.
 *
 * The trigger index is deterministic: entries for each predicate are ordered
 * by (rule_id ascending, body_position ascending).
 */
laplace_error_t laplace_exec_build_trigger_index(laplace_exec_context_t* ctx);

/*
 * Mark a fact entity as READY for execution processing.
 *
 * The entity must be alive, have exact role FACT, and be in READY state.
 * In sparse mode, the entity is enqueued into the ready queue.
 * In dense mode, the entity's bit is set in the ready bitset.
 * In both modes, both the bitset and queue are maintained for consistency.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_ARGUMENT if entity is invalid.
 * Returns LAPLACE_ERR_INVALID_STATE if entity is not a READY-state fact.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if sparse queue is full.
 */
laplace_error_t laplace_exec_mark_ready(laplace_exec_context_t* ctx,
                                         laplace_entity_id_t entity_id);

/*
 * Mark all currently asserted facts in the exact store as READY.
 * Convenience API for initial execution setup.
 *
 * Only marks fact entities that are currently in READY entity state.
 * Returns the number of entities marked.
 */
uint32_t laplace_exec_mark_all_facts_ready(laplace_exec_context_t* ctx);

/*
 * Query the number of READY entities currently tracked.
 */
uint32_t laplace_exec_ready_count(const laplace_exec_context_t* ctx);

/*
 * Execute a single step: process one READY fact entity.
 *
 * In dense mode, processes the lowest-ID READY entity.
 * In sparse mode, processes the entity at the head of the ready queue.
 *
 * The entity is matched against all triggered rules.  New derived facts
 * are inserted with dedup and provenance.  Newly derived facts that are
 * unique are themselves marked READY.
 *
 * The processed entity transitions from READY to ACTIVE.
 *
 * Returns LAPLACE_OK if a step was executed.
 * Returns LAPLACE_ERR_INVALID_STATE if no READY entities exist.
 */
laplace_error_t laplace_exec_step(laplace_exec_context_t* ctx);

/*
 * Run execution until fixpoint or budget exhaustion.
 *
 * Fixpoint is reached when no READY entities remain.
 * Budget is exhausted when max_steps or max_derivations is reached.
 *
 * Returns the run status indicating why execution stopped.
 */
laplace_exec_run_status_t laplace_exec_run(laplace_exec_context_t* ctx);

const laplace_exec_stats_t* laplace_exec_get_stats(const laplace_exec_context_t* ctx);

laplace_exec_mode_t laplace_exec_get_mode(const laplace_exec_context_t* ctx);

void laplace_exec_bind_observe(laplace_exec_context_t* ctx,
                               laplace_observe_context_t* observe);

#ifdef __cplusplus
}
#endif

#endif
