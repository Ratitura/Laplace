#include "test_harness.h"
#include "laplace/graph_artifact.h"
#include "laplace/graph_import.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/state.h"

#include <string.h>

#define TEST_GA_ENTITY_CAPACITY     512u
#define TEST_GA_ENTITY_ARENA_BYTES  (TEST_GA_ENTITY_CAPACITY * 96u)
#define TEST_GA_STORE_ARENA_BYTES   (2u * 1024u * 1024u)
#define TEST_GA_EXEC_ARENA_BYTES    (512u * 1024u)

static _Alignas(64) uint8_t g_test_ga_entity_buf[TEST_GA_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_test_ga_store_buf[TEST_GA_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_test_ga_exec_buf[TEST_GA_EXEC_ARENA_BYTES];

typedef struct test_ga_ctx {
    laplace_arena_t         entity_arena;
    laplace_arena_t         store_arena;
    laplace_arena_t         exec_arena;
    laplace_entity_pool_t   entity_pool;
    laplace_exact_store_t   store;
    laplace_exec_context_t  exec;
} test_ga_ctx_t;

static test_ga_ctx_t g_ctx;

static void test_ga_init(test_ga_ctx_t* const ctx) {
    memset(ctx, 0, sizeof(*ctx));
    (void)laplace_arena_init(&ctx->entity_arena, g_test_ga_entity_buf,
                             sizeof(g_test_ga_entity_buf));
    (void)laplace_arena_init(&ctx->store_arena, g_test_ga_store_buf,
                             sizeof(g_test_ga_store_buf));
    (void)laplace_arena_init(&ctx->exec_arena, g_test_ga_exec_buf,
                             sizeof(g_test_ga_exec_buf));
    (void)laplace_entity_pool_init(&ctx->entity_pool, &ctx->entity_arena,
                                   TEST_GA_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&ctx->store, &ctx->store_arena,
                                   &ctx->entity_pool);
    (void)laplace_exec_init(&ctx->exec, &ctx->exec_arena, &ctx->store,
                            &ctx->entity_pool);
}

/* Helper: build a minimal valid header */
static laplace_graph_artifact_header_t make_valid_header(
    const uint32_t npred, const uint32_t nent,
    const uint32_t nfact, const uint32_t nrule)
{
    laplace_graph_artifact_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic           = LAPLACE_GRAPH_ARTIFACT_MAGIC;
    hdr.version         = LAPLACE_GRAPH_ARTIFACT_VERSION;
    hdr.profile_id      = LAPLACE_GRAPH_PROFILE_HORN_CLOSURE;
    hdr.predicate_count = npred;
    hdr.entity_count    = nent;
    hdr.fact_count      = nfact;
    hdr.rule_count      = nrule;
    if (nrule > 0u) {
        hdr.flags = LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES;
    }
    hdr.checksum = laplace_graph_artifact_compute_checksum(&hdr);
    return hdr;
}

/* 1. valid artifact with predicates, entities, facts */
static int test_valid_artifact(void) {
    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;
    preds[1].local_id = 1u; preds[1].arity = 2u;

    laplace_graph_artifact_entity_t ents[3];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;
    ents[1].local_id = 20u;
    ents[2].local_id = 30u;

    laplace_graph_artifact_fact_t facts[2];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 1u;
    facts[0].arg_entity_local_ids[0] = 10u;
    facts[1].predicate_local_id = 1u; facts[1].arg_count = 2u;
    facts[1].arg_entity_local_ids[0] = 10u;
    facts[1].arg_entity_local_ids[1] = 20u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 3u, 2u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_OK);
    return 0;
}

/* 2. bad magic */
static int test_bad_magic(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);
    art.header.magic = 0xDEADBEEFu;
    art.header.checksum = 0u; /* skip checksum check */

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_BAD_MAGIC);
    return 0;
}

