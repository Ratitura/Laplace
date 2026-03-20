#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/arena.h"
#include "laplace/derive.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/observe.h"
#include "laplace/state.h"
#include "laplace/version.h"
#include "../test_harness.h"

#define TEST_DERIVE_ENTITY_CAPACITY 128u
#define TEST_DERIVE_ENTITY_ARENA    (TEST_DERIVE_ENTITY_CAPACITY * 96u)
#define TEST_DERIVE_STORE_ARENA     (2u * 1024u * 1024u)
#define TEST_DERIVE_EXEC_ARENA      (256u * 1024u)
#define TEST_DERIVE_OBSERVE_ARENA   (512u * 1024u)

static _Alignas(64) uint8_t g_derive_entity_buf[TEST_DERIVE_ENTITY_ARENA];
static _Alignas(64) uint8_t g_derive_store_buf[TEST_DERIVE_STORE_ARENA];
static _Alignas(64) uint8_t g_derive_exec_buf[TEST_DERIVE_EXEC_ARENA];
static _Alignas(64) uint8_t g_derive_observe_buf[TEST_DERIVE_OBSERVE_ARENA];

typedef struct test_derive_fixture {
    laplace_arena_t          entity_arena;
    laplace_arena_t          store_arena;
    laplace_arena_t          exec_arena;
    laplace_arena_t          observe_arena;
    laplace_entity_pool_t    entity_pool;
    laplace_exact_store_t    store;
    laplace_exec_context_t   exec;
    laplace_observe_context_t observe;
    laplace_derive_context_t derive;
} test_derive_fixture_t;

static int derive_fixture_init(test_derive_fixture_t* f) {
    memset(f, 0, sizeof(*f));
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->entity_arena,
        g_derive_entity_buf, sizeof(g_derive_entity_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->store_arena,
        g_derive_store_buf, sizeof(g_derive_store_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->exec_arena,
        g_derive_exec_buf, sizeof(g_derive_exec_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->observe_arena,
        g_derive_observe_buf, sizeof(g_derive_observe_buf)) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(
        &f->entity_pool, &f->entity_arena,
        TEST_DERIVE_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_store_init(
        &f->store, &f->store_arena, &f->entity_pool) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_init(
        &f->exec, &f->exec_arena, &f->store,
        &f->entity_pool) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_observe_init(
        &f->observe, &f->observe_arena, 42u) == LAPLACE_OK);
    laplace_observe_set_level(&f->observe, LAPLACE_OBSERVE_DEBUG);
    laplace_observe_set_mask(&f->observe,
        LAPLACE_OBSERVE_MASK_ALL | LAPLACE_OBSERVE_MASK_DERIVE);

    laplace_exact_bind_observe(&f->store, &f->observe);
    laplace_exec_bind_observe(&f->exec, &f->observe);

    laplace_derive_context_init(&f->derive,
        &f->store, &f->exec, NULL, &f->observe, NULL, NULL, NULL);
    return 0;
}

static laplace_derive_action_t derive_make_action(
    laplace_kernel_id_t          kernel,
    laplace_derive_action_kind_t action,
    uint64_t                     correlation_id)
{
    laplace_derive_action_t a;
    memset(&a, 0, sizeof(a));
    a.api_version    = LAPLACE_DERIVE_API_VERSION;
    a.kernel         = kernel;
    a.action         = action;
    a.correlation_id = correlation_id;
    return a;
}

static laplace_entity_handle_t derive_alloc_ready(test_derive_fixture_t* f) {
    laplace_entity_handle_t h = laplace_entity_pool_alloc(&f->entity_pool);
    if (h.id != LAPLACE_ENTITY_ID_INVALID) {
        laplace_entity_pool_set_state(&f->entity_pool, h, LAPLACE_STATE_READY);
    }
    return h;
}

static laplace_entity_handle_t derive_register_constant(
    test_derive_fixture_t* f, laplace_exact_type_id_t type_id)
{
    laplace_entity_handle_t h = derive_alloc_ready(f);
    if (h.id != LAPLACE_ENTITY_ID_INVALID) {
        laplace_exact_register_constant(&f->store, h, type_id, 0u);
    }
    return h;
}

static int test_derive_envelope_sizes(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_derive_action_t) == 672u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_derive_result_t) == 160u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_derive_entity_ref_t) == 8u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_derive_term_t) == 8u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_derive_literal_t) == 68u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_trace_derive_payload_t) == 16u);
    return 0;
}

