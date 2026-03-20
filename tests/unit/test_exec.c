#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/state.h"
#include "test_harness.h"

#define TEST_EXEC_ENTITY_CAPACITY 256u
#define TEST_EXEC_ENTITY_ARENA_BYTES (TEST_EXEC_ENTITY_CAPACITY * 96u)
#define TEST_EXEC_STORE_ARENA_BYTES  (2u * 1024u * 1024u)
#define TEST_EXEC_EXEC_ARENA_BYTES   (512u * 1024u)

static _Alignas(64) uint8_t g_exec_entity_buf[TEST_EXEC_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_exec_store_buf[TEST_EXEC_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_exec_exec_buf[TEST_EXEC_EXEC_ARENA_BYTES];

typedef struct test_exec_fixture {
    laplace_arena_t          entity_arena;
    laplace_arena_t          store_arena;
    laplace_arena_t          exec_arena;
    laplace_entity_pool_t    entity_pool;
    laplace_exact_store_t    store;
    laplace_exec_context_t   exec;
} test_exec_fixture_t;

static int exec_fixture_init(test_exec_fixture_t* const f) {
    memset(f, 0, sizeof(*f));
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->entity_arena, g_exec_entity_buf, sizeof(g_exec_entity_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->store_arena, g_exec_store_buf, sizeof(g_exec_store_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->exec_arena, g_exec_exec_buf, sizeof(g_exec_exec_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&f->entity_pool, &f->entity_arena, TEST_EXEC_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_store_init(&f->store, &f->store_arena, &f->entity_pool) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_init(&f->exec, &f->exec_arena, &f->store, &f->entity_pool) == LAPLACE_OK);
    return 0;
}

static laplace_entity_handle_t exec_alloc_ready(test_exec_fixture_t* const f) {
    laplace_entity_handle_t h = laplace_entity_pool_alloc(&f->entity_pool);
    if (h.id != LAPLACE_ENTITY_ID_INVALID) {
        (void)laplace_entity_pool_set_state(&f->entity_pool, h, LAPLACE_STATE_READY);
    }
    return h;
}

static laplace_entity_handle_t exec_register_constant(test_exec_fixture_t* const f,
                                                       const laplace_exact_type_id_t type_id) {
    laplace_entity_handle_t h = exec_alloc_ready(f);
    if (h.id == LAPLACE_ENTITY_ID_INVALID) {
        return h;
    }
    if (laplace_exact_register_constant(&f->store, h, type_id, 0u) != LAPLACE_OK) {
        h.id = LAPLACE_ENTITY_ID_INVALID;
        h.generation = LAPLACE_GENERATION_INVALID;
    }
    return h;
}

static laplace_provenance_id_t exec_insert_asserted_provenance(test_exec_fixture_t* const f) {
    laplace_provenance_id_t prov = LAPLACE_PROVENANCE_ID_INVALID;
    const laplace_exact_provenance_desc_t desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    (void)laplace_exact_insert_provenance(&f->store, &desc, &prov);
    return prov;
}

static laplace_entity_handle_t exec_assert_binary_fact(test_exec_fixture_t* const f,
                                                        const laplace_predicate_id_t predicate,
                                                        const laplace_entity_handle_t arg0,
                                                        const laplace_entity_handle_t arg1,
                                                        const laplace_provenance_id_t prov) {
    const laplace_entity_handle_t args[2] = {arg0, arg1};
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    bool inserted = false;
    (void)laplace_exact_assert_fact(&f->store, predicate, args, 2u, prov,
                                     LAPLACE_EXACT_FACT_FLAG_ASSERTED, &row, &fact_entity, &inserted);
    return fact_entity;
}

static laplace_rule_id_t exec_add_rule(test_exec_fixture_t* const f,
                                        const laplace_exact_rule_desc_t* const desc) {
    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t validation;
    (void)laplace_exact_add_rule(&f->store, desc, &rule_id, &validation);
    return rule_id;
}

static int test_exec_ready_marking(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred_desc = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred_desc) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    const laplace_entity_handle_t fact_entity = exec_assert_binary_fact(&f, 1u, a, b, prov);
    LAPLACE_TEST_ASSERT(fact_entity.id != LAPLACE_ENTITY_ID_INVALID);

    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 0u);
    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, fact_entity.id) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 1u);

    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, fact_entity.id) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 1u);

    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, LAPLACE_ENTITY_ID_INVALID) == LAPLACE_ERR_INVALID_ARGUMENT);

    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, a.id) == LAPLACE_ERR_INVALID_STATE);

    return 0;
}

