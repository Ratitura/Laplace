#include "laplace/observe.h"

#include <string.h>

#include "laplace/arena.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static uint64_t observe_now_ns(void) {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    const uint64_t ticks = (uint64_t)counter.QuadPart;
    const uint64_t hz    = (uint64_t)frequency.QuadPart;
    return (ticks / hz) * 1000000000ULL + ((ticks % hz) * 1000000000ULL) / hz;
}
#else
#include <time.h>
static uint64_t observe_now_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

laplace_error_t laplace_observe_init(laplace_observe_context_t* const ctx,
                                      laplace_arena_t* const arena,
                                      const laplace_replay_session_id_t session_id) {
    if (ctx == NULL || arena == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(ctx, 0, sizeof(*ctx));

    const laplace_error_t trace_err = laplace_trace_buffer_init(&ctx->trace, arena);
    if (trace_err != LAPLACE_OK) {
        return trace_err;
    }

    const laplace_error_t replay_err = laplace_replay_init(&ctx->replay, session_id);
    if (replay_err != LAPLACE_OK) {
        return replay_err;
    }

    memset(&ctx->counters, 0, sizeof(ctx->counters));

    for (uint32_t i = 0u; i < LAPLACE_OBSERVE_LATENCY_COUNT_; ++i) {
        ctx->latency[i].sample_count = 0u;
        ctx->latency[i].total_ns     = 0u;
        ctx->latency[i].min_ns       = UINT64_MAX;
        ctx->latency[i].max_ns       = 0u;
    }

    ctx->level          = LAPLACE_OBSERVE_AUDIT;
    ctx->subsystem_mask = LAPLACE_OBSERVE_MASK_ALL;
    ctx->initialized    = true;

    return LAPLACE_OK;
}

void laplace_observe_reset(laplace_observe_context_t* const ctx) {
    if (ctx == NULL) {
        return;
    }

    laplace_trace_buffer_reset(&ctx->trace);
    memset(&ctx->counters, 0, sizeof(ctx->counters));

    for (uint32_t i = 0u; i < LAPLACE_OBSERVE_LATENCY_COUNT_; ++i) {
        ctx->latency[i].sample_count = 0u;
        ctx->latency[i].total_ns     = 0u;
        ctx->latency[i].min_ns       = UINT64_MAX;
        ctx->latency[i].max_ns       = 0u;
    }
}

void laplace_observe_set_level(laplace_observe_context_t* const ctx,
                                const laplace_observe_level_t level) {
    LAPLACE_ASSERT(ctx != NULL);
    ctx->level = level;
}

laplace_observe_level_t laplace_observe_get_level(const laplace_observe_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    return ctx->level;
}

void laplace_observe_set_mask(laplace_observe_context_t* const ctx, const uint32_t mask) {
    LAPLACE_ASSERT(ctx != NULL);
    ctx->subsystem_mask = mask;
}

uint32_t laplace_observe_get_mask(const laplace_observe_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    return ctx->subsystem_mask;
}

const laplace_observe_counters_t* laplace_observe_get_counters(const laplace_observe_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    return &ctx->counters;
}

void laplace_observe_reset_counters(laplace_observe_context_t* const ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    memset(&ctx->counters, 0, sizeof(ctx->counters));
}

laplace_observe_latency_sample_t laplace_observe_latency_begin(void) {
    laplace_observe_latency_sample_t sample;
    sample.start_ns = observe_now_ns();
    sample.end_ns   = 0u;
    return sample;
}

void laplace_observe_latency_end(laplace_observe_context_t* const ctx,
                                  const uint32_t latency_id,
                                  laplace_observe_latency_sample_t sample) {
    LAPLACE_ASSERT(ctx != NULL);
    if (latency_id >= LAPLACE_OBSERVE_LATENCY_COUNT_) {
        return;
    }

    sample.end_ns = observe_now_ns();
    const uint64_t elapsed = sample.end_ns - sample.start_ns;

    laplace_observe_latency_stats_t* const stats = &ctx->latency[latency_id];
    stats->sample_count += 1u;
    stats->total_ns     += elapsed;
    if (elapsed < stats->min_ns) {
        stats->min_ns = elapsed;
    }
    if (elapsed > stats->max_ns) {
        stats->max_ns = elapsed;
    }
}

laplace_observe_latency_stats_t laplace_observe_get_latency(const laplace_observe_context_t* const ctx,
                                                             const uint32_t latency_id) {
    LAPLACE_ASSERT(ctx != NULL);
    laplace_observe_latency_stats_t empty;
    memset(&empty, 0, sizeof(empty));

    if (latency_id >= LAPLACE_OBSERVE_LATENCY_COUNT_) {
        return empty;
    }
    return ctx->latency[latency_id];
}

static laplace_observe_level_t trace_kind_min_level(const laplace_trace_kind_t kind) {
    switch (kind) {
        case LAPLACE_TRACE_KIND_OVERFLOW_MARKER:
        case LAPLACE_TRACE_KIND_TRANSPORT_ERROR:
            return LAPLACE_OBSERVE_ERRORS;

        case LAPLACE_TRACE_KIND_FACT_ASSERTED:
        case LAPLACE_TRACE_KIND_FACT_DERIVED:
        case LAPLACE_TRACE_KIND_FACT_DUPLICATE:
        case LAPLACE_TRACE_KIND_RULE_ACCEPTED:
        case LAPLACE_TRACE_KIND_RULE_REJECTED:
        case LAPLACE_TRACE_KIND_EXEC_DERIVATION:
        case LAPLACE_TRACE_KIND_EXEC_FIXPOINT:
        case LAPLACE_TRACE_KIND_BRANCH_CREATE:
        case LAPLACE_TRACE_KIND_BRANCH_COMMIT:
        case LAPLACE_TRACE_KIND_BRANCH_FAIL:
        case LAPLACE_TRACE_KIND_EPOCH_ADVANCE:
        case LAPLACE_TRACE_KIND_TRANSPORT_CMD:
        case LAPLACE_TRACE_KIND_TRANSPORT_EVT:
        case LAPLACE_TRACE_KIND_COUNTER_SNAPSHOT:
            return LAPLACE_OBSERVE_AUDIT;

        case LAPLACE_TRACE_KIND_EXEC_STEP:
            return LAPLACE_OBSERVE_DEBUG;

        default:
            return LAPLACE_OBSERVE_DEBUG;
    }
}

static uint32_t subsystem_mask_bit(const laplace_trace_subsystem_t subsystem) {
    switch (subsystem) {
        case LAPLACE_TRACE_SUBSYSTEM_EXACT:     return LAPLACE_OBSERVE_MASK_EXACT;
        case LAPLACE_TRACE_SUBSYSTEM_EXEC:      return LAPLACE_OBSERVE_MASK_EXEC;
        case LAPLACE_TRACE_SUBSYSTEM_BRANCH:    return LAPLACE_OBSERVE_MASK_BRANCH;
        case LAPLACE_TRACE_SUBSYSTEM_TRANSPORT: return LAPLACE_OBSERVE_MASK_TRANSPORT;
        default:                                return LAPLACE_OBSERVE_MASK_ALL;
    }
}

bool laplace_observe_should_trace(const laplace_observe_context_t* const ctx,
                                   const laplace_trace_subsystem_t subsystem,
                                   const laplace_trace_kind_t kind) {
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    if (ctx->level == LAPLACE_OBSERVE_OFF) {
        return false;
    }
    if ((ctx->subsystem_mask & subsystem_mask_bit(subsystem)) == 0u) {
        return false;
    }
    return (uint32_t)ctx->level >= (uint32_t)trace_kind_min_level(kind);
}

laplace_trace_seq_t laplace_observe_emit(laplace_observe_context_t* const ctx,
                                          const laplace_trace_record_t* const record) {
    LAPLACE_ASSERT(ctx != NULL && record != NULL);

    if (!laplace_observe_should_trace(ctx,
                                       (laplace_trace_subsystem_t)record->subsystem,
                                       (laplace_trace_kind_t)record->kind)) {
        return 0u;
    }

    const laplace_trace_seq_t seq = laplace_trace_emit(&ctx->trace, record);
    ctx->counters.trace_records_emitted += 1u;
    ctx->counters.trace_overflow_count = laplace_trace_overflow_count(&ctx->trace);

    return seq;
}

static void observe_build_record(laplace_trace_record_t* const rec,
                                  const laplace_trace_kind_t kind,
                                  const laplace_trace_subsystem_t subsystem) {
    memset(rec, 0, sizeof(*rec));
    rec->kind      = (uint16_t)kind;
    rec->subsystem = (uint8_t)subsystem;
}

void laplace_observe_trace_fact_asserted(laplace_observe_context_t* const ctx,
                                          const laplace_exact_fact_row_t fact_row,
                                          const laplace_predicate_id_t predicate,
                                          const laplace_entity_id_t entity_id,
                                          const laplace_provenance_id_t provenance_id,
                                          const laplace_branch_id_t branch_id,
                                          const laplace_branch_generation_t branch_gen,
                                          const laplace_epoch_id_t epoch_id,
                                          const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exact_facts_asserted += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXACT,
                                       LAPLACE_TRACE_KIND_FACT_ASSERTED)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_FACT_ASSERTED, LAPLACE_TRACE_SUBSYSTEM_EXACT);
    rec.entity_id     = entity_id;
    rec.provenance_id = provenance_id;
    rec.branch_id     = branch_id;
    rec.branch_gen    = branch_gen;
    rec.epoch_id      = epoch_id;
    rec.tick           = tick;
    rec.payload.fact.fact_row  = fact_row;
    rec.payload.fact.predicate = predicate;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_fact_derived(laplace_observe_context_t* const ctx,
                                         const laplace_exact_fact_row_t fact_row,
                                         const laplace_predicate_id_t predicate,
                                         const laplace_entity_id_t entity_id,
                                         const laplace_provenance_id_t provenance_id,
                                         const laplace_rule_id_t rule_id,
                                         const laplace_branch_id_t branch_id,
                                         const laplace_branch_generation_t branch_gen,
                                         const laplace_epoch_id_t epoch_id,
                                         const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exact_facts_derived += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXACT,
                                       LAPLACE_TRACE_KIND_FACT_DERIVED)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_FACT_DERIVED, LAPLACE_TRACE_SUBSYSTEM_EXACT);
    rec.entity_id     = entity_id;
    rec.provenance_id = provenance_id;
    rec.rule_id       = rule_id;
    rec.branch_id     = branch_id;
    rec.branch_gen    = branch_gen;
    rec.epoch_id      = epoch_id;
    rec.tick           = tick;
    rec.payload.fact.fact_row  = fact_row;
    rec.payload.fact.predicate = predicate;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_fact_duplicate(laplace_observe_context_t* const ctx,
                                           const laplace_predicate_id_t predicate,
                                           const laplace_branch_id_t branch_id,
                                           const laplace_branch_generation_t branch_gen,
                                           const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exact_facts_duplicated += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXACT,
                                       LAPLACE_TRACE_KIND_FACT_DUPLICATE)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_FACT_DUPLICATE, LAPLACE_TRACE_SUBSYSTEM_EXACT);
    rec.branch_id  = branch_id;
    rec.branch_gen = branch_gen;
    rec.tick        = tick;
    rec.payload.fact.predicate = predicate;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_rule_accepted(laplace_observe_context_t* const ctx,
                                          const laplace_rule_id_t rule_id,
                                          const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exact_rules_accepted += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXACT,
                                       LAPLACE_TRACE_KIND_RULE_ACCEPTED)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_RULE_ACCEPTED, LAPLACE_TRACE_SUBSYSTEM_EXACT);
    rec.rule_id = rule_id;
    rec.tick     = tick;
    rec.payload.rule.rule_id = rule_id;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_rule_rejected(laplace_observe_context_t* const ctx,
                                          const laplace_rule_id_t rule_id,
                                          const uint32_t validation_error,
                                          const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exact_rules_rejected += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXACT,
                                       LAPLACE_TRACE_KIND_RULE_REJECTED)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_RULE_REJECTED, LAPLACE_TRACE_SUBSYSTEM_EXACT);
    rec.rule_id = rule_id;
    rec.tick     = tick;
    rec.payload.rule.rule_id          = rule_id;
    rec.payload.rule.validation_error = validation_error;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_exec_step(laplace_observe_context_t* const ctx,
                                      const laplace_entity_id_t entity_id,
                                      const uint32_t rules_fired,
                                      const laplace_branch_id_t branch_id,
                                      const laplace_branch_generation_t branch_gen,
                                      const laplace_epoch_id_t epoch_id,
                                      const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exec_steps += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXEC,
                                       LAPLACE_TRACE_KIND_EXEC_STEP)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_EXEC_STEP, LAPLACE_TRACE_SUBSYSTEM_EXEC);
    rec.entity_id  = entity_id;
    rec.branch_id  = branch_id;
    rec.branch_gen = branch_gen;
    rec.epoch_id   = epoch_id;
    rec.tick        = tick;
    rec.payload.exec.entity_id   = entity_id;
    rec.payload.exec.rules_fired = rules_fired;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_exec_derivation(laplace_observe_context_t* const ctx,
                                            const laplace_entity_id_t entity_id,
                                            const laplace_rule_id_t rule_id,
                                            const laplace_exact_fact_row_t fact_row,
                                            const laplace_provenance_id_t provenance_id,
                                            const laplace_branch_id_t branch_id,
                                            const laplace_branch_generation_t branch_gen,
                                            const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exec_derivations += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXEC,
                                       LAPLACE_TRACE_KIND_EXEC_DERIVATION)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_EXEC_DERIVATION, LAPLACE_TRACE_SUBSYSTEM_EXEC);
    rec.entity_id     = entity_id;
    rec.rule_id       = rule_id;
    rec.provenance_id = provenance_id;
    rec.branch_id     = branch_id;
    rec.branch_gen    = branch_gen;
    rec.tick           = tick;
    rec.payload.fact.fact_row  = fact_row;
    rec.payload.fact.predicate = 0u; /* predicate available from fact_row lookup */
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_exec_fixpoint(laplace_observe_context_t* const ctx,
                                          const uint32_t rounds,
                                          const laplace_tick_t tick) {
    if (ctx == NULL) { return; }
    ctx->counters.exec_fixpoint_rounds += (uint64_t)rounds;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_EXEC,
                                       LAPLACE_TRACE_KIND_EXEC_FIXPOINT)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_EXEC_FIXPOINT, LAPLACE_TRACE_SUBSYSTEM_EXEC);
    rec.tick = tick;
    rec.payload.exec.rules_fired = rounds;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_branch_create(laplace_observe_context_t* const ctx,
                                          const laplace_branch_id_t branch_id,
                                          const laplace_branch_generation_t branch_gen,
                                          const laplace_epoch_id_t epoch_id) {
    if (ctx == NULL) { return; }
    ctx->counters.branch_creates += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_BRANCH,
                                       LAPLACE_TRACE_KIND_BRANCH_CREATE)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_BRANCH_CREATE, LAPLACE_TRACE_SUBSYSTEM_BRANCH);
    rec.branch_id  = branch_id;
    rec.branch_gen = branch_gen;
    rec.epoch_id   = epoch_id;
    rec.payload.branch.branch_id  = branch_id;
    rec.payload.branch.branch_gen = branch_gen;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_branch_commit(laplace_observe_context_t* const ctx,
                                          const laplace_branch_id_t branch_id,
                                          const laplace_branch_generation_t branch_gen,
                                          const uint32_t promoted_facts,
                                          const laplace_epoch_id_t epoch_id) {
    if (ctx == NULL) { return; }
    ctx->counters.branch_commits += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_BRANCH,
                                       LAPLACE_TRACE_KIND_BRANCH_COMMIT)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_BRANCH_COMMIT, LAPLACE_TRACE_SUBSYSTEM_BRANCH);
    rec.branch_id  = branch_id;
    rec.branch_gen = branch_gen;
    rec.epoch_id   = epoch_id;
    rec.payload.branch.branch_id  = branch_id;
    rec.payload.branch.branch_gen = branch_gen;
    rec.payload.branch.detail     = promoted_facts;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_branch_fail(laplace_observe_context_t* const ctx,
                                        const laplace_branch_id_t branch_id,
                                        const laplace_branch_generation_t branch_gen,
                                        const laplace_epoch_id_t epoch_id) {
    if (ctx == NULL) { return; }
    ctx->counters.branch_fails += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_BRANCH,
                                       LAPLACE_TRACE_KIND_BRANCH_FAIL)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_BRANCH_FAIL, LAPLACE_TRACE_SUBSYSTEM_BRANCH);
    rec.branch_id  = branch_id;
    rec.branch_gen = branch_gen;
    rec.epoch_id   = epoch_id;
    rec.payload.branch.branch_id  = branch_id;
    rec.payload.branch.branch_gen = branch_gen;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_epoch_advance(laplace_observe_context_t* const ctx,
                                          const laplace_epoch_id_t new_epoch) {
    if (ctx == NULL) { return; }
    ctx->counters.branch_epoch_advances += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_BRANCH,
                                       LAPLACE_TRACE_KIND_EPOCH_ADVANCE)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_EPOCH_ADVANCE, LAPLACE_TRACE_SUBSYSTEM_BRANCH);
    rec.epoch_id = new_epoch;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_transport_cmd(laplace_observe_context_t* const ctx,
                                          const uint32_t cmd_kind,
                                          const uint64_t correlation_id) {
    if (ctx == NULL) { return; }
    ctx->counters.transport_commands_processed += 1u;

    laplace_replay_update_transport_correlation(&ctx->replay, correlation_id);

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT,
                                       LAPLACE_TRACE_KIND_TRANSPORT_CMD)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_TRANSPORT_CMD, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT);
    rec.correlation_id = correlation_id;
    rec.payload.transport.cmd_or_evt_kind = cmd_kind;
    rec.payload.transport.correlation_id  = correlation_id;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_transport_evt(laplace_observe_context_t* const ctx,
                                          const uint32_t evt_kind,
                                          const uint32_t status,
                                          const uint64_t correlation_id) {
    if (ctx == NULL) { return; }
    ctx->counters.transport_events_emitted += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT,
                                       LAPLACE_TRACE_KIND_TRANSPORT_EVT)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_TRANSPORT_EVT, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT);
    rec.correlation_id = correlation_id;
    rec.payload.transport.cmd_or_evt_kind = evt_kind;
    rec.payload.transport.status          = status;
    rec.payload.transport.correlation_id  = correlation_id;
    laplace_observe_emit(ctx, &rec);
}

void laplace_observe_trace_transport_error(laplace_observe_context_t* const ctx,
                                            const uint32_t cmd_kind,
                                            const uint32_t status,
                                            const uint64_t correlation_id) {
    if (ctx == NULL) { return; }
    ctx->counters.transport_commands_failed += 1u;

    if (!laplace_observe_should_trace(ctx, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT,
                                       LAPLACE_TRACE_KIND_TRANSPORT_ERROR)) {
        return;
    }

    laplace_trace_record_t rec;
    observe_build_record(&rec, LAPLACE_TRACE_KIND_TRANSPORT_ERROR, LAPLACE_TRACE_SUBSYSTEM_TRANSPORT);
    rec.correlation_id = correlation_id;
    rec.payload.transport.cmd_or_evt_kind = cmd_kind;
    rec.payload.transport.status          = status;
    rec.payload.transport.correlation_id  = correlation_id;
    laplace_observe_emit(ctx, &rec);
}
