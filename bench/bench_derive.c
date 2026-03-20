#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/derive.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/state.h"

#define BENCH_DERIVE_ENTITY_CAPACITY 256u
#define BENCH_DERIVE_ENTITY_ARENA    (BENCH_DERIVE_ENTITY_CAPACITY * 96u)
#define BENCH_DERIVE_STORE_ARENA     (2u * 1024u * 1024u)
#define BENCH_DERIVE_EXEC_ARENA      (256u * 1024u)

typedef struct bench_derive_ctx {
    laplace_arena_t          entity_arena;
    laplace_arena_t          store_arena;
    laplace_arena_t          exec_arena;
    laplace_entity_pool_t    entity_pool;
    laplace_exact_store_t    store;
    laplace_exec_context_t   exec;
    laplace_derive_context_t derive;

    laplace_derive_action_t  action_caps;
    laplace_derive_action_t  action_assert;
    laplace_derive_action_t  action_lookup;
    laplace_derive_action_t  action_unsupported;

    volatile uint8_t         sink_u8;
    volatile uint32_t        sink_u32;
} bench_derive_ctx_t;

static _Alignas(64) uint8_t g_bench_derive_entity_buf[BENCH_DERIVE_ENTITY_ARENA];
static _Alignas(64) uint8_t g_bench_derive_store_buf[BENCH_DERIVE_STORE_ARENA];
static _Alignas(64) uint8_t g_bench_derive_exec_buf[BENCH_DERIVE_EXEC_ARENA];
static bench_derive_ctx_t g_bench_derive;

static laplace_entity_handle_t bench_derive_alloc_ready(bench_derive_ctx_t* ctx) {
    laplace_entity_handle_t h = laplace_entity_pool_alloc(&ctx->entity_pool);
    if (h.id != LAPLACE_ENTITY_ID_INVALID) {
        (void)laplace_entity_pool_set_state(&ctx->entity_pool, h, LAPLACE_STATE_READY);
    }
    return h;
}