static int test_exec_mark_all_facts(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred_desc = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred_desc) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t c = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, a, b, prov);
    (void)exec_assert_binary_fact(&f, 1u, b, c, prov);

    const uint32_t marked = laplace_exec_mark_all_facts_ready(&f.exec);
    LAPLACE_TEST_ASSERT(marked == 2u);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 2u);

    return 0;
}

static int test_exec_trigger_index(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 3u, &pred2) == LAPLACE_OK);

    laplace_exact_literal_t body[2];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    body[1].predicate = 1u; body[1].arity = 2u;
    body[1].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    body[1].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 3u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 2u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(f.exec.trigger_counts[1] == 2u);
    LAPLACE_TEST_ASSERT(f.exec.trigger_counts[3] == 0u);

    return 0;
}

static int test_exec_single_rule_derivation_dense(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t alice = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t bob   = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov  = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, alice, bob, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup_args[2] = {alice.id, bob.id};
    const laplace_exact_fact_row_t derived_row = laplace_exact_find_fact(&f.store, 2u, lookup_args, 2u);
    LAPLACE_TEST_ASSERT(derived_row != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exact_fact_t* const derived_fact = laplace_exact_get_fact(&f.store, derived_row);
    LAPLACE_TEST_ASSERT(derived_fact != NULL);
    LAPLACE_TEST_ASSERT((derived_fact->flags & LAPLACE_EXACT_FACT_FLAG_DERIVED) != 0u);
    LAPLACE_TEST_ASSERT((derived_fact->flags & LAPLACE_EXACT_FACT_FLAG_COMMITTED) != 0u);
    LAPLACE_TEST_ASSERT((derived_fact->flags & LAPLACE_EXACT_FACT_FLAG_BRANCH_LOCAL) == 0u);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 1u);
    LAPLACE_TEST_ASSERT(stats->provenance_created >= 1u);

    const laplace_exact_provenance_record_t* const prov_rec = laplace_exact_get_provenance(&f.store, derived_fact->provenance);
    LAPLACE_TEST_ASSERT(prov_rec != NULL);
    LAPLACE_TEST_ASSERT(prov_rec->kind == LAPLACE_EXACT_PROVENANCE_DERIVED);

    return 0;
}

static int test_exec_single_rule_derivation_sparse(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t alice = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t bob   = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov  = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, alice, bob, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_SPARSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup_args[2] = {alice.id, bob.id};
    const laplace_exact_fact_row_t derived_row = laplace_exact_find_fact(&f.store, 2u, lookup_args, 2u);
    LAPLACE_TEST_ASSERT(derived_row != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 1u);

    return 0;
}

static int test_exec_multi_body_join(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t c = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, a, b, prov);
    (void)exec_assert_binary_fact(&f, 1u, b, c, prov);

    laplace_exact_literal_t body[2];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    body[1].predicate = 1u; body[1].arity = 2u;
    body[1].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    body[1].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 2u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup[2] = {a.id, c.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, lookup, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived >= 1u);

    return 0;
}

static int test_exec_repeated_variable(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    const laplace_exact_predicate_desc_t pred1 = {.arity = 1u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred1) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, a, a, prov);
    (void)exec_assert_binary_fact(&f, 1u, a, b, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u}; /* same var! */

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 1u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t self_a[1] = {a.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, self_a, 1u) != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_entity_id_t self_b[1] = {b.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, self_b, 1u) == LAPLACE_EXACT_FACT_ROW_INVALID);

    return 0;
}