/* 3. bad version */
static int test_bad_version(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);
    art.header.version = 99u;
    art.header.checksum = 0u;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_BAD_VERSION);
    return 0;
}

/* 4. bad profile */
static int test_bad_profile(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);
    art.header.profile_id = LAPLACE_GRAPH_PROFILE_INVALID;
    art.header.checksum = 0u;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_BAD_PROFILE);
    return 0;
}

/* 5. predicate overflow */
static int test_predicate_overflow(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);
    art.header.predicate_count = LAPLACE_GRAPH_ARTIFACT_MAX_PREDICATES + 1u;
    art.header.checksum = 0u;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_OVERFLOW);
    return 0;
}

/* 6. entity overflow */
static int test_entity_overflow(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);
    art.header.entity_count = LAPLACE_GRAPH_ARTIFACT_MAX_ENTITIES + 1u;
    art.header.checksum = 0u;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_OVERFLOW);
    return 0;
}

/* 7. duplicate predicate local IDs */
static int test_predicate_duplicate(void) {
    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 5u; preds[0].arity = 2u;
    preds[1].local_id = 5u; preds[1].arity = 2u; /* duplicate! */

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 0u, 0u, 0u);
    art.predicates = preds;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_DUPLICATE);
    LAPLACE_TEST_ASSERT(detail.record_index == 1u);
    return 0;
}

/* 8. duplicate entity local IDs */
static int test_entity_duplicate(void) {
    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;

    laplace_graph_artifact_entity_t ents[2];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 42u;
    ents[1].local_id = 42u; /* duplicate! */

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 2u, 0u, 0u);
    art.predicates = preds;
    art.entities   = ents;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_DUPLICATE);
    LAPLACE_TEST_ASSERT(detail.record_index == 1u);
    return 0;
}

/* 9. bad predicate arity (zero) */
static int test_predicate_zero_arity(void) {
    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 0u; /* invalid! */

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 0u, 0u, 0u);
    art.predicates = preds;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_ARITY);
    return 0;
}

/* 10. fact references undeclared predicate */
static int test_fact_bad_pred_ref(void) {
    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;

    laplace_graph_artifact_entity_t ents[1];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;

    laplace_graph_artifact_fact_t facts[1];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 99u; /* not declared! */
    facts[0].arg_count = 1u;
    facts[0].arg_entity_local_ids[0] = 10u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 1u, 1u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_FACT_PREDICATE_REF);
    return 0;
}

/* 11. fact arity mismatch */
static int test_fact_arity_mismatch(void) {
    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;

    laplace_graph_artifact_entity_t ents[1];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;

    laplace_graph_artifact_fact_t facts[1];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u;
    facts[0].arg_count = 1u; /* should be 2! */
    facts[0].arg_entity_local_ids[0] = 10u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 1u, 1u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ARITY_MISMATCH);
    return 0;
}

/* 12. fact references undeclared entity */
static int test_fact_bad_entity_ref(void) {
    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;

    laplace_graph_artifact_entity_t ents[1];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;

    laplace_graph_artifact_fact_t facts[1];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u;
    facts[0].arg_count = 1u;
    facts[0].arg_entity_local_ids[0] = 99u; /* not declared! */

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 1u, 1u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ENTITY_REF);
    return 0;
}

/* 13. rules without flag */
static int test_rules_without_flag(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 1u);
    art.header.flags = 0u; /* no HAS_RULES flag */
    art.header.checksum = 0u;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_RULES_WITHOUT_FLAG);
    return 0;
}

/* 14. rules not supported by BASIC_TRIPLES profile */
static int test_rules_not_supported(void) {
    laplace_graph_artifact_rule_t rules[1];
    memset(rules, 0, sizeof(rules));

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 1u);
    art.header.profile_id = LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES;
    art.header.flags = LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES;
    art.header.checksum = 0u;
    art.rules = rules;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_RULES_NOT_SUPPORTED);
    return 0;
}

/* 15. null artifact */
static int test_null_artifact(void) {
    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(NULL, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_NULL);
    return 0;
}

/* 16. status strings are non-null */
static int test_status_strings(void) {
    for (uint32_t i = 0u; i < LAPLACE_GRAPH_ARTIFACT_STATUS_COUNT_; ++i) {
        const char* const str =
            laplace_graph_artifact_status_string((laplace_graph_artifact_status_t)i);
        LAPLACE_TEST_ASSERT(str != NULL);
        LAPLACE_TEST_ASSERT(str[0] != '\0');
    }
    return 0;
}

/* 17. checksum computation is deterministic */
static int test_checksum_deterministic(void) {
    laplace_graph_artifact_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic           = LAPLACE_GRAPH_ARTIFACT_MAGIC;
    hdr.version         = LAPLACE_GRAPH_ARTIFACT_VERSION;
    hdr.profile_id      = LAPLACE_GRAPH_PROFILE_HORN_CLOSURE;
    hdr.predicate_count = 5u;
    hdr.entity_count    = 10u;
    hdr.fact_count      = 20u;
    hdr.rule_count      = 2u;
    hdr.flags           = LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES;

    const uint32_t c1 = laplace_graph_artifact_compute_checksum(&hdr);
    const uint32_t c2 = laplace_graph_artifact_compute_checksum(&hdr);
    LAPLACE_TEST_ASSERT(c1 == c2);
    LAPLACE_TEST_ASSERT(c1 != 0u);
    return 0;
}

/* 18. import valid artifact (facts only) */
static int test_import_facts_only(void) {
    test_ga_init(&g_ctx);

    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;
    preds[1].local_id = 1u; preds[1].arity = 2u;

    laplace_graph_artifact_entity_t ents[3];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 100u;
    ents[1].local_id = 200u;
    ents[2].local_id = 300u;

    laplace_graph_artifact_fact_t facts[3];
    memset(facts, 0, sizeof(facts));
    /* unary: type(e0) */
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 1u;
    facts[0].arg_entity_local_ids[0] = 100u;
    /* binary: edge(e0, e1) */
    facts[1].predicate_local_id = 1u; facts[1].arg_count = 2u;
    facts[1].arg_entity_local_ids[0] = 100u;
    facts[1].arg_entity_local_ids[1] = 200u;
    /* binary: edge(e1, e2) */
    facts[2].predicate_local_id = 1u; facts[2].arg_count = 2u;
    facts[2].arg_entity_local_ids[0] = 200u;
    facts[2].arg_entity_local_ids[1] = 300u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 3u, 3u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &g_ctx.store, &g_ctx.entity_pool, &g_ctx.exec);

    laplace_graph_import_result_t result;
    const laplace_graph_import_status_t s = laplace_graph_import(&ictx, &art, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_IMPORT_OK);
    LAPLACE_TEST_ASSERT(result.predicates_imported == 2u);
    LAPLACE_TEST_ASSERT(result.entities_imported == 3u);
    LAPLACE_TEST_ASSERT(result.facts_imported == 3u);
    LAPLACE_TEST_ASSERT(result.facts_deduplicated == 0u);
    LAPLACE_TEST_ASSERT(result.rules_imported == 0u);
    return 0;
}

/* 19. import duplicate facts (dedup accounting) */
static int test_import_duplicate_facts(void) {
    test_ga_init(&g_ctx);

    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;

    laplace_graph_artifact_entity_t ents[2];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 1u;
    ents[1].local_id = 2u;

    /* Two identical facts */
    laplace_graph_artifact_fact_t facts[2];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 2u;
    facts[0].arg_entity_local_ids[0] = 1u;
    facts[0].arg_entity_local_ids[1] = 2u;
    facts[1].predicate_local_id = 0u; facts[1].arg_count = 2u;
    facts[1].arg_entity_local_ids[0] = 1u;
    facts[1].arg_entity_local_ids[1] = 2u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 2u, 2u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &g_ctx.store, &g_ctx.entity_pool, &g_ctx.exec);

    laplace_graph_import_result_t result;
    const laplace_graph_import_status_t s = laplace_graph_import(&ictx, &art, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_IMPORT_OK);
    LAPLACE_TEST_ASSERT(result.facts_imported == 1u);
    LAPLACE_TEST_ASSERT(result.facts_deduplicated == 1u);
    return 0;
}

/* 20. import with rules */
static int test_import_with_rules(void) {
    test_ga_init(&g_ctx);

    /* pred 0: binary edge;  pred 1: binary tc */
    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;
    preds[1].local_id = 1u; preds[1].arity = 2u;

    laplace_graph_artifact_entity_t ents[3];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;
    ents[1].local_id = 20u;
    ents[2].local_id = 30u;

    /* edge(10,20), edge(20,30) */
    laplace_graph_artifact_fact_t facts[2];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 2u;
    facts[0].arg_entity_local_ids[0] = 10u;
    facts[0].arg_entity_local_ids[1] = 20u;
    facts[1].predicate_local_id = 0u; facts[1].arg_count = 2u;
    facts[1].arg_entity_local_ids[0] = 20u;
    facts[1].arg_entity_local_ids[1] = 30u;

    /* rule: edge(X,Y) => tc(X,Y) */
    laplace_graph_artifact_rule_t rules[1];
    memset(rules, 0, sizeof(rules));
    rules[0].body_count = 1u;
    rules[0].head.predicate_local_id = 1u;
    rules[0].head.arity = 2u;
    rules[0].head.terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].head.terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};
    rules[0].body[0].predicate_local_id = 0u;
    rules[0].body[0].arity = 2u;
    rules[0].body[0].terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].body[0].terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 3u, 2u, 1u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;
    art.rules      = rules;

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &g_ctx.store, &g_ctx.entity_pool, &g_ctx.exec);

    laplace_graph_import_result_t result;
    const laplace_graph_import_status_t s = laplace_graph_import(&ictx, &art, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_IMPORT_OK);
    LAPLACE_TEST_ASSERT(result.predicates_imported == 2u);
    LAPLACE_TEST_ASSERT(result.entities_imported == 3u);
    LAPLACE_TEST_ASSERT(result.facts_imported == 2u);
    LAPLACE_TEST_ASSERT(result.rules_imported == 1u);
    LAPLACE_TEST_ASSERT(result.rules_rejected == 0u);
    return 0;
}

/* 21. deterministic repeated import gives same result */
static int test_import_deterministic(void) {
    test_ga_init(&g_ctx);

    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 1u;

    laplace_graph_artifact_entity_t ents[2];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 1u;
    ents[1].local_id = 2u;

    laplace_graph_artifact_fact_t facts[2];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 1u;
    facts[0].arg_entity_local_ids[0] = 1u;
    facts[1].predicate_local_id = 0u; facts[1].arg_count = 1u;
    facts[1].arg_entity_local_ids[0] = 2u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 2u, 2u, 0u);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    /* First import */
    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &g_ctx.store, &g_ctx.entity_pool, &g_ctx.exec);
    laplace_graph_import_result_t r1;
    (void)laplace_graph_import(&ictx, &art, &r1);

    /* Second import of same artifact — predicates already registered,
       entities are new allocations, facts should dedup */
    laplace_graph_import_result_t r2;
    (void)laplace_graph_import(&ictx, &art, &r2);

    LAPLACE_TEST_ASSERT(r1.status == LAPLACE_GRAPH_IMPORT_OK);
    LAPLACE_TEST_ASSERT(r2.status == LAPLACE_GRAPH_IMPORT_OK);
    /* Second import: predicates already declared -> still counted as imported */
    LAPLACE_TEST_ASSERT(r2.predicates_imported == 1u);
    /* Second import: new entities allocated */
    LAPLACE_TEST_ASSERT(r2.entities_imported == 2u);
    /* Second import: facts use new entity handles so they're new facts, not deduped */
    LAPLACE_TEST_ASSERT(r2.facts_imported == 2u);
    return 0;
}

/* 22. import null context */
static int test_import_null(void) {
    laplace_graph_import_result_t result;
    const laplace_graph_import_status_t s = laplace_graph_import(NULL, NULL, &result);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_IMPORT_ERR_NULL);
    return 0;
}

/* 23. profile support query */
static int test_import_profile_support(void) {
    LAPLACE_TEST_ASSERT(laplace_graph_import_supports_profile(LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES));
    LAPLACE_TEST_ASSERT(laplace_graph_import_supports_profile(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE));
    LAPLACE_TEST_ASSERT(!laplace_graph_import_supports_profile(LAPLACE_GRAPH_PROFILE_INVALID));
    LAPLACE_TEST_ASSERT(!laplace_graph_import_supports_profile(99u));
    return 0;
}

/* 24. import status strings are non-null */
static int test_import_status_strings(void) {
    for (uint32_t i = 0u; i < LAPLACE_GRAPH_IMPORT_STATUS_COUNT_; ++i) {
        const char* const str =
            laplace_graph_import_status_string((laplace_graph_import_status_t)i);
        LAPLACE_TEST_ASSERT(str != NULL);
        LAPLACE_TEST_ASSERT(str[0] != '\0');
    }
    return 0;
}

/* 25. rule with Datalog safety violation */
static int test_rule_variable_safety(void) {
    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;
    preds[1].local_id = 1u; preds[1].arity = 2u;

    laplace_graph_artifact_entity_t ents[1];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;

    /* rule: body has edge(X,Y), head has tc(X,Z) — Z not in body! */
    laplace_graph_artifact_rule_t rules[1];
    memset(rules, 0, sizeof(rules));
    rules[0].body_count = 1u;
    rules[0].head.predicate_local_id = 1u;
    rules[0].head.arity = 2u;
    rules[0].head.terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].head.terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 3u}; /* Z not in body */
    rules[0].body[0].predicate_local_id = 0u;
    rules[0].body[0].arity = 2u;
    rules[0].body[0].terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].body[0].terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 1u, 0u, 1u);
    art.predicates = preds;
    art.entities   = ents;
    art.rules      = rules;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_ERR_RULE_VARIABLE_SAFETY);
    return 0;
}

/* 26. valid rule passes validation */
static int test_valid_rule(void) {
    laplace_graph_artifact_predicate_t preds[2];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;
    preds[1].local_id = 1u; preds[1].arity = 2u;

    laplace_graph_artifact_entity_t ents[1];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 10u;

    /* rule: edge(X,Y) => tc(X,Y) — valid */
    laplace_graph_artifact_rule_t rules[1];
    memset(rules, 0, sizeof(rules));
    rules[0].body_count = 1u;
    rules[0].head.predicate_local_id = 1u;
    rules[0].head.arity = 2u;
    rules[0].head.terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].head.terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};
    rules[0].body[0].predicate_local_id = 0u;
    rules[0].body[0].arity = 2u;
    rules[0].body[0].terms[0] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 1u};
    rules[0].body[0].terms[1] = (laplace_graph_artifact_term_t){.kind = 1u, .value = 2u};

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(2u, 1u, 0u, 1u);
    art.predicates = preds;
    art.entities   = ents;
    art.rules      = rules;

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_OK);
    return 0;
}

/* 27. empty artifact (zero predicates, entities, facts, rules) is valid */
static int test_empty_artifact(void) {
    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header = make_valid_header(0u, 0u, 0u, 0u);

    laplace_graph_artifact_validation_t detail;
    const laplace_graph_artifact_status_t s =
        laplace_graph_artifact_validate(&art, &detail);
    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_ARTIFACT_OK);
    return 0;
}