static int test_derive_invalid_version(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL, LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES, 1001u);
    action.api_version = 999u;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_INVALID_VERSION);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_REJECT);
    LAPLACE_TEST_ASSERT(result.correlation_id == 1001u);
    return 0;
}

static int test_derive_invalid_kernel(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_INVALID, LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT, 1002u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_INVALID_KERNEL);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_REJECT);
    LAPLACE_TEST_ASSERT(result.correlation_id == 1002u);

    action.kernel = 200u;
    laplace_derive_dispatch(&f.derive, &action, &result);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_INVALID_KERNEL);
    return 0;
}

static int test_derive_unsupported_kernel(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_PROOF, LAPLACE_DERIVE_ACTION_PROOF_IMPORT, 2001u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL);
    LAPLACE_TEST_ASSERT(result.kernel == LAPLACE_KERNEL_PROOF);
    LAPLACE_TEST_ASSERT(result.correlation_id == 2001u);

    action = derive_make_action(
        LAPLACE_KERNEL_BITVECTOR, LAPLACE_DERIVE_ACTION_BV_VALIDATE_WITNESS, 2002u);
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL);
    LAPLACE_TEST_ASSERT(result.kernel == LAPLACE_KERNEL_BITVECTOR);
    LAPLACE_TEST_ASSERT(result.correlation_id == 2002u);
    return 0;
}

static int test_derive_unsupported_action(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL, 99u, 3001u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION);
    LAPLACE_TEST_ASSERT(result.correlation_id == 3001u);
    return 0;
}

static int test_derive_correlation_propagation(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const uint64_t corr_ids[] = {0u, 1u, UINT64_MAX, 0xDEADBEEFCAFE0001ULL};
    for (uint32_t i = 0; i < 4u; ++i) {
        laplace_derive_action_t action = derive_make_action(
            LAPLACE_KERNEL_RELATIONAL,
            LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES,
            corr_ids[i]);

        laplace_derive_result_t result;
        laplace_derive_dispatch(&f.derive, &action, &result);
        LAPLACE_TEST_ASSERT(result.correlation_id == corr_ids[i]);
    }
    return 0;
}

static int test_derive_query_capabilities(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES, 4001u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_CAPABILITY);

    const laplace_derive_capability_result_t* cap = &result.payload.capability;
    LAPLACE_TEST_ASSERT(cap->abi_version == LAPLACE_DERIVE_API_VERSION);
    LAPLACE_TEST_ASSERT(cap->kernel_version_major == laplace_version_major());
    LAPLACE_TEST_ASSERT(cap->kernel_version_minor == laplace_version_minor());
    LAPLACE_TEST_ASSERT(cap->kernel_version_patch == laplace_version_patch());
    LAPLACE_TEST_ASSERT(cap->max_predicates == LAPLACE_EXACT_MAX_PREDICATES);
    LAPLACE_TEST_ASSERT(cap->max_arity == LAPLACE_EXACT_MAX_ARITY);
    LAPLACE_TEST_ASSERT(cap->supported_kernels == (1u << LAPLACE_KERNEL_RELATIONAL));
    return 0;
}

static int test_derive_query_stats(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_QUERY_STATS, 4002u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_STATS);

    const laplace_derive_stats_result_t* s = &result.payload.stats;
    LAPLACE_TEST_ASSERT(s->predicate_count == 0u);
    LAPLACE_TEST_ASSERT(s->fact_count == 0u);
    LAPLACE_TEST_ASSERT(s->rule_count == 0u);
    LAPLACE_TEST_ASSERT(s->entity_capacity == TEST_DERIVE_ENTITY_CAPACITY);
    return 0;
}

