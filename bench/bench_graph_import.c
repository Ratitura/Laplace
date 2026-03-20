#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/graph_artifact.h"
#include "laplace/graph_import.h"
#include "laplace/state.h"

#define BENCH_GI_ENTITY_CAPACITY     512u
#define BENCH_GI_ENTITY_ARENA_BYTES  (BENCH_GI_ENTITY_CAPACITY * 96u)
#define BENCH_GI_STORE_ARENA_BYTES   (2u * 1024u * 1024u)
#define BENCH_GI_EXEC_ARENA_BYTES    (512u * 1024u)
#define BENCH_GI_GRAPH_SIZE          32u

typedef struct bench_gi_ctx {
    laplace_arena_t          entity_arena;
    laplace_arena_t          store_arena;
    laplace_arena_t          exec_arena;
    laplace_entity_pool_t    entity_pool;
    laplace_exact_store_t    store;
    laplace_exec_context_t   exec;
    volatile uint32_t        sink_u32;
    volatile uint64_t        sink_u64;
} bench_gi_ctx_t;

static _Alignas(64) uint8_t g_bench_gi_entity_buf[BENCH_GI_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_gi_store_buf[BENCH_GI_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_bench_gi_exec_buf[BENCH_GI_EXEC_ARENA_BYTES];
static bench_gi_ctx_t g_bench_gi;

static laplace_graph_artifact_predicate_t g_gi_preds[2];
static laplace_graph_artifact_entity_t    g_gi_ents[BENCH_GI_GRAPH_SIZE + 1u];
static laplace_graph_artifact_fact_t      g_gi_facts[BENCH_GI_GRAPH_SIZE + BENCH_GI_GRAPH_SIZE];
static laplace_graph_artifact_rule_t      g_gi_rules[1];
static laplace_graph_artifact_t           g_gi_artifact;

static void bench_gi_build_fixture(void) {
    memset(g_gi_preds, 0, sizeof(g_gi_preds));
    memset(g_gi_ents, 0, sizeof(g_gi_ents));
    memset(g_gi_facts, 0, sizeof(g_gi_facts));
    memset(g_gi_rules, 0, sizeof(g_gi_rules));

    /* pred 0: unary type;  pred 1: binary edge */
    g_gi_preds[0].local_id = 0u; g_gi_preds[0].arity = 1u;
    g_gi_preds[1].local_id = 1u; g_gi_preds[1].arity = 2u;

    /* entities */
    for (uint32_t i = 0u; i <= BENCH_GI_GRAPH_SIZE; ++i) {
        g_gi_ents[i].local_id = i + 1u;
    }

    /* unary type facts */
    for (uint32_t i = 0u; i < BENCH_GI_GRAPH_SIZE; ++i) {
        g_gi_facts[i].predicate_local_id = 0u;
        g_gi_facts[i].arg_count = 1u;
        g_gi_facts[i].arg_entity_local_ids[0] = i + 1u;
    }

    /* binary edge facts */
    for (uint32_t i = 0u; i < BENCH_GI_GRAPH_SIZE; ++i) {
        const uint32_t fi = BENCH_GI_GRAPH_SIZE + i;
        g_gi_facts[fi].predicate_local_id = 1u;
        g_gi_facts[fi].arg_count = 2u;
        g_gi_facts[fi].arg_entity_local_ids[0] = i + 1u;
        g_gi_facts[fi].arg_entity_local_ids[1] = i + 2u;
    }

    /* rule: edge(X,Y) => edge(Y,X) (symmetric) */
    g_gi_rules[0].body_count = 1u;
    g_gi_rules[0].head.predicate_local_id = 1u;
    g_gi_rules[0].head.arity = 2u;
    g_gi_rules[0].head.terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};
    g_gi_rules[0].head.terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    g_gi_rules[0].body[0].predicate_local_id = 1u;
    g_gi_rules[0].body[0].arity = 2u;
    g_gi_rules[0].body[0].terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    g_gi_rules[0].body[0].terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};

    memset(&g_gi_artifact, 0, sizeof(g_gi_artifact));
    g_gi_artifact.header.magic = LAPLACE_GRAPH_ARTIFACT_MAGIC;
    g_gi_artifact.header.version = LAPLACE_GRAPH_ARTIFACT_VERSION;
    g_gi_artifact.header.profile_id = LAPLACE_GRAPH_PROFILE_HORN_CLOSURE;
    g_gi_artifact.header.predicate_count = 2u;
    g_gi_artifact.header.entity_count = BENCH_GI_GRAPH_SIZE + 1u;
    g_gi_artifact.header.fact_count = BENCH_GI_GRAPH_SIZE * 2u;
    g_gi_artifact.header.rule_count = 0u;
    g_gi_artifact.header.flags = 0u;
    g_gi_artifact.header.checksum =
        laplace_graph_artifact_compute_checksum(&g_gi_artifact.header);
    g_gi_artifact.predicates = g_gi_preds;
    g_gi_artifact.entities = g_gi_ents;
    g_gi_artifact.facts = g_gi_facts;
    g_gi_artifact.rules = NULL;
}