static int test_exec_constant_in_body(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t alice = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t bob   = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t carol = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov  = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, alice, bob, prov);
    (void)exec_assert_binary_fact(&f, 1u, carol, bob, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_CONSTANT, .value.constant = bob.id};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_CONSTANT, .value.constant = bob.id};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t la[2] = {alice.id, bob.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, la, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    const laplace_entity_id_t lc[2] = {carol.id, bob.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, lc, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 2u);

    return 0;
}

static int test_exec_dedup(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, a, b, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule_desc) != LAPLACE_RULE_ID_INVALID);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);

    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup[2] = {a.id, b.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, lookup, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 1u);
    LAPLACE_TEST_ASSERT(stats->facts_deduplicated >= 1u);

    return 0;
}

static int test_exec_provenance_attachment(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    const laplace_entity_handle_t source_fact = exec_assert_binary_fact(&f, 1u, a, b, prov);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u; body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u; rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    const laplace_rule_id_t rid = exec_add_rule(&f, &rule_desc);
    LAPLACE_TEST_ASSERT(rid != LAPLACE_RULE_ID_INVALID);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);
    LAPLACE_TEST_ASSERT(laplace_exec_run(&f.exec) == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup[2] = {a.id, b.id};
    const laplace_exact_fact_row_t derived_row = laplace_exact_find_fact(&f.store, 2u, lookup, 2u);
    LAPLACE_TEST_ASSERT(derived_row != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exact_fact_t* const derived = laplace_exact_get_fact(&f.store, derived_row);
    LAPLACE_TEST_ASSERT(derived != NULL);
    LAPLACE_TEST_ASSERT(derived->provenance != LAPLACE_PROVENANCE_ID_INVALID);

    const laplace_exact_provenance_record_t* const prov_rec = laplace_exact_get_provenance(&f.store, derived->provenance);
    LAPLACE_TEST_ASSERT(prov_rec != NULL);
    LAPLACE_TEST_ASSERT(prov_rec->kind == LAPLACE_EXACT_PROVENANCE_DERIVED);
    LAPLACE_TEST_ASSERT(prov_rec->source_rule_id == rid);

    uint32_t parent_count = 0u;
    const laplace_entity_id_t* const parents = laplace_exact_get_provenance_parents(&f.store, derived->provenance, &parent_count);
    LAPLACE_TEST_ASSERT(parents != NULL);
    LAPLACE_TEST_ASSERT(parent_count == 1u);
    LAPLACE_TEST_ASSERT(parents[0] == source_fact.id);

    return 0;
}

static int test_exec_state_transitions(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    const laplace_entity_handle_t fact_entity = exec_assert_binary_fact(&f, 1u, a, b, prov);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&f.entity_pool, fact_entity) == LAPLACE_STATE_READY);

    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, fact_entity.id) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_step(&f.exec) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&f.entity_pool, fact_entity) == LAPLACE_STATE_ACTIVE);

    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 0u);
    LAPLACE_TEST_ASSERT(laplace_exec_step(&f.exec) == LAPLACE_ERR_INVALID_STATE);

    return 0;
}

static int test_exec_bounded_step(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t c = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, a, b, prov);
    (void)exec_assert_binary_fact(&f, 1u, b, c, prov);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_set_max_steps(&f.exec, 1u);
    laplace_exec_mark_all_facts_ready(&f.exec);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 2u);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_BUDGET);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->steps_executed == 1u);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 1u);

    return 0;
}

static int test_exec_transitive_closure_fixpoint(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 2u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t n1 = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t n2 = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t n3 = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t n4 = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, n1, n2, prov);
    (void)exec_assert_binary_fact(&f, 1u, n2, n3, prov);
    (void)exec_assert_binary_fact(&f, 1u, n3, n4, prov);

    laplace_exact_literal_t body1[1];
    memset(body1, 0, sizeof(body1));
    body1[0].predicate = 1u; body1[0].arity = 2u;
    body1[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body1[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule1;
    memset(&rule1, 0, sizeof(rule1));
    rule1.head.predicate = 2u; rule1.head.arity = 2u;
    rule1.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule1.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule1.body_literals = body1;
    rule1.body_count = 1u;

    laplace_exact_literal_t body2[2];
    memset(body2, 0, sizeof(body2));
    body2[0].predicate = 2u; body2[0].arity = 2u;
    body2[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body2[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    body2[1].predicate = 1u; body2[1].arity = 2u;
    body2[1].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 3u};
    body2[1].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule2;
    memset(&rule2, 0, sizeof(rule2));
    rule2.head.predicate = 2u; rule2.head.arity = 2u;
    rule2.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule2.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule2.body_literals = body2;
    rule2.body_count = 2u;

    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule1) != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(exec_add_rule(&f, &rule2) != LAPLACE_RULE_ID_INVALID);

    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_mark_all_facts_ready(&f.exec);

    const laplace_exec_run_status_t status = laplace_exec_run(&f.exec);
    LAPLACE_TEST_ASSERT(status == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t p12[2] = {n1.id, n2.id};
    const laplace_entity_id_t p23[2] = {n2.id, n3.id};
    const laplace_entity_id_t p34[2] = {n3.id, n4.id};
    const laplace_entity_id_t p13[2] = {n1.id, n3.id};
    const laplace_entity_id_t p24[2] = {n2.id, n4.id};
    const laplace_entity_id_t p14[2] = {n1.id, n4.id};

    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p12, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p23, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p34, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p13, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p24, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p14, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_entity_id_t p41[2] = {n4.id, n1.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, p41, 2u) == LAPLACE_EXACT_FACT_ROW_INVALID);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 6u);

    return 0;
}

static uint32_t exec_run_tc_and_count(const laplace_exec_mode_t mode) {
    test_exec_fixture_t f;
    memset(&f, 0, sizeof(f));
    (void)laplace_arena_init(&f.entity_arena, g_exec_entity_buf, sizeof(g_exec_entity_buf));
    (void)laplace_arena_init(&f.store_arena, g_exec_store_buf, sizeof(g_exec_store_buf));
    (void)laplace_arena_init(&f.exec_arena, g_exec_exec_buf, sizeof(g_exec_exec_buf));
    (void)laplace_entity_pool_init(&f.entity_pool, &f.entity_arena, TEST_EXEC_ENTITY_CAPACITY);
    (void)laplace_exact_store_init(&f.store, &f.store_arena, &f.entity_pool);
    (void)laplace_exec_init(&f.exec, &f.exec_arena, &f.store, &f.entity_pool);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    (void)laplace_exact_register_predicate(&f.store, 1u, &pred2);
    (void)laplace_exact_register_predicate(&f.store, 2u, &pred2);

    laplace_entity_handle_t nodes[4];
    for (int i = 0; i < 4; ++i) {
        nodes[i] = exec_register_constant(&f, 1u);
    }
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    (void)exec_assert_binary_fact(&f, 1u, nodes[0], nodes[1], prov);
    (void)exec_assert_binary_fact(&f, 1u, nodes[1], nodes[2], prov);
    (void)exec_assert_binary_fact(&f, 1u, nodes[2], nodes[3], prov);

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

    laplace_rule_id_t rid1 = LAPLACE_RULE_ID_INVALID;
    laplace_rule_id_t rid2 = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t val;
    (void)laplace_exact_add_rule(&f.store, &r1, &rid1, &val);
    (void)laplace_exact_add_rule(&f.store, &r2, &rid2, &val);
    (void)laplace_exec_build_trigger_index(&f.exec);

    laplace_exec_set_mode(&f.exec, mode);
    laplace_exec_mark_all_facts_ready(&f.exec);
    (void)laplace_exec_run(&f.exec);

    uint32_t path_count = 0u;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j) continue;
            const laplace_entity_id_t args[2] = {nodes[i].id, nodes[j].id};
            if (laplace_exact_find_fact(&f.store, 2u, args, 2u) != LAPLACE_EXACT_FACT_ROW_INVALID) {
                ++path_count;
            }
        }
    }

    return path_count;
}