static int test_derive_rel_assert_fact(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 1u, &desc) == LAPLACE_OK);

    laplace_entity_handle_t a = derive_register_constant(&f, 1u);
    laplace_entity_handle_t b = derive_register_constant(&f, 1u);
    LAPLACE_TEST_ASSERT(a.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(b.id != LAPLACE_ENTITY_ID_INVALID);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT, 5001u);
    action.payload.rel_assert_fact.predicate_id = 1u;
    action.payload.rel_assert_fact.arg_count    = 2u;
    action.payload.rel_assert_fact.flags        = 0u;
    action.payload.rel_assert_fact.args[0].id         = a.id;
    action.payload.rel_assert_fact.args[0].generation  = a.generation;
    action.payload.rel_assert_fact.args[1].id         = b.id;
    action.payload.rel_assert_fact.args[1].generation  = b.generation;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_REL_FACT);
    LAPLACE_TEST_ASSERT(result.payload.rel_fact.inserted == 1u);
    LAPLACE_TEST_ASSERT(result.payload.rel_fact.fact_row != 0u ||
                         result.payload.rel_fact.entity_id != 0u);
    LAPLACE_TEST_ASSERT(result.correlation_id == 5001u);

    laplace_entity_id_t ids[2] = {a.id, b.id};
    const laplace_exact_fact_row_t found =
        laplace_exact_find_fact(&f.store, 1u, ids, 2u);
    LAPLACE_TEST_ASSERT(found != LAPLACE_EXACT_FACT_ROW_INVALID);
    return 0;
}

static int test_derive_rel_lookup_fact(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 1u, &desc) == LAPLACE_OK);

    laplace_entity_handle_t a = derive_register_constant(&f, 1u);
    laplace_entity_handle_t b = derive_register_constant(&f, 1u);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_LOOKUP_FACT, 6001u);
    action.payload.rel_lookup_fact.predicate_id = 1u;
    action.payload.rel_lookup_fact.arg_count    = 2u;
    action.payload.rel_lookup_fact.args[0].id         = a.id;
    action.payload.rel_lookup_fact.args[0].generation  = a.generation;
    action.payload.rel_lookup_fact.args[1].id         = b.id;
    action.payload.rel_lookup_fact.args[1].generation  = b.generation;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_REL_LOOKUP);
    LAPLACE_TEST_ASSERT(result.payload.rel_lookup.found == 0u);

    laplace_provenance_id_t prov = LAPLACE_PROVENANCE_ID_INVALID;
    laplace_exact_provenance_desc_t pd = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED};
    laplace_exact_insert_provenance(&f.store, &pd, &prov);

    laplace_entity_handle_t handles[2] = {a, b};
    laplace_exact_fact_row_t fr = 0;
    laplace_entity_handle_t fe = {0, 0};
    bool ins = false;
    laplace_exact_assert_fact(&f.store, 1u, handles, 2u, prov,
        LAPLACE_EXACT_FACT_FLAG_ASSERTED | LAPLACE_EXACT_FACT_FLAG_COMMITTED,
        &fr, &fe, &ins);

    action.correlation_id = 6002u;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.payload.rel_lookup.found == 1u);
    LAPLACE_TEST_ASSERT(result.correlation_id == 6002u);
    return 0;
}

static int test_derive_rel_add_rule(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 1u, &desc) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 2u, &desc) == LAPLACE_OK);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_ADD_RULE, 7001u);

    laplace_derive_rel_add_rule_payload_t* rp = &action.payload.rel_add_rule;
    rp->body_count = 1u;

    rp->head.predicate_id = 2u;
    rp->head.arity        = 2u;
    rp->head.terms[0].kind  = 1u;
    rp->head.terms[0].value = 1u;
    rp->head.terms[1].kind  = 1u;
    rp->head.terms[1].value = 2u;

    rp->body[0].predicate_id = 1u;
    rp->body[0].arity        = 2u;
    rp->body[0].terms[0].kind  = 1u;
    rp->body[0].terms[0].value = 1u;
    rp->body[0].terms[1].kind  = 1u;
    rp->body[0].terms[1].value = 2u;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_REL_RULE);
    LAPLACE_TEST_ASSERT(result.payload.rel_rule.rule_id != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(result.payload.rel_rule.validation_error == 0u);
    return 0;
}

static int test_derive_rel_build_trigger_idx(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_BUILD_TRIGGER_IDX, 8001u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_ACK);
    return 0;
}

static int test_derive_rel_exec_run(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 1u, &desc) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 2u, &desc) == LAPLACE_OK);

    laplace_entity_handle_t a = derive_register_constant(&f, 1u);
    laplace_entity_handle_t b = derive_register_constant(&f, 1u);

    laplace_derive_action_t assert_action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT, 9000u);
    assert_action.payload.rel_assert_fact.predicate_id = 1u;
    assert_action.payload.rel_assert_fact.arg_count    = 2u;
    assert_action.payload.rel_assert_fact.args[0].id         = a.id;
    assert_action.payload.rel_assert_fact.args[0].generation  = a.generation;
    assert_action.payload.rel_assert_fact.args[1].id         = b.id;
    assert_action.payload.rel_assert_fact.args[1].generation  = b.generation;

    laplace_derive_result_t r;
    laplace_derive_dispatch(&f.derive, &assert_action, &r);
    LAPLACE_TEST_ASSERT(r.status == LAPLACE_DERIVE_STATUS_OK);

    laplace_derive_action_t rule_action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_ADD_RULE, 9001u);

    laplace_derive_rel_add_rule_payload_t* rp = &rule_action.payload.rel_add_rule;
    rp->body_count = 1u;
    rp->head.predicate_id = 2u;
    rp->head.arity        = 2u;
    rp->head.terms[0].kind  = 1u; rp->head.terms[0].value = 1u;
    rp->head.terms[1].kind  = 1u; rp->head.terms[1].value = 2u;
    rp->body[0].predicate_id = 1u;
    rp->body[0].arity        = 2u;
    rp->body[0].terms[0].kind  = 1u; rp->body[0].terms[0].value = 1u;
    rp->body[0].terms[1].kind  = 1u; rp->body[0].terms[1].value = 2u;

    laplace_derive_dispatch(&f.derive, &rule_action, &r);
    LAPLACE_TEST_ASSERT(r.status == LAPLACE_DERIVE_STATUS_OK);

    laplace_derive_action_t run_action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_EXEC_RUN, 9002u);
    run_action.payload.rel_exec_run.max_steps       = 100u;
    run_action.payload.rel_exec_run.max_derivations  = 100u;
    run_action.payload.rel_exec_run.mode             = 0u;
    run_action.payload.rel_exec_run.semi_naive       = 1u;

    laplace_derive_dispatch(&f.derive, &run_action, &r);

    LAPLACE_TEST_ASSERT(r.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(r.result_kind == LAPLACE_DERIVE_RESULT_REL_EXEC);
    LAPLACE_TEST_ASSERT(r.correlation_id == 9002u);

    laplace_entity_id_t ids[2] = {a.id, b.id};
    const laplace_exact_fact_row_t found =
        laplace_exact_find_fact(&f.store, 2u, ids, 2u);
    LAPLACE_TEST_ASSERT(found != LAPLACE_EXACT_FACT_ROW_INVALID);
    return 0;
}

