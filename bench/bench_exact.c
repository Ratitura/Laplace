#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/state.h"

#define BENCH_EXACT_ENTITY_CAPACITY 512u
#define BENCH_EXACT_ENTITY_ARENA_BYTES (BENCH_EXACT_ENTITY_CAPACITY * 96u)
#define BENCH_EXACT_STORE_ARENA_BYTES  (2u * 1024u * 1024u)
#define BENCH_EXACT_FACT_BULK_COUNT    128u

typedef struct bench_exact_ctx {
    laplace_arena_t       entity_arena;
    laplace_arena_t       store_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;
    laplace_entity_handle_t constants[BENCH_EXACT_FACT_BULK_COUNT + 2u];
    laplace_entity_handle_t fact_entities[BENCH_EXACT_FACT_BULK_COUNT];
    laplace_exact_fact_row_t fact_rows[BENCH_EXACT_FACT_BULK_COUNT];
    laplace_provenance_id_t asserted_provenance;
    volatile uint32_t sink_u32;
} bench_exact_ctx_t;

static _Alignas(64) uint8_t g_bench_exact_entity_buf[BENCH_EXACT_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_exact_store_buf[BENCH_EXACT_STORE_ARENA_BYTES];
static bench_exact_ctx_t g_bench_exact;

static laplace_entity_handle_t bench_exact_alloc_ready(bench_exact_ctx_t* const ctx) {
    laplace_entity_handle_t handle = laplace_entity_pool_alloc(&ctx->entity_pool);
    if (handle.id != LAPLACE_ENTITY_ID_INVALID) {
        (void)laplace_entity_pool_set_state(&ctx->entity_pool, handle, LAPLACE_STATE_READY);
    }
    return handle;
}

static void bench_exact_setup(void) {
    memset(&g_bench_exact, 0, sizeof(g_bench_exact));
    (void)laplace_arena_init(&g_bench_exact.entity_arena, g_bench_exact_entity_buf, sizeof(g_bench_exact_entity_buf));
    (void)laplace_arena_init(&g_bench_exact.store_arena, g_bench_exact_store_buf, sizeof(g_bench_exact_store_buf));
    (void)laplace_entity_pool_init(&g_bench_exact.entity_pool, &g_bench_exact.entity_arena, BENCH_EXACT_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&g_bench_exact.store, &g_bench_exact.store_arena, &g_bench_exact.entity_pool);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u,
        .flags = 0u,
        .fact_capacity = 256u
    };
    (void)laplace_exact_register_predicate(&g_bench_exact.store, 1u, &desc);

    const laplace_exact_provenance_desc_t provenance_desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    (void)laplace_exact_insert_provenance(&g_bench_exact.store, &provenance_desc, &g_bench_exact.asserted_provenance);

    for (uint32_t i = 0u; i < BENCH_EXACT_FACT_BULK_COUNT + 2u; ++i) {
        const laplace_entity_handle_t constant = bench_exact_alloc_ready(&g_bench_exact);
        (void)laplace_exact_register_constant(&g_bench_exact.store, constant, 1u, 0u);
        g_bench_exact.constants[i] = constant;
    }

    for (uint32_t i = 0u; i < BENCH_EXACT_FACT_BULK_COUNT; ++i) {
        const laplace_entity_handle_t args[2] = {
            g_bench_exact.constants[i],
            g_bench_exact.constants[i + 1u]
        };
        bool inserted = false;
        (void)laplace_exact_assert_fact(&g_bench_exact.store,
                                        1u,
                                        args,
                                        2u,
                                        g_bench_exact.asserted_provenance,
                                        LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                        &g_bench_exact.fact_rows[i],
                                        &g_bench_exact.fact_entities[i],
                                        &inserted);
    }
}

static void bench_exact_lookup_hit(void* const context) {
    bench_exact_ctx_t* const ctx = (bench_exact_ctx_t*)context;
    const laplace_entity_id_t args[2] = {ctx->constants[17u].id, ctx->constants[18u].id};
    ctx->sink_u32 = laplace_exact_find_fact(&ctx->store, 1u, args, 2u);
}

static void bench_exact_lookup_miss(void* const context) {
    bench_exact_ctx_t* const ctx = (bench_exact_ctx_t*)context;
    const laplace_entity_id_t args[2] = {ctx->constants[BENCH_EXACT_FACT_BULK_COUNT].id, ctx->constants[BENCH_EXACT_FACT_BULK_COUNT + 1u].id};
    ctx->sink_u32 = laplace_exact_find_fact(&ctx->store, 1u, args, 2u);
}

static void bench_exact_predicate_scan(void* const context) {
    bench_exact_ctx_t* const ctx = (bench_exact_ctx_t*)context;
    const laplace_exact_predicate_view_t view = laplace_exact_predicate_rows(&ctx->store, 1u);
    ctx->sink_u32 = view.count;
    if (view.count != 0u) {
        ctx->sink_u32 ^= view.rows[0];
    }
}

void laplace_bench_exact(void) {
    bench_exact_setup();

    const laplace_bench_case_t benches[] = {
        {"exact_lookup_hit", bench_exact_lookup_hit, &g_bench_exact, 1000000u},
        {"exact_lookup_miss", bench_exact_lookup_miss, &g_bench_exact, 1000000u},
        {"exact_predicate_scan", bench_exact_predicate_scan, &g_bench_exact, 1000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}