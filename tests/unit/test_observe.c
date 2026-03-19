#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/align.h"
#include "laplace/arena.h"
#include "laplace/observe.h"
#include "test_harness.h"

#define TEST_ARENA_SIZE (512u * 1024u)

static int test_observe_init(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    const laplace_error_t rc = laplace_observe_init(&ctx, &arena, 1u);
    LAPLACE_TEST_ASSERT(rc == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(ctx.initialized == true);
    LAPLACE_TEST_ASSERT(ctx.level == LAPLACE_OBSERVE_AUDIT);
    LAPLACE_TEST_ASSERT(ctx.subsystem_mask == LAPLACE_OBSERVE_MASK_ALL);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.exec_steps == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.branch_creates == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.transport_commands_processed == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.trace_records_emitted == 0u);
    return 0;
}

static int test_observe_init_null(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    LAPLACE_TEST_ASSERT(laplace_observe_init(NULL, &arena, 1u) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_observe_context_t ctx;
    LAPLACE_TEST_ASSERT(laplace_observe_init(&ctx, NULL, 1u) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_observe_set_level(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_DEBUG);
    LAPLACE_TEST_ASSERT(laplace_observe_get_level(&ctx) == LAPLACE_OBSERVE_DEBUG);

    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_OFF);
    LAPLACE_TEST_ASSERT(laplace_observe_get_level(&ctx) == LAPLACE_OBSERVE_OFF);

    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_ERRORS);
    LAPLACE_TEST_ASSERT(laplace_observe_get_level(&ctx) == LAPLACE_OBSERVE_ERRORS);
    return 0;
}

static int test_observe_set_mask(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    laplace_observe_set_mask(&ctx, LAPLACE_OBSERVE_MASK_EXACT | LAPLACE_OBSERVE_MASK_BRANCH);
    LAPLACE_TEST_ASSERT(laplace_observe_get_mask(&ctx) == (LAPLACE_OBSERVE_MASK_EXACT | LAPLACE_OBSERVE_MASK_BRANCH));

    laplace_observe_set_mask(&ctx, 0u);
    LAPLACE_TEST_ASSERT(laplace_observe_get_mask(&ctx) == 0u);
    return 0;
}

static int test_observe_counters_always_update(void) {
    /*
     * Critical invariant: counters update regardless of trace level.
     * Even at OBSERVE_OFF, convenience helpers must increment counters.
     */
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_OFF);

    /* Emit events at OFF level */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    laplace_observe_trace_fact_derived(&ctx, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 0u, 2u);
    laplace_observe_trace_fact_duplicate(&ctx, 3u, 0u, 0u, 3u);
    laplace_observe_trace_rule_accepted(&ctx, 0u, 4u);
    laplace_observe_trace_rule_rejected(&ctx, 1u, 99u, 5u);
    laplace_observe_trace_exec_step(&ctx, 20u, 3u, 0u, 0u, 0u, 6u);
    laplace_observe_trace_exec_derivation(&ctx, 21u, 0u, 2u, 2u, 0u, 0u, 7u);
    laplace_observe_trace_exec_fixpoint(&ctx, 2u, 8u);
    laplace_observe_trace_branch_create(&ctx, 1u, 0u, 1u);
    laplace_observe_trace_branch_commit(&ctx, 1u, 0u, 5u, 1u);
    laplace_observe_trace_branch_fail(&ctx, 2u, 0u, 2u);
    laplace_observe_trace_epoch_advance(&ctx, 3u);
    laplace_observe_trace_transport_cmd(&ctx, 1u, 100u);
    laplace_observe_trace_transport_evt(&ctx, 2u, 0u, 100u);
    laplace_observe_trace_transport_error(&ctx, 3u, 1u, 200u);

    /* Counters must have been updated despite OFF level */
    const laplace_observe_counters_t* c = laplace_observe_get_counters(&ctx);
    LAPLACE_TEST_ASSERT(c->exact_facts_asserted == 1u);
    LAPLACE_TEST_ASSERT(c->exact_facts_derived == 1u);
    LAPLACE_TEST_ASSERT(c->exact_facts_duplicated == 1u);
    LAPLACE_TEST_ASSERT(c->exact_rules_accepted == 1u);
    LAPLACE_TEST_ASSERT(c->exact_rules_rejected == 1u);
    LAPLACE_TEST_ASSERT(c->exec_steps == 1u);
    LAPLACE_TEST_ASSERT(c->exec_derivations == 1u);
    LAPLACE_TEST_ASSERT(c->branch_creates == 1u);
    LAPLACE_TEST_ASSERT(c->branch_commits == 1u);
    LAPLACE_TEST_ASSERT(c->branch_fails == 1u);
    LAPLACE_TEST_ASSERT(c->branch_epoch_advances == 1u);
    LAPLACE_TEST_ASSERT(c->transport_commands_processed == 1u);
    LAPLACE_TEST_ASSERT(c->transport_events_emitted == 1u);
    LAPLACE_TEST_ASSERT(c->transport_commands_failed == 1u);

    /* But no trace records should have been emitted at OFF */
    LAPLACE_TEST_ASSERT(c->trace_records_emitted == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 0u);
    return 0;
}

static int test_observe_level_gating(void) {
    /*
     * Verify that AUDIT level emits derivation-critical events but
     * suppresses DEBUG-only events (exec_step).
     */
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_AUDIT);

    /* AUDIT-level event: should be emitted */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 1u);

    /* DEBUG-level event: should be suppressed */
    laplace_observe_trace_exec_step(&ctx, 20u, 3u, 0u, 0u, 0u, 2u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 1u); /* still 1 */

    /* But counter was still updated */
    LAPLACE_TEST_ASSERT(ctx.counters.exec_steps == 1u);

    /* At DEBUG level, exec_step should be emitted */
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_DEBUG);
    laplace_observe_trace_exec_step(&ctx, 21u, 2u, 0u, 0u, 0u, 3u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 2u);
    LAPLACE_TEST_ASSERT(ctx.counters.exec_steps == 2u);
    return 0;
}

static int test_observe_subsystem_mask(void) {
    /*
     * Verify that subsystem mask filters trace emission
     * while counters still update.
     */
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_AUDIT);

    /* Disable EXACT subsystem */
    laplace_observe_set_mask(&ctx, LAPLACE_OBSERVE_MASK_ALL & ~LAPLACE_OBSERVE_MASK_EXACT);

    /* EXACT event should be suppressed */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 1u); /* counter still updated */

    /* BRANCH event should be emitted (BRANCH is still enabled) */
    laplace_observe_trace_branch_create(&ctx, 1u, 0u, 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 1u);
    LAPLACE_TEST_ASSERT(ctx.counters.branch_creates == 1u);
    return 0;
}

static int test_observe_branch_epoch_tagging(void) {
    /*
     * Verify that branch/epoch fields in trace records are correctly
     * populated by convenience helpers.
     */
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_AUDIT);

    /* Emit a fact_asserted with specific branch/epoch */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u,
        3u,   /* branch_id */
        7u,   /* branch_gen */
        5u,   /* epoch_id */
        42u); /* tick */

    const laplace_trace_record_t* rec = laplace_trace_get(&ctx.trace, 0u);
    LAPLACE_TEST_ASSERT(rec != NULL);
    LAPLACE_TEST_ASSERT(rec->branch_id == 3u);
    LAPLACE_TEST_ASSERT(rec->branch_gen == 7u);
    LAPLACE_TEST_ASSERT(rec->epoch_id == 5u);
    LAPLACE_TEST_ASSERT(rec->tick == 42u);
    LAPLACE_TEST_ASSERT(rec->entity_id == 10u);
    LAPLACE_TEST_ASSERT(rec->kind == (uint16_t)LAPLACE_TRACE_KIND_FACT_ASSERTED);
    return 0;
}

static int test_observe_reset(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    /* Emit some events */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    laplace_observe_trace_rule_accepted(&ctx, 0u, 2u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 1u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_rules_accepted == 1u);

    /* Reset */
    laplace_observe_reset(&ctx);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_rules_accepted == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 0u);
    return 0;
}

