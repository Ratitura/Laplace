#ifndef LAPLACE_OBSERVE_H
#define LAPLACE_OBSERVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/replay.h"
#include "laplace/trace.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum laplace_observe_level {
    LAPLACE_OBSERVE_OFF    = 0u,   /* no trace emission, counters still update */
    LAPLACE_OBSERVE_ERRORS = 1u,   /* trace only errors and overflow markers */
    LAPLACE_OBSERVE_AUDIT  = 2u,   /* trace derivation-critical events */
    LAPLACE_OBSERVE_DEBUG  = 3u    /* trace all events including steps */
} laplace_observe_level_t;

enum {
    LAPLACE_OBSERVE_MASK_EXACT     = 1u << 0,
    LAPLACE_OBSERVE_MASK_EXEC      = 1u << 1,
    LAPLACE_OBSERVE_MASK_BRANCH    = 1u << 2,
    LAPLACE_OBSERVE_MASK_TRANSPORT = 1u << 3,
    LAPLACE_OBSERVE_MASK_ALL       = 0xFu
};

typedef struct laplace_observe_counters {
    uint64_t exact_facts_asserted;
    uint64_t exact_facts_derived;
    uint64_t exact_facts_duplicated;
    uint64_t exact_rules_accepted;
    uint64_t exact_rules_rejected;
    uint64_t exact_provenance_created;

    uint64_t exec_steps;
    uint64_t exec_rules_fired;
    uint64_t exec_matches_found;
    uint64_t exec_derivations;
    uint64_t exec_duplicates_skipped;
    uint64_t exec_fixpoint_rounds;

    uint64_t branch_creates;
    uint64_t branch_commits;
    uint64_t branch_fails;
    uint64_t branch_epoch_advances;
    uint64_t branch_reclaims;

    uint64_t transport_commands_processed;
    uint64_t transport_commands_failed;
    uint64_t transport_events_emitted;
    uint64_t transport_egress_drops;

    uint64_t trace_records_emitted;
    uint64_t trace_overflow_count;
} laplace_observe_counters_t;

typedef struct laplace_observe_latency_sample {
    uint64_t start_ns;
    uint64_t end_ns;
} laplace_observe_latency_sample_t;

typedef struct laplace_observe_latency_stats {
    uint64_t sample_count;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
} laplace_observe_latency_stats_t;

enum {
    LAPLACE_OBSERVE_LATENCY_EXACT_INSERT   = 0u,
    LAPLACE_OBSERVE_LATENCY_EXEC_STEP      = 1u,
    LAPLACE_OBSERVE_LATENCY_EXEC_RUN       = 2u,
    LAPLACE_OBSERVE_LATENCY_BRANCH_CREATE  = 3u,
    LAPLACE_OBSERVE_LATENCY_BRANCH_COMMIT  = 4u,
    LAPLACE_OBSERVE_LATENCY_TRANSPORT_CMD  = 5u,
    LAPLACE_OBSERVE_LATENCY_COUNT_         = 6u
};

typedef struct laplace_observe_context {
    laplace_trace_buffer_t trace;

    laplace_replay_metadata_t replay;

    laplace_observe_counters_t counters;

    laplace_observe_latency_stats_t latency[LAPLACE_OBSERVE_LATENCY_COUNT_];

    laplace_observe_level_t level;
    uint32_t                subsystem_mask;

    bool initialized;
} laplace_observe_context_t;

laplace_error_t laplace_observe_init(laplace_observe_context_t* ctx,
                                      struct laplace_arena* arena,
                                      laplace_replay_session_id_t session_id);

void laplace_observe_reset(laplace_observe_context_t* ctx);

void laplace_observe_set_level(laplace_observe_context_t* ctx,
                                laplace_observe_level_t level);

laplace_observe_level_t laplace_observe_get_level(const laplace_observe_context_t* ctx);

void laplace_observe_set_mask(laplace_observe_context_t* ctx, uint32_t mask);

uint32_t laplace_observe_get_mask(const laplace_observe_context_t* ctx);

const laplace_observe_counters_t* laplace_observe_get_counters(const laplace_observe_context_t* ctx);

void laplace_observe_reset_counters(laplace_observe_context_t* ctx);

laplace_observe_latency_sample_t laplace_observe_latency_begin(void);

void laplace_observe_latency_end(laplace_observe_context_t* ctx,
                                  uint32_t latency_id,
                                  laplace_observe_latency_sample_t sample);

