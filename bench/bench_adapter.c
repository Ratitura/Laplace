#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/adapter.h"
#include "laplace/adapter_facts.h"
#include "laplace/adapter_hv.h"
#include "laplace/adapter_rules.h"
#include "laplace/adapter_verify.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/hv.h"

#define BENCH_ADAPTER_ENTITY_CAPACITY   256u
#define BENCH_ADAPTER_ENTITY_ARENA_BYTES (BENCH_ADAPTER_ENTITY_CAPACITY * 96u)
#define BENCH_ADAPTER_STORE_ARENA_BYTES  (4u * 1024u * 1024u)

static _Alignas(64) uint8_t g_bench_entity_buf[BENCH_ADAPTER_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_store_buf[BENCH_ADAPTER_STORE_ARENA_BYTES];

typedef struct bench_adapter_ctx {
    laplace_arena_t       entity_arena;
    laplace_arena_t       store_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;

    /* Pre-built artifacts for benchmarking. */
    laplace_adapter_rule_artifact_t  rule_artifact;
    laplace_adapter_hv_header_t      hv_header;
    uint64_t                          hv_words[LAPLACE_HV_WORDS];
    laplace_adapter_fact_request_t   fact_request;
    laplace_adapter_verify_fact_query_t verify_query;

    /* Entities for fact injection. */
    laplace_entity_handle_t constants[16];
    uint32_t                constant_count;

    /* Sinks to prevent optimization. */
    volatile uint64_t sink_u64;
    volatile uint32_t sink_u32;
} bench_adapter_ctx_t;

static bench_adapter_ctx_t g_bench_ctx;

static int bench_adapter_setup(void) {
    bench_adapter_ctx_t* const ctx = &g_bench_ctx;
    memset(ctx, 0, sizeof(*ctx));

    if (laplace_arena_init(&ctx->entity_arena,
                           g_bench_entity_buf,
                           sizeof(g_bench_entity_buf)) != LAPLACE_OK) {
        return -1;
    }
    if (laplace_arena_init(&ctx->store_arena,
                           g_bench_store_buf,
                           sizeof(g_bench_store_buf)) != LAPLACE_OK) {
        return -1;
    }
    if (laplace_entity_pool_init(&ctx->entity_pool,
                                 &ctx->entity_arena,
                                 BENCH_ADAPTER_ENTITY_CAPACITY) != LAPLACE_OK) {
        return -1;
    }
    if (laplace_exact_store_init(&ctx->store,
                                 &ctx->store_arena,
                                 &ctx->entity_pool) != LAPLACE_OK) {
        return -1;
    }

    /* Register predicates. */
    const laplace_exact_predicate_desc_t unary_desc = {
        .arity = 1,
        .flags = LAPLACE_EXACT_PREDICATE_FLAG_NONE,
        .fact_capacity = 128
    };
    if (laplace_exact_register_predicate(&ctx->store, 1, &unary_desc) != LAPLACE_OK) {
        return -1;
    }
    if (laplace_exact_register_predicate(&ctx->store, 2, &unary_desc) != LAPLACE_OK) {
        return -1;
    }

    /* Allocate constants. */
    for (uint32_t i = 0; i < 16; ++i) {
        ctx->constants[i] = laplace_entity_pool_alloc(&ctx->entity_pool);
        if (ctx->constants[i].id == LAPLACE_ENTITY_ID_INVALID) { return -1; }
        if (laplace_entity_pool_set_state(&ctx->entity_pool,
                                          ctx->constants[i],
                                          LAPLACE_STATE_READY) != LAPLACE_OK) {
            return -1;
        }
        if (laplace_exact_register_constant(&ctx->store,
                                            ctx->constants[i],
                                            (laplace_exact_type_id_t)1u,
                                            0u) != LAPLACE_OK) {
            return -1;
        }
    }
    ctx->constant_count = 16;

    /* Pre-build rule artifact: Q(X) :- P(X). */
    memset(&ctx->rule_artifact, 0, sizeof(ctx->rule_artifact));
    ctx->rule_artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    ctx->rule_artifact.body_count  = 1;
    ctx->rule_artifact.head.predicate_id = 2;
    ctx->rule_artifact.head.arity = 1;
    ctx->rule_artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};
    ctx->rule_artifact.body[0].predicate_id = 1;
    ctx->rule_artifact.body[0].arity = 1;
    ctx->rule_artifact.body[0].terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};

    /* Pre-build HV header and words. */
    memset(&ctx->hv_header, 0, sizeof(ctx->hv_header));
    ctx->hv_header.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    ctx->hv_header.hv_dimension = LAPLACE_HV_DIM;
    ctx->hv_header.hv_words     = LAPLACE_HV_WORDS;
    for (uint32_t i = 0; i < LAPLACE_HV_WORDS; ++i) {
        ctx->hv_words[i] = (uint64_t)i * 0x5555555555555555ULL;
    }

    /* Pre-build fact request. */
    memset(&ctx->fact_request, 0, sizeof(ctx->fact_request));
    ctx->fact_request.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    ctx->fact_request.predicate_id = 1;
    ctx->fact_request.arg_count    = 1;
    ctx->fact_request.args[0].id         = ctx->constants[0].id;
    ctx->fact_request.args[0].generation = ctx->constants[0].generation;

    /* Pre-build verify query. */
    memset(&ctx->verify_query, 0, sizeof(ctx->verify_query));
    ctx->verify_query.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    ctx->verify_query.predicate_id = 1;
    ctx->verify_query.arg_count    = 1;
    ctx->verify_query.args[0]      = ctx->constants[0].id;

    return 0;
}