static void bench_gi_init_base(bench_gi_ctx_t* const ctx) {
    memset(ctx, 0, sizeof(*ctx));
    (void)laplace_arena_init(&ctx->entity_arena, g_bench_gi_entity_buf,
                             sizeof(g_bench_gi_entity_buf));
    (void)laplace_arena_init(&ctx->store_arena, g_bench_gi_store_buf,
                             sizeof(g_bench_gi_store_buf));
    (void)laplace_arena_init(&ctx->exec_arena, g_bench_gi_exec_buf,
                             sizeof(g_bench_gi_exec_buf));
    (void)laplace_entity_pool_init(&ctx->entity_pool, &ctx->entity_arena,
                                   BENCH_GI_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&ctx->store, &ctx->store_arena,
                                   &ctx->entity_pool);
    (void)laplace_exec_init(&ctx->exec, &ctx->exec_arena, &ctx->store,
                            &ctx->entity_pool);
}

static void bench_gi_validate(void* const context) {
    bench_gi_ctx_t* const ctx = (bench_gi_ctx_t*)context;
    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&g_gi_artifact, &detail);
    ctx->sink_u32 = s;
}

static void bench_gi_entities(void* const context) {
    bench_gi_ctx_t* const ctx = (bench_gi_ctx_t*)context;
    bench_gi_init_base(ctx);

    /* Import the no-rule artifact (entities + facts) */
    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &ctx->store, &ctx->entity_pool, NULL);

    /* Build a minimal artifact with just entities */
    laplace_graph_artifact_t ent_art;
    memset(&ent_art, 0, sizeof(ent_art));
    ent_art.header.magic = LAPLACE_GRAPH_ARTIFACT_MAGIC;
    ent_art.header.version = LAPLACE_GRAPH_ARTIFACT_VERSION;
    ent_art.header.profile_id = LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES;
    ent_art.header.predicate_count = 1u;
    ent_art.header.entity_count = BENCH_GI_GRAPH_SIZE;
    ent_art.header.fact_count = 0u;
    ent_art.header.rule_count = 0u;
    ent_art.header.checksum = laplace_graph_artifact_compute_checksum(&ent_art.header);
    ent_art.predicates = g_gi_preds;
    ent_art.entities = g_gi_ents;

    laplace_graph_import_result_t result;
    (void)laplace_graph_import(&ictx, &ent_art, &result);
    ctx->sink_u32 = result.entities_imported;
}

static void bench_gi_facts(void* const context) {
    bench_gi_ctx_t* const ctx = (bench_gi_ctx_t*)context;
    bench_gi_init_base(ctx);

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &ctx->store, &ctx->entity_pool, NULL);

    laplace_graph_import_result_t result;
    (void)laplace_graph_import(&ictx, &g_gi_artifact, &result);
    ctx->sink_u32 = result.facts_imported;
}

static void bench_gi_end_to_end(void* const context) {
    bench_gi_ctx_t* const ctx = (bench_gi_ctx_t*)context;
    bench_gi_init_base(ctx);

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &ctx->store, &ctx->entity_pool, &ctx->exec);

    laplace_graph_import_result_t result;
    (void)laplace_graph_import(&ictx, &g_gi_artifact, &result);
    ctx->sink_u32 = result.facts_imported + result.entities_imported;
}

static void bench_gi_with_rules(void* const context) {
    bench_gi_ctx_t* const ctx = (bench_gi_ctx_t*)context;
    bench_gi_init_base(ctx);

    /* Build artifact with rule included */
    laplace_graph_artifact_t rule_art = g_gi_artifact;
    rule_art.header.rule_count = 1u;
    rule_art.header.flags = LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES;
    rule_art.header.checksum = laplace_graph_artifact_compute_checksum(&rule_art.header);
    rule_art.rules = g_gi_rules;

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &ctx->store, &ctx->entity_pool, &ctx->exec);

    laplace_graph_import_result_t result;
    (void)laplace_graph_import(&ictx, &rule_art, &result);
    ctx->sink_u32 = result.rules_imported + result.facts_imported;
}

void laplace_bench_graph_import(void) {
    bench_gi_build_fixture();

    const laplace_bench_case_t benches[] = {
        {"graph_artifact_validate",     bench_gi_validate,       &g_bench_gi, 100000u},
        {"graph_import_entities",       bench_gi_entities,       &g_bench_gi, 1000u},
        {"graph_import_facts",          bench_gi_facts,          &g_bench_gi, 1000u},
        {"graph_import_end_to_end",     bench_gi_end_to_end,     &g_bench_gi, 1000u},
        {"graph_import_with_rules",     bench_gi_with_rules,     &g_bench_gi, 1000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        (void)laplace_bench_run_case(&benches[i]);
    }
}
