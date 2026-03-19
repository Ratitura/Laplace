#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/branch.h"
#include "laplace/entity.h"
#include "laplace/exact.h"

#define BENCH_BRANCH_ENTITY_CAPACITY 256u
#define BENCH_BRANCH_ENTITY_ARENA_BYTES (BENCH_BRANCH_ENTITY_CAPACITY * 128u)
#define BENCH_BRANCH_STORE_ARENA_BYTES  (2u * 1024u * 1024u)
#define BENCH_BRANCH_BRANCH_ARENA_BYTES (3u * 1024u * 1024u)

typedef struct bench_branch_ctx {
    laplace_arena_t entity_arena;
    laplace_arena_t store_arena;
    laplace_arena_t branch_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;
    laplace_branch_system_t branches;
    laplace_entity_handle_t alice;
    laplace_entity_handle_t bob;
    laplace_provenance_id_t asserted;
    volatile uint32_t sink_u32;
} bench_branch_ctx_t;

static _Alignas(64) uint8_t g_bench_branch_entity_buf[BENCH_BRANCH_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_branch_store_buf[BENCH_BRANCH_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_branch_branch_buf[BENCH_BRANCH_BRANCH_ARENA_BYTES];
static bench_branch_ctx_t g_bench_branch;

static laplace_entity_handle_t bench_branch_alloc_ready(bench_branch_ctx_t* const ctx) {
    laplace_entity_handle_t handle = laplace_entity_pool_alloc(&ctx->entity_pool);
    if (handle.id != LAPLACE_ENTITY_ID_INVALID) {
        (void)laplace_entity_pool_set_state(&ctx->entity_pool, handle, LAPLACE_STATE_READY);
    }
    return handle;
}

static void bench_branch_setup(bench_branch_ctx_t* const ctx) {
    memset(ctx, 0, sizeof(*ctx));
    (void)laplace_arena_init(&ctx->entity_arena, g_bench_branch_entity_buf, sizeof(g_bench_branch_entity_buf));
    (void)laplace_arena_init(&ctx->store_arena, g_bench_branch_store_buf, sizeof(g_bench_branch_store_buf));
    (void)laplace_arena_init(&ctx->branch_arena, g_bench_branch_branch_buf, sizeof(g_bench_branch_branch_buf));
    (void)laplace_entity_pool_init(&ctx->entity_pool, &ctx->entity_arena, BENCH_BRANCH_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&ctx->store, &ctx->store_arena, &ctx->entity_pool);
    (void)laplace_branch_system_init(&ctx->branches, &ctx->branch_arena, &ctx->store, &ctx->entity_pool);

    const laplace_exact_predicate_desc_t pred = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    (void)laplace_exact_register_predicate(&ctx->store, 1u, &pred);

    ctx->alice = bench_branch_alloc_ready(ctx);
    ctx->bob = bench_branch_alloc_ready(ctx);
    (void)laplace_exact_register_constant(&ctx->store, ctx->alice, 1u, 0u);
    (void)laplace_exact_register_constant(&ctx->store, ctx->bob, 1u, 0u);

    const laplace_exact_provenance_desc_t asserted_desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    (void)laplace_exact_insert_provenance(&ctx->store, &asserted_desc, &ctx->asserted);
}

static laplace_branch_handle_t bench_branch_create_with_fact(bench_branch_ctx_t* const ctx,
                                                              laplace_exact_fact_row_t* const out_row,
                                                              laplace_entity_handle_t* const out_fact_entity) {
    laplace_error_t error = LAPLACE_OK;
    const laplace_branch_handle_t branch = laplace_branch_create(&ctx->branches,
                                                                  (laplace_branch_handle_t){0},
                                                                  &error);
    laplace_provenance_id_t branch_prov = LAPLACE_PROVENANCE_ID_INVALID;
    (void)laplace_branch_insert_asserted_provenance(&ctx->branches, branch, &branch_prov);

    const laplace_entity_handle_t args[2] = {ctx->alice, ctx->bob};
    bool inserted = false;
    (void)laplace_branch_assert_fact(&ctx->branches,
                                     branch,
                                     1u,
                                     args,
                                     2u,
                                     branch_prov,
                                     LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                     out_row,
                                     out_fact_entity,
                                     &inserted);
    return branch;
}

static void bench_branch_e2e_create(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;
    bench_branch_setup(ctx);
    laplace_error_t error = LAPLACE_OK;
    const laplace_branch_handle_t branch = laplace_branch_create(&ctx->branches,
                                                                  (laplace_branch_handle_t){0},
                                                                  &error);
    ctx->sink_u32 = (uint32_t)branch.id + (uint32_t)error;
}

static void bench_branch_e2e_fail(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;
    bench_branch_setup(ctx);

    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    const laplace_branch_handle_t branch = bench_branch_create_with_fact(ctx, &row, &fact_entity);
    (void)laplace_branch_fail(&ctx->branches, branch);
    ctx->sink_u32 = row + (uint32_t)fact_entity.id;
}

static void bench_branch_e2e_commit(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;
    bench_branch_setup(ctx);

    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    const laplace_branch_handle_t branch = bench_branch_create_with_fact(ctx, &row, &fact_entity);
    uint32_t promoted = 0u;
    uint32_t deduplicated = 0u;
    (void)laplace_branch_commit(&ctx->branches, branch, &promoted, &deduplicated);
    ctx->sink_u32 = promoted + deduplicated + row;
}

static void bench_branch_setup_only(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;
    bench_branch_setup(ctx);
    ctx->sink_u32 = ctx->branches.current_epoch;
}

static void bench_branch_iso_create(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;

    /* Timed: create + immediate fail to free the slot for reuse */
    laplace_error_t error = LAPLACE_OK;
    const laplace_branch_handle_t branch = laplace_branch_create(&ctx->branches,
                                                                  (laplace_branch_handle_t){0},
                                                                  &error);
    ctx->sink_u32 = (uint32_t)branch.id + (uint32_t)error;

    /* Teardown: fail + reclaim to reset the slot.  No owned facts/entities,
     * so this is just status flip + generation bump — trivially cheap. */
    (void)laplace_branch_fail(&ctx->branches, branch);
    (void)laplace_branch_advance_epoch(&ctx->branches, NULL);
    (void)laplace_branch_reclaim_closed(&ctx->branches, NULL, NULL);
}

static void bench_branch_iso_fail(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;

    /* Pre-op: create branch + assert one fact */
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    const laplace_branch_handle_t branch = bench_branch_create_with_fact(ctx, &row, &fact_entity);

    /* Timed target: fail the branch (retire 1 fact + mark status) */
    (void)laplace_branch_fail(&ctx->branches, branch);
    ctx->sink_u32 = row + (uint32_t)fact_entity.id;

    /* Reclaim to reset the slot for next iteration */
    (void)laplace_branch_advance_epoch(&ctx->branches, NULL);
    (void)laplace_branch_reclaim_closed(&ctx->branches, NULL, NULL);
}

static void bench_branch_iso_commit(void* const context) {
    bench_branch_ctx_t* const ctx = (bench_branch_ctx_t*)context;

    /* Pre-op: create branch + assert one fact */
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    const laplace_branch_handle_t branch = bench_branch_create_with_fact(ctx, &row, &fact_entity);

    /* Timed target: commit the branch (promote 1 fact + dedup check + mark status) */
    uint32_t promoted = 0u;
    uint32_t deduplicated = 0u;
    (void)laplace_branch_commit(&ctx->branches, branch, &promoted, &deduplicated);
    ctx->sink_u32 = promoted + deduplicated + row;

    /* Reclaim to reset the slot for next iteration.
     * The promoted fact remains in the committed store, which is fine:
     * subsequent iterations will dedup against it, exercising the dedup path
     * naturally rather than artificially. */
    (void)laplace_branch_advance_epoch(&ctx->branches, NULL);
    (void)laplace_branch_reclaim_closed(&ctx->branches, NULL, NULL);
}

void laplace_bench_branch(void) {
    bench_branch_setup(&g_bench_branch);

    printf("\n  End-to-end (cold setup per iteration):\n");
    {
        const laplace_bench_case_t benches[] = {
            {"branch_e2e_setup_only",         bench_branch_setup_only,  &g_bench_branch, 100000u},
            {"branch_e2e_create",             bench_branch_e2e_create,  &g_bench_branch, 100000u},
            {"branch_e2e_fail_single_fact",   bench_branch_e2e_fail,    &g_bench_branch, 20000u},
            {"branch_e2e_commit_single_fact", bench_branch_e2e_commit,  &g_bench_branch, 20000u},
        };
        for (size_t i = 0u; i < sizeof(benches) / sizeof(benches[0]); ++i) {
            laplace_bench_run_case(&benches[i]);
        }
    }

    /* Re-setup for isolated runs (fixture is reused across iterations) */
    bench_branch_setup(&g_bench_branch);

    printf("\n  Isolated (pre-initialized fixture, branch-op only):\n");
    {
        const laplace_bench_case_t benches[] = {
            {"branch_iso_create",             bench_branch_iso_create,  &g_bench_branch, 1000000u},
            {"branch_iso_fail_single_fact",   bench_branch_iso_fail,    &g_bench_branch, 500000u},
            {"branch_iso_commit_single_fact", bench_branch_iso_commit,  &g_bench_branch, 500000u},
        };
        for (size_t i = 0u; i < sizeof(benches) / sizeof(benches[0]); ++i) {
            laplace_bench_run_case(&benches[i]);
        }
    }
}