static int test_derive_result_equivalence(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(
        &f.store, 1u, &desc) == LAPLACE_OK);

    laplace_entity_handle_t c1 = derive_register_constant(&f, 1u);
    laplace_entity_handle_t c2 = derive_register_constant(&f, 1u);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT, 10001u);
    action.payload.rel_assert_fact.predicate_id = 1u;
    action.payload.rel_assert_fact.arg_count    = 2u;
    action.payload.rel_assert_fact.args[0].id         = c1.id;
    action.payload.rel_assert_fact.args[0].generation  = c1.generation;
    action.payload.rel_assert_fact.args[1].id         = c2.id;
    action.payload.rel_assert_fact.args[1].generation  = c2.generation;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);

    laplace_entity_id_t ids[2] = {c1.id, c2.id};
    const laplace_exact_fact_row_t direct_row =
        laplace_exact_find_fact(&f.store, 1u, ids, 2u);

    LAPLACE_TEST_ASSERT(direct_row != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT((uint32_t)direct_row == result.payload.rel_fact.fact_row);

    laplace_derive_action_t stats_action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_QUERY_STATS, 10002u);
    laplace_derive_dispatch(&f.derive, &stats_action, &result);
    LAPLACE_TEST_ASSERT(result.payload.stats.fact_count == f.store.fact_count);
    LAPLACE_TEST_ASSERT(result.payload.stats.predicate_count == f.store.predicate_count);
    return 0;
}

static int test_derive_trace_emission(void) {
    test_derive_fixture_t f;
    LAPLACE_TEST_ASSERT(derive_fixture_init(&f) == 0);

    const uint32_t initial_count = laplace_trace_count(&f.observe.trace);

    laplace_derive_action_t action = derive_make_action(
        LAPLACE_KERNEL_RELATIONAL,
        LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES, 11001u);

    laplace_derive_result_t result;
    laplace_derive_dispatch(&f.derive, &action, &result);

    const uint32_t after_count = laplace_trace_count(&f.observe.trace);
    LAPLACE_TEST_ASSERT(after_count > initial_count);

    const laplace_trace_record_t* rec =
        laplace_trace_get(&f.observe.trace, after_count - 1u);
    LAPLACE_TEST_ASSERT(rec != NULL);
    LAPLACE_TEST_ASSERT(rec->kind == (uint16_t)LAPLACE_TRACE_KIND_DERIVE_DISPATCH);
    LAPLACE_TEST_ASSERT(rec->subsystem == (uint8_t)LAPLACE_TRACE_SUBSYSTEM_DERIVE);
    LAPLACE_TEST_ASSERT(rec->correlation_id == 11001u);
    return 0;
}

static int test_derive_string_helpers(void) {
    LAPLACE_TEST_ASSERT(laplace_derive_status_string(LAPLACE_DERIVE_STATUS_OK) != NULL);
    LAPLACE_TEST_ASSERT(laplace_derive_status_string(LAPLACE_DERIVE_STATUS_OK)[0] == 'o');
    LAPLACE_TEST_ASSERT(laplace_derive_status_string(255u) != NULL);

    LAPLACE_TEST_ASSERT(laplace_derive_action_string(LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES) != NULL);
    LAPLACE_TEST_ASSERT(laplace_derive_action_string(255u) != NULL);

    LAPLACE_TEST_ASSERT(laplace_derive_result_kind_string(LAPLACE_DERIVE_RESULT_ACK) != NULL);
    LAPLACE_TEST_ASSERT(laplace_derive_result_kind_string(255u) != NULL);
    return 0;
}

int laplace_test_derive(void) {
    int failures = 0;
    failures += test_derive_envelope_sizes();
    failures += test_derive_invalid_version();
    failures += test_derive_invalid_kernel();
    failures += test_derive_unsupported_kernel();
    failures += test_derive_unsupported_action();
    failures += test_derive_correlation_propagation();
    failures += test_derive_query_capabilities();
    failures += test_derive_query_stats();
    failures += test_derive_rel_assert_fact();
    failures += test_derive_rel_lookup_fact();
    failures += test_derive_rel_add_rule();
    failures += test_derive_rel_build_trigger_idx();
    failures += test_derive_rel_exec_run();
    failures += test_derive_result_equivalence();
    failures += test_derive_trace_emission();
    failures += test_derive_string_helpers();
    return failures;
}