static int test_observe_latency_basic(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    /* Take a latency sample */
    laplace_observe_latency_sample_t sample = laplace_observe_latency_begin();
    /* Do some trivial work to burn time */
    volatile uint64_t sum = 0u;
    for (uint32_t i = 0u; i < 1000u; ++i) {
        sum += i;
    }
    (void)sum;
    laplace_observe_latency_end(&ctx, LAPLACE_OBSERVE_LATENCY_EXACT_INSERT, sample);

    laplace_observe_latency_stats_t stats = laplace_observe_get_latency(&ctx,
        LAPLACE_OBSERVE_LATENCY_EXACT_INSERT);
    LAPLACE_TEST_ASSERT(stats.sample_count == 1u);
    LAPLACE_TEST_ASSERT(stats.total_ns > 0u);
    LAPLACE_TEST_ASSERT(stats.min_ns > 0u);
    LAPLACE_TEST_ASSERT(stats.max_ns >= stats.min_ns);
    LAPLACE_TEST_ASSERT(stats.max_ns == stats.min_ns); /* single sample */
    return 0;
}

static int test_observe_counter_reset(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 1u);

    laplace_observe_reset_counters(&ctx);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 0u);
    /* Trace records are NOT cleared by counter reset */
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) > 0u);
    return 0;
}

static int test_observe_errors_level(void) {
    /*
     * At ERRORS level, only error/overflow events should be traced.
     * Transport errors should still be traced.
     * Normal derivation events should NOT be traced.
     */
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);
    laplace_observe_set_level(&ctx, LAPLACE_OBSERVE_ERRORS);

    /* Normal audit event — should be suppressed */
    laplace_observe_trace_fact_asserted(&ctx, 0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 0u);
    LAPLACE_TEST_ASSERT(ctx.counters.exact_facts_asserted == 1u);

    /* Transport error — should be traced at ERRORS level */
    laplace_observe_trace_transport_error(&ctx, 1u, 2u, 300u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&ctx.trace) == 1u);
    LAPLACE_TEST_ASSERT(ctx.counters.transport_commands_failed == 1u);
    return 0;
}

static int test_observe_should_trace(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_observe_context_t ctx;
    laplace_observe_init(&ctx, &arena, 1u);

    /* At AUDIT level, FACT_ASSERTED should be traced */
    LAPLACE_TEST_ASSERT(laplace_observe_should_trace(&ctx,
        LAPLACE_TRACE_SUBSYSTEM_EXACT, LAPLACE_TRACE_KIND_FACT_ASSERTED) == true);

    /* At AUDIT level, EXEC_STEP should NOT be traced */
    LAPLACE_TEST_ASSERT(laplace_observe_should_trace(&ctx,
        LAPLACE_TRACE_SUBSYSTEM_EXEC, LAPLACE_TRACE_KIND_EXEC_STEP) == false);

    /* NULL context should return false */
    LAPLACE_TEST_ASSERT(laplace_observe_should_trace(NULL,
        LAPLACE_TRACE_SUBSYSTEM_EXACT, LAPLACE_TRACE_KIND_FACT_ASSERTED) == false);
    return 0;
}

int laplace_test_observe(void) {
    typedef struct { const char* name; int (*fn)(void); } sub_t;
    const sub_t subs[] = {
        {"observe_init",                     test_observe_init},
        {"observe_init_null",                test_observe_init_null},
        {"observe_set_level",                test_observe_set_level},
        {"observe_set_mask",                 test_observe_set_mask},
        {"observe_counters_always_update",   test_observe_counters_always_update},
        {"observe_level_gating",             test_observe_level_gating},
        {"observe_subsystem_mask",           test_observe_subsystem_mask},
        {"observe_branch_epoch_tagging",     test_observe_branch_epoch_tagging},
        {"observe_reset",                    test_observe_reset},
        {"observe_latency_basic",            test_observe_latency_basic},
        {"observe_counter_reset",            test_observe_counter_reset},
        {"observe_errors_level",             test_observe_errors_level},
        {"observe_should_trace",             test_observe_should_trace},
    };
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); ++i) {
        const int r = subs[i].fn();
        if (r != 0) {
            fprintf(stderr, "  SUBTEST FAIL: %s\n", subs[i].name);
            return 1;
        }
    }
    return 0;
}
