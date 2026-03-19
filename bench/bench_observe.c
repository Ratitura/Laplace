#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/align.h"
#include "laplace/arena.h"
#include "laplace/observe.h"
#include "laplace/trace.h"

#define BENCH_OBSERVE_ARENA_BYTES (512u * 1024u)

static LAPLACE_ALIGNAS(64) uint8_t g_bench_observe_buf[BENCH_OBSERVE_ARENA_BYTES];
static laplace_arena_t g_bench_observe_arena;
static laplace_trace_buffer_t g_bench_trace_buf;
static laplace_observe_context_t g_bench_observe_ctx;

static void bench_observe_setup(void) {
    (void)laplace_arena_init(&g_bench_observe_arena, g_bench_observe_buf,
                              sizeof(g_bench_observe_buf));
    (void)laplace_trace_buffer_init(&g_bench_trace_buf, &g_bench_observe_arena);
    (void)laplace_observe_init(&g_bench_observe_ctx, &g_bench_observe_arena, 1u);
}

/*
 * Raw trace ring buffer emit cost.
 * This is the minimal cost of writing one 64-byte record into the ring.
 */
static void bench_trace_emit(void* const context) {
    (void)context;
    laplace_trace_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind      = (uint16_t)LAPLACE_TRACE_KIND_FACT_ASSERTED;
    rec.subsystem = (uint8_t)LAPLACE_TRACE_SUBSYSTEM_EXACT;
    rec.entity_id = 42u;
    rec.tick       = 100u;
    laplace_trace_emit(&g_bench_trace_buf, &rec);
}

/*
 * Convenience helper cost at OFF level: counter increment only, no trace.
 */
static void bench_counter_increment_off(void* const context) {
    (void)context;
    laplace_observe_trace_fact_asserted(&g_bench_observe_ctx,
        0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
}

/*
 * Convenience helper cost at AUDIT level: counter increment + trace emission.
 */
static void bench_trace_audit(void* const context) {
    (void)context;
    laplace_observe_trace_fact_asserted(&g_bench_observe_ctx,
        0u, 1u, 10u, 0u, 0u, 0u, 0u, 1u);
}

/*
 * exec_step at DEBUG level (normally suppressed at AUDIT).
 */
static void bench_trace_exec_step(void* const context) {
    (void)context;
    laplace_observe_trace_exec_step(&g_bench_observe_ctx,
        20u, 3u, 0u, 0u, 0u, 100u);
}

/*
 * Observe context initialization cost.
 */
static void bench_observe_init_cost(void* const context) {
    (void)context;
    /* Re-init uses the existing arena allocation */
    laplace_observe_reset(&g_bench_observe_ctx);
}

void laplace_bench_observe(void) {
    bench_observe_setup();

    /* Raw trace emit */
    {
        const laplace_bench_case_t c = {
            .name = "trace_emit",
            .fn = bench_trace_emit,
            .context = NULL,
            .iterations = 1000000u
        };
        (void)laplace_bench_run_case(&c);
    }

    /* Counter increment only (OFF level) */
    {
        laplace_observe_set_level(&g_bench_observe_ctx, LAPLACE_OBSERVE_OFF);
        const laplace_bench_case_t c = {
            .name = "counter_incr_off",
            .fn = bench_counter_increment_off,
            .context = NULL,
            .iterations = 1000000u
        };
        (void)laplace_bench_run_case(&c);
    }

    /* AUDIT-level trace (counter + trace emission) */
    {
        laplace_observe_set_level(&g_bench_observe_ctx, LAPLACE_OBSERVE_AUDIT);
        const laplace_bench_case_t c = {
            .name = "trace_audit_fact",
            .fn = bench_trace_audit,
            .context = NULL,
            .iterations = 1000000u
        };
        (void)laplace_bench_run_case(&c);
    }

    /* DEBUG-level exec_step */
    {
        laplace_observe_set_level(&g_bench_observe_ctx, LAPLACE_OBSERVE_DEBUG);
        const laplace_bench_case_t c = {
            .name = "trace_debug_step",
            .fn = bench_trace_exec_step,
            .context = NULL,
            .iterations = 1000000u
        };
        (void)laplace_bench_run_case(&c);
    }

    /* Reset cost */
    {
        const laplace_bench_case_t c = {
            .name = "observe_reset",
            .fn = bench_observe_init_cost,
            .context = NULL,
            .iterations = 100000u
        };
        (void)laplace_bench_run_case(&c);
    }
}