laplace_observe_latency_stats_t laplace_observe_get_latency(const laplace_observe_context_t* ctx,
                                                             uint32_t latency_id);

bool laplace_observe_should_trace(const laplace_observe_context_t* ctx,
                                   laplace_trace_subsystem_t subsystem,
                                   laplace_trace_kind_t kind);

laplace_trace_seq_t laplace_observe_emit(laplace_observe_context_t* ctx,
                                          const laplace_trace_record_t* record);

void laplace_observe_trace_fact_asserted(laplace_observe_context_t* ctx,
                                          laplace_exact_fact_row_t fact_row,
                                          laplace_predicate_id_t predicate,
                                          laplace_entity_id_t entity_id,
                                          laplace_provenance_id_t provenance_id,
                                          laplace_branch_id_t branch_id,
                                          laplace_branch_generation_t branch_gen,
                                          laplace_epoch_id_t epoch_id,
                                          laplace_tick_t tick);

void laplace_observe_trace_fact_derived(laplace_observe_context_t* ctx,
                                         laplace_exact_fact_row_t fact_row,
                                         laplace_predicate_id_t predicate,
                                         laplace_entity_id_t entity_id,
                                         laplace_provenance_id_t provenance_id,
                                         laplace_rule_id_t rule_id,
                                         laplace_branch_id_t branch_id,
                                         laplace_branch_generation_t branch_gen,
                                         laplace_epoch_id_t epoch_id,
                                         laplace_tick_t tick);

void laplace_observe_trace_fact_duplicate(laplace_observe_context_t* ctx,
                                           laplace_predicate_id_t predicate,
                                           laplace_branch_id_t branch_id,
                                           laplace_branch_generation_t branch_gen,
                                           laplace_tick_t tick);

void laplace_observe_trace_rule_accepted(laplace_observe_context_t* ctx,
                                          laplace_rule_id_t rule_id,
                                          laplace_tick_t tick);

void laplace_observe_trace_rule_rejected(laplace_observe_context_t* ctx,
                                          laplace_rule_id_t rule_id,
                                          uint32_t validation_error,
                                          laplace_tick_t tick);

void laplace_observe_trace_exec_step(laplace_observe_context_t* ctx,
                                      laplace_entity_id_t entity_id,
                                      uint32_t rules_fired,
                                      laplace_branch_id_t branch_id,
                                      laplace_branch_generation_t branch_gen,
                                      laplace_epoch_id_t epoch_id,
                                      laplace_tick_t tick);

void laplace_observe_trace_exec_derivation(laplace_observe_context_t* ctx,
                                            laplace_entity_id_t entity_id,
                                            laplace_rule_id_t rule_id,
                                            laplace_exact_fact_row_t fact_row,
                                            laplace_provenance_id_t provenance_id,
                                            laplace_branch_id_t branch_id,
                                            laplace_branch_generation_t branch_gen,
                                            laplace_tick_t tick);

void laplace_observe_trace_exec_fixpoint(laplace_observe_context_t* ctx,
                                          uint32_t rounds,
                                          laplace_tick_t tick);

void laplace_observe_trace_branch_create(laplace_observe_context_t* ctx,
                                          laplace_branch_id_t branch_id,
                                          laplace_branch_generation_t branch_gen,
                                          laplace_epoch_id_t epoch_id);

void laplace_observe_trace_branch_commit(laplace_observe_context_t* ctx,
                                          laplace_branch_id_t branch_id,
                                          laplace_branch_generation_t branch_gen,
                                          uint32_t promoted_facts,
                                          laplace_epoch_id_t epoch_id);

void laplace_observe_trace_branch_fail(laplace_observe_context_t* ctx,
                                        laplace_branch_id_t branch_id,
                                        laplace_branch_generation_t branch_gen,
                                        laplace_epoch_id_t epoch_id);

void laplace_observe_trace_epoch_advance(laplace_observe_context_t* ctx,
                                          laplace_epoch_id_t new_epoch);

void laplace_observe_trace_transport_cmd(laplace_observe_context_t* ctx,
                                          uint32_t cmd_kind,
                                          uint64_t correlation_id);

void laplace_observe_trace_transport_evt(laplace_observe_context_t* ctx,
                                          uint32_t evt_kind,
                                          uint32_t status,
                                          uint64_t correlation_id);

void laplace_observe_trace_transport_error(laplace_observe_context_t* ctx,
                                            uint32_t cmd_kind,
                                            uint32_t status,
                                            uint64_t correlation_id);

#ifdef __cplusplus
}
#endif

#endif