/* 28. import with BASIC_TRIPLES profile (no rules) */
static int test_import_basic_triples(void) {
    test_ga_init(&g_ctx);

    laplace_graph_artifact_predicate_t preds[1];
    memset(preds, 0, sizeof(preds));
    preds[0].local_id = 0u; preds[0].arity = 2u;

    laplace_graph_artifact_entity_t ents[2];
    memset(ents, 0, sizeof(ents));
    ents[0].local_id = 1u;
    ents[1].local_id = 2u;

    laplace_graph_artifact_fact_t facts[1];
    memset(facts, 0, sizeof(facts));
    facts[0].predicate_local_id = 0u; facts[0].arg_count = 2u;
    facts[0].arg_entity_local_ids[0] = 1u;
    facts[0].arg_entity_local_ids[1] = 2u;

    laplace_graph_artifact_t art;
    memset(&art, 0, sizeof(art));
    art.header     = make_valid_header(1u, 2u, 1u, 0u);
    art.header.profile_id = LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES;
    art.header.checksum = laplace_graph_artifact_compute_checksum(&art.header);
    art.predicates = preds;
    art.entities   = ents;
    art.facts      = facts;

    laplace_graph_import_context_t ictx;
    laplace_graph_import_context_init(&ictx, &g_ctx.store, &g_ctx.entity_pool, &g_ctx.exec);

    laplace_graph_import_result_t result;
    const laplace_graph_import_status_t s = laplace_graph_import(&ictx, &art, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_GRAPH_IMPORT_OK);
    LAPLACE_TEST_ASSERT(result.predicates_imported == 1u);
    LAPLACE_TEST_ASSERT(result.entities_imported == 2u);
    LAPLACE_TEST_ASSERT(result.facts_imported == 1u);
    return 0;
}

int laplace_test_graph_artifact(void) {
    typedef int (*test_fn)(void);
    const struct { const char* name; test_fn fn; } tests[] = {
        {"valid_artifact",           test_valid_artifact},
        {"bad_magic",                test_bad_magic},
        {"bad_version",              test_bad_version},
        {"bad_profile",              test_bad_profile},
        {"predicate_overflow",       test_predicate_overflow},
        {"entity_overflow",          test_entity_overflow},
        {"predicate_duplicate",      test_predicate_duplicate},
        {"entity_duplicate",         test_entity_duplicate},
        {"predicate_zero_arity",     test_predicate_zero_arity},
        {"fact_bad_pred_ref",        test_fact_bad_pred_ref},
        {"fact_arity_mismatch",      test_fact_arity_mismatch},
        {"fact_bad_entity_ref",      test_fact_bad_entity_ref},
        {"rules_without_flag",       test_rules_without_flag},
        {"rules_not_supported",      test_rules_not_supported},
        {"null_artifact",            test_null_artifact},
        {"status_strings",           test_status_strings},
        {"checksum_deterministic",   test_checksum_deterministic},
        {"import_facts_only",        test_import_facts_only},
        {"import_duplicate_facts",   test_import_duplicate_facts},
        {"import_with_rules",        test_import_with_rules},
        {"import_deterministic",     test_import_deterministic},
        {"import_null",              test_import_null},
        {"import_profile_support",   test_import_profile_support},
        {"import_status_strings",    test_import_status_strings},
        {"rule_variable_safety",     test_rule_variable_safety},
        {"valid_rule",               test_valid_rule},
        {"empty_artifact",           test_empty_artifact},
        {"import_basic_triples",     test_import_basic_triples},
    };

    const size_t count = sizeof(tests) / sizeof(tests[0]);
    int failures = 0;

    for (size_t i = 0u; i < count; ++i) {
        const int r = tests[i].fn();
        if (r != 0) {
            fprintf(stderr, "  [FAIL] graph_artifact/%s\n", tests[i].name);
            ++failures;
        }
    }

    return failures;
}