static void bench_capability_query(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    laplace_adapter_capability_t cap;
    laplace_adapter_query_capability(&cap);
    ctx->sink_u32 = cap.abi_version;
}

static void bench_rule_validate(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    const laplace_adapter_status_t s =
        laplace_adapter_validate_rule_artifact(&ctx->rule_artifact);
    ctx->sink_u32 = (uint32_t)s;
}

static void bench_hv_ingest(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    laplace_hv_t hv;
    const laplace_adapter_status_t s =
        laplace_adapter_hv_ingest(&ctx->hv_header, ctx->hv_words, &hv);
    ctx->sink_u64 = hv.words[0];
    ctx->sink_u32 = (uint32_t)s;
}

static void bench_fact_validate(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    const laplace_adapter_status_t s =
        laplace_adapter_validate_fact_request(&ctx->store, &ctx->entity_pool,
                                             &ctx->fact_request);
    ctx->sink_u32 = (uint32_t)s;
}

/*
 * Benchmark fact injection using a pre-injected (duplicate) fact.
 * This measures the dedup path, which is the steady-state hot path.
 */
static void bench_fact_inject_dedup(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&ctx->store, &ctx->fact_request, &resp);
    ctx->sink_u32 = (uint32_t)s;
    ctx->sink_u64 = resp.fact_row;
}

static void bench_verify_fact_exists(void* const context) {
    bench_adapter_ctx_t* const ctx = (bench_adapter_ctx_t*)context;
    laplace_adapter_verify_fact_result_t result;
    const laplace_adapter_status_t s =
        laplace_adapter_verify_fact_exists(&ctx->store, &ctx->verify_query, &result);
    ctx->sink_u32 = (uint32_t)s;
    ctx->sink_u64 = result.fact_row;
}

void laplace_bench_adapter(void) {
    if (bench_adapter_setup() != 0) {
        fprintf(stderr, "  [ERROR] adapter benchmark setup failed\n");
        return;
    }

    /* Pre-inject one fact so the dedup bench and verify bench work. */
    {
        laplace_adapter_fact_response_t resp;
        laplace_adapter_inject_fact(&g_bench_ctx.store,
                                    &g_bench_ctx.fact_request, &resp);
    }

    const laplace_bench_case_t benches[] = {
        {"adapter_capability_query",  bench_capability_query,  &g_bench_ctx, 10000000u},
        {"adapter_rule_validate",     bench_rule_validate,     &g_bench_ctx,  5000000u},
        {"adapter_hv_ingest",         bench_hv_ingest,         &g_bench_ctx,  1000000u},
        {"adapter_fact_validate",     bench_fact_validate,     &g_bench_ctx,  5000000u},
        {"adapter_fact_inject_dedup", bench_fact_inject_dedup, &g_bench_ctx,  2000000u},
        {"adapter_verify_fact_exists",bench_verify_fact_exists,&g_bench_ctx,  5000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);

    for (size_t i = 0; i < count; ++i) {
        const double ns = laplace_bench_run_case(&benches[i]);
        if (ns < 0.0) {
            fprintf(stderr, "  [ERROR] bench %s failed\n", benches[i].name);
        }
    }
}