static void bench_derive_setup(void) {
    bench_derive_ctx_t* const ctx = &g_bench_derive;
    memset(ctx, 0, sizeof(*ctx));

    (void)laplace_arena_init(&ctx->entity_arena,
        g_bench_derive_entity_buf, sizeof(g_bench_derive_entity_buf));
    (void)laplace_arena_init(&ctx->store_arena,
        g_bench_derive_store_buf, sizeof(g_bench_derive_store_buf));
    (void)laplace_arena_init(&ctx->exec_arena,
        g_bench_derive_exec_buf, sizeof(g_bench_derive_exec_buf));

    (void)laplace_entity_pool_init(&ctx->entity_pool,
        &ctx->entity_arena, BENCH_DERIVE_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&ctx->store,
        &ctx->store_arena, &ctx->entity_pool);
    (void)laplace_exec_init(&ctx->exec, &ctx->exec_arena,
        &ctx->store, &ctx->entity_pool);

    laplace_derive_context_init(&ctx->derive,
        &ctx->store, &ctx->exec, NULL, NULL, NULL, NULL, NULL);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 256u};
    (void)laplace_exact_register_predicate(&ctx->store, 1u, &desc);

    laplace_entity_handle_t a = bench_derive_alloc_ready(ctx);
    laplace_entity_handle_t b = bench_derive_alloc_ready(ctx);
    (void)laplace_exact_register_constant(&ctx->store, a, 1u, 0u);
    (void)laplace_exact_register_constant(&ctx->store, b, 1u, 0u);

    memset(&ctx->action_caps, 0, sizeof(ctx->action_caps));
    ctx->action_caps.api_version    = LAPLACE_DERIVE_API_VERSION;
    ctx->action_caps.kernel         = LAPLACE_KERNEL_RELATIONAL;
    ctx->action_caps.action         = LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES;
    ctx->action_caps.correlation_id = 1u;

    memset(&ctx->action_assert, 0, sizeof(ctx->action_assert));
    ctx->action_assert.api_version = LAPLACE_DERIVE_API_VERSION;
    ctx->action_assert.kernel      = LAPLACE_KERNEL_RELATIONAL;
    ctx->action_assert.action      = LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT;
    ctx->action_assert.correlation_id = 2u;
    ctx->action_assert.payload.rel_assert_fact.predicate_id = 1u;
    ctx->action_assert.payload.rel_assert_fact.arg_count    = 2u;
    ctx->action_assert.payload.rel_assert_fact.args[0].id         = a.id;
    ctx->action_assert.payload.rel_assert_fact.args[0].generation  = a.generation;
    ctx->action_assert.payload.rel_assert_fact.args[1].id         = b.id;
    ctx->action_assert.payload.rel_assert_fact.args[1].generation  = b.generation;

    memset(&ctx->action_lookup, 0, sizeof(ctx->action_lookup));
    ctx->action_lookup.api_version = LAPLACE_DERIVE_API_VERSION;
    ctx->action_lookup.kernel      = LAPLACE_KERNEL_RELATIONAL;
    ctx->action_lookup.action      = LAPLACE_DERIVE_ACTION_REL_LOOKUP_FACT;
    ctx->action_lookup.correlation_id = 3u;
    ctx->action_lookup.payload.rel_lookup_fact.predicate_id = 1u;
    ctx->action_lookup.payload.rel_lookup_fact.arg_count    = 2u;
    ctx->action_lookup.payload.rel_lookup_fact.args[0].id         = a.id;
    ctx->action_lookup.payload.rel_lookup_fact.args[0].generation  = a.generation;
    ctx->action_lookup.payload.rel_lookup_fact.args[1].id         = b.id;
    ctx->action_lookup.payload.rel_lookup_fact.args[1].generation  = b.generation;

    memset(&ctx->action_unsupported, 0, sizeof(ctx->action_unsupported));
    ctx->action_unsupported.api_version    = LAPLACE_DERIVE_API_VERSION;
    ctx->action_unsupported.kernel         = LAPLACE_KERNEL_PROOF;
    ctx->action_unsupported.action         = LAPLACE_DERIVE_ACTION_PROOF_IMPORT;
    ctx->action_unsupported.correlation_id = 4u;

    laplace_derive_result_t r;
    laplace_derive_dispatch(&ctx->derive, &ctx->action_assert, &r);
}

static void bench_derive_dispatch_capabilities(void* const context) {
    bench_derive_ctx_t* const ctx = (bench_derive_ctx_t*)context;
    laplace_derive_result_t result;
    laplace_derive_dispatch(&ctx->derive, &ctx->action_caps, &result);
    ctx->sink_u8 = result.status;
}

static void bench_derive_dispatch_lookup_hit(void* const context) {
    bench_derive_ctx_t* const ctx = (bench_derive_ctx_t*)context;
    laplace_derive_result_t result;
    laplace_derive_dispatch(&ctx->derive, &ctx->action_lookup, &result);
    ctx->sink_u32 = result.payload.rel_lookup.found;
}

static void bench_derive_dispatch_unsupported(void* const context) {
    bench_derive_ctx_t* const ctx = (bench_derive_ctx_t*)context;
    laplace_derive_result_t result;
    laplace_derive_dispatch(&ctx->derive, &ctx->action_unsupported, &result);
    ctx->sink_u8 = result.status;
}

void laplace_bench_derive(void) {
    bench_derive_setup();

    const laplace_bench_case_t benches[] = {
        {"derive_dispatch_capabilities", bench_derive_dispatch_capabilities,
         &g_bench_derive, 1000000u},
        {"derive_dispatch_lookup_hit", bench_derive_dispatch_lookup_hit,
         &g_bench_derive, 1000000u},
        {"derive_dispatch_unsupported", bench_derive_dispatch_unsupported,
         &g_bench_derive, 1000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