static int test_exec_dense_sparse_equivalence(void) {
    const uint32_t dense_count  = exec_run_tc_and_count(LAPLACE_EXEC_MODE_DENSE);
    const uint32_t sparse_count = exec_run_tc_and_count(LAPLACE_EXEC_MODE_SPARSE);

    LAPLACE_TEST_ASSERT(dense_count == 6u);
    LAPLACE_TEST_ASSERT(sparse_count == 6u);
    LAPLACE_TEST_ASSERT(dense_count == sparse_count);

    return 0;
}

static int test_exec_requires_trigger_index(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    LAPLACE_TEST_ASSERT(laplace_exec_step(&f.exec) == LAPLACE_ERR_INVALID_STATE);
    LAPLACE_TEST_ASSERT(laplace_exec_run(&f.exec) == LAPLACE_EXEC_RUN_ERROR);

    return 0;
}

static int test_exec_reset(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    const laplace_exact_predicate_desc_t pred2 = {.arity = 2u, .flags = 0u, .fact_capacity = 32u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f.store, 1u, &pred2) == LAPLACE_OK);

    const laplace_entity_handle_t a = exec_register_constant(&f, 1u);
    const laplace_entity_handle_t b = exec_register_constant(&f, 1u);
    const laplace_provenance_id_t prov = exec_insert_asserted_provenance(&f);

    const laplace_entity_handle_t fact_entity = exec_assert_binary_fact(&f, 1u, a, b, prov);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, fact_entity.id) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 1u);

    laplace_exec_reset(&f.exec);

    LAPLACE_TEST_ASSERT(laplace_exec_ready_count(&f.exec) == 0u);
    LAPLACE_TEST_ASSERT(!f.exec.trigger_index_built);

    const laplace_exec_stats_t* const stats = laplace_exec_get_stats(&f.exec);
    LAPLACE_TEST_ASSERT(stats->steps_executed == 0u);
    LAPLACE_TEST_ASSERT(stats->facts_derived == 0u);

    return 0;
}

static int test_exec_mode_getter(void) {
    test_exec_fixture_t f;
    LAPLACE_TEST_ASSERT(exec_fixture_init(&f) == 0);

    LAPLACE_TEST_ASSERT(laplace_exec_get_mode(&f.exec) == LAPLACE_EXEC_MODE_DENSE);
    laplace_exec_set_mode(&f.exec, LAPLACE_EXEC_MODE_SPARSE);
    LAPLACE_TEST_ASSERT(laplace_exec_get_mode(&f.exec) == LAPLACE_EXEC_MODE_SPARSE);

    return 0;
}

int laplace_test_exec(void) {
    const laplace_test_case_t subtests[] = {
        {"exec_ready_marking",                  test_exec_ready_marking},
        {"exec_mark_all_facts",                 test_exec_mark_all_facts},
        {"exec_trigger_index",                  test_exec_trigger_index},
        {"exec_single_rule_derivation_dense",   test_exec_single_rule_derivation_dense},
        {"exec_single_rule_derivation_sparse",  test_exec_single_rule_derivation_sparse},
        {"exec_multi_body_join",                test_exec_multi_body_join},
        {"exec_repeated_variable",              test_exec_repeated_variable},
        {"exec_constant_in_body",               test_exec_constant_in_body},
        {"exec_dedup",                          test_exec_dedup},
        {"exec_provenance_attachment",          test_exec_provenance_attachment},
        {"exec_state_transitions",              test_exec_state_transitions},
        {"exec_bounded_step",                   test_exec_bounded_step},
        {"exec_transitive_closure_fixpoint",    test_exec_transitive_closure_fixpoint},
        {"exec_dense_sparse_equivalence",       test_exec_dense_sparse_equivalence},
        {"exec_requires_trigger_index",         test_exec_requires_trigger_index},
        {"exec_reset",                          test_exec_reset},
        {"exec_mode_getter",                    test_exec_mode_getter},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    for (size_t i = 0u; i < count; ++i) {
        const int rc = subtests[i].fn();
        if (rc != 0) {
            fprintf(stderr, "  subtest FAIL: %s\n", subtests[i].name);
            return 1;
        }
    }

    return 0;
}
