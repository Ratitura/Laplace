#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/state.h"

#define BENCH_EXEC_ENTITY_CAPACITY   512u
#define BENCH_EXEC_ENTITY_ARENA_BYTES (BENCH_EXEC_ENTITY_CAPACITY * 96u)
#define BENCH_EXEC_STORE_ARENA_BYTES  (2u * 1024u * 1024u)
#define BENCH_EXEC_EXEC_ARENA_BYTES   (512u * 1024u)
#define BENCH_EXEC_CHAIN_LENGTH       32u

typedef struct bench_exec_ctx {
    laplace_arena_t          entity_arena;
    laplace_arena_t          store_arena;
    laplace_arena_t          exec_arena;
    laplace_entity_pool_t    entity_pool;
    laplace_exact_store_t    store;
    laplace_exec_context_t   exec;
    laplace_entity_handle_t  nodes[BENCH_EXEC_CHAIN_LENGTH + 1u];
    laplace_provenance_id_t  asserted_prov;
    volatile uint32_t        sink_u32;
    volatile uint64_t        sink_u64;
} bench_exec_ctx_t;

static _Alignas(64) uint8_t g_bench_exec_entity_buf[BENCH_EXEC_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_exec_store_buf[BENCH_EXEC_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_exec_exec_buf[BENCH_EXEC_EXEC_ARENA_BYTES];
static bench_exec_ctx_t g_bench_exec;

static laplace_entity_handle_t bench_exec_alloc_ready(bench_exec_ctx_t* const ctx) {
    laplace_entity_handle_t h = laplace_entity_pool_alloc(&ctx->entity_pool);
    if (h.id != LAPLACE_ENTITY_ID_INVALID) {
        (void)laplace_entity_pool_set_state(&ctx->entity_pool, h, LAPLACE_STATE_READY);
    }
    return h;
}

static void bench_exec_setup_tc(bench_exec_ctx_t* const ctx) {
    memset(ctx, 0, sizeof(*ctx));
    (void)laplace_arena_init(&ctx->entity_arena, g_bench_exec_entity_buf, sizeof(g_bench_exec_entity_buf));
    (void)laplace_arena_init(&ctx->store_arena, g_bench_exec_store_buf, sizeof(g_bench_exec_store_buf));
    (void)laplace_arena_init(&ctx->exec_arena, g_bench_exec_exec_buf, sizeof(g_bench_exec_exec_buf));
    (void)laplace_entity_pool_init(&ctx->entity_pool, &ctx->entity_arena, BENCH_EXEC_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&ctx->store, &ctx->store_arena, &ctx->entity_pool);
    (void)laplace_exec_init(&ctx->exec, &ctx->exec_arena, &ctx->store, &ctx->entity_pool);

    const laplace_exact_predicate_desc_t pred = {.arity = 2u, .flags = 0u, .fact_capacity = 256u};
    (void)laplace_exact_register_predicate(&ctx->store, 1u, &pred);
    (void)laplace_exact_register_predicate(&ctx->store, 2u, &pred);

    const laplace_exact_provenance_desc_t prov_desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    (void)laplace_exact_insert_provenance(&ctx->store, &prov_desc, &ctx->asserted_prov);

    for (uint32_t i = 0u; i <= BENCH_EXEC_CHAIN_LENGTH; ++i) {
        ctx->nodes[i] = bench_exec_alloc_ready(ctx);
        (void)laplace_exact_register_constant(&ctx->store, ctx->nodes[i], 1u, 0u);
    }

    for (uint32_t i = 0u; i < BENCH_EXEC_CHAIN_LENGTH; ++i) {
        const laplace_entity_handle_t args[2] = {ctx->nodes[i], ctx->nodes[i + 1u]};
        laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
        laplace_entity_handle_t fe = {0};
        bool inserted = false;
        (void)laplace_exact_assert_fact(&ctx->store, 1u, args, 2u, ctx->asserted_prov,
                                         LAPLACE_EXACT_FACT_FLAG_ASSERTED, &row, &fe, &inserted);
    }

    laplace_exact_literal_t b1[1];
    memset(b1, 0, sizeof(b1));
    b1[0].predicate = 1u; b1[0].arity = 2u;
    b1[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    b1[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t r1;
    memset(&r1, 0, sizeof(r1));
    r1.head.predicate = 2u; r1.head.arity = 2u;
    r1.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    r1.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    r1.body_literals = b1;
    r1.body_count = 1u;

    laplace_exact_literal_t b2[2];
    memset(b2, 0, sizeof(b2));
    b2[0].predicate = 2u; b2[0].arity = 2u;
    b2[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    b2[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    b2[1].predicate = 1u; b2[1].arity = 2u;
    b2[1].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    b2[1].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t r2;
    memset(&r2, 0, sizeof(r2));
    r2.head.predicate = 2u; r2.head.arity = 2u;
    r2.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    r2.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    r2.body_literals = b2;
    r2.body_count = 2u;

    laplace_rule_id_t rid = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t val;
    (void)laplace_exact_add_rule(&ctx->store, &r1, &rid, &val);
    (void)laplace_exact_add_rule(&ctx->store, &r2, &rid, &val);
    (void)laplace_exec_build_trigger_index(&ctx->exec);
}

static void bench_exec_ready_push_pop(void* const context) {
    bench_exec_ctx_t* const ctx = (bench_exec_ctx_t*)context;

    laplace_exec_reset(&ctx->exec);
    (void)laplace_exec_build_trigger_index(&ctx->exec);

    const uint32_t marked = laplace_exec_mark_all_facts_ready(&ctx->exec);
    ctx->sink_u32 = marked;
    ctx->sink_u32 ^= laplace_exec_ready_count(&ctx->exec);
}

static void bench_exec_dense_scan(void* const context) {
    bench_exec_ctx_t* const ctx = (bench_exec_ctx_t*)context;

    laplace_exec_reset(&ctx->exec);
    (void)laplace_exec_build_trigger_index(&ctx->exec);
    laplace_exec_set_mode(&ctx->exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&ctx->exec);

    (void)laplace_exec_step(&ctx->exec);
    ctx->sink_u32 = laplace_exec_ready_count(&ctx->exec);
}

static void bench_exec_single_rule_fire(void* const context) {
    bench_exec_ctx_t* const ctx = (bench_exec_ctx_t*)context;

    bench_exec_setup_tc(ctx);
    laplace_exec_set_mode(&ctx->exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&ctx->exec);

    (void)laplace_exec_step(&ctx->exec);
    ctx->sink_u64 = ctx->exec.stats.rules_fired;
}

static void bench_exec_fixpoint_run(void* const context) {
    bench_exec_ctx_t* const ctx = (bench_exec_ctx_t*)context;

    bench_exec_setup_tc(ctx);
    laplace_exec_set_mode(&ctx->exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&ctx->exec);

    (void)laplace_exec_run(&ctx->exec);
    ctx->sink_u64 = ctx->exec.stats.facts_derived;
}

static void bench_exec_fixpoint_run_semi_naive(void* const context) {
    bench_exec_ctx_t* const ctx = (bench_exec_ctx_t*)context;

    bench_exec_setup_tc(ctx);
    laplace_exec_set_mode(&ctx->exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_set_semi_naive(&ctx->exec, true);
    laplace_exec_mark_all_facts_ready(&ctx->exec);

    (void)laplace_exec_run(&ctx->exec);
    ctx->sink_u64 = ctx->exec.stats.facts_derived;
}

void laplace_bench_exec(void) {
    bench_exec_setup_tc(&g_bench_exec);

    const laplace_bench_case_t benches[] = {
        {"exec_ready_push_pop",             bench_exec_ready_push_pop,           &g_bench_exec, 100000u},
        {"exec_dense_scan_step",            bench_exec_dense_scan,               &g_bench_exec, 100000u},
        {"exec_single_rule_fire",           bench_exec_single_rule_fire,         &g_bench_exec, 10000u},
        {"exec_fixpoint_run_tc32",          bench_exec_fixpoint_run,             &g_bench_exec, 1000u},
        {"exec_fixpoint_run_tc32_seminaive", bench_exec_fixpoint_run_semi_naive, &g_bench_exec, 1000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
