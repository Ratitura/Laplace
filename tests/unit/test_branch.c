#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/arena.h"
#include "laplace/branch.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/state.h"
#include "test_harness.h"

#define TEST_BRANCH_ENTITY_CAPACITY 256u
#define TEST_BRANCH_ENTITY_ARENA_BYTES (TEST_BRANCH_ENTITY_CAPACITY * 128u)
#define TEST_BRANCH_STORE_ARENA_BYTES  (2u * 1024u * 1024u)
#define TEST_BRANCH_EXEC_ARENA_BYTES   (512u * 1024u)
#define TEST_BRANCH_BRANCH_ARENA_BYTES (3u * 1024u * 1024u)

static _Alignas(64) uint8_t g_branch_entity_buf[TEST_BRANCH_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_branch_store_buf[TEST_BRANCH_STORE_ARENA_BYTES];
static _Alignas(64) uint8_t g_branch_exec_buf[TEST_BRANCH_EXEC_ARENA_BYTES];
static _Alignas(64) uint8_t g_branch_branch_buf[TEST_BRANCH_BRANCH_ARENA_BYTES];

typedef struct test_branch_fixture {
    laplace_arena_t entity_arena;
    laplace_arena_t store_arena;
    laplace_arena_t exec_arena;
    laplace_arena_t branch_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;
    laplace_exec_context_t exec;
    laplace_branch_system_t branches;
} test_branch_fixture_t;

static int branch_fixture_init(test_branch_fixture_t* const f) {
    memset(f, 0, sizeof(*f));
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->entity_arena, g_branch_entity_buf, sizeof(g_branch_entity_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->store_arena, g_branch_store_buf, sizeof(g_branch_store_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->exec_arena, g_branch_exec_buf, sizeof(g_branch_exec_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&f->branch_arena, g_branch_branch_buf, sizeof(g_branch_branch_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&f->entity_pool, &f->entity_arena, TEST_BRANCH_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_store_init(&f->store, &f->store_arena, &f->entity_pool) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_init(&f->exec, &f->exec_arena, &f->store, &f->entity_pool) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_branch_system_init(&f->branches, &f->branch_arena, &f->store, &f->entity_pool) == LAPLACE_OK);
    laplace_exec_bind_branch_system(&f->exec, &f->branches);
    return 0;
}

static laplace_entity_handle_t branch_alloc_ready(test_branch_fixture_t* const f) {
    laplace_entity_handle_t handle = laplace_entity_pool_alloc(&f->entity_pool);
    if (handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return handle;
    }

    if (laplace_entity_pool_set_state(&f->entity_pool, handle, LAPLACE_STATE_READY) != LAPLACE_OK) {
        handle.id = LAPLACE_ENTITY_ID_INVALID;
        handle.generation = LAPLACE_GENERATION_INVALID;
    }

    return handle;
}

static laplace_entity_handle_t branch_register_global_constant(test_branch_fixture_t* const f,
                                                                const laplace_exact_type_id_t type_id) {
    laplace_entity_handle_t handle = branch_alloc_ready(f);
    if (handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return handle;
    }

    if (laplace_exact_register_constant(&f->store, handle, type_id, 0u) != LAPLACE_OK) {
        handle.id = LAPLACE_ENTITY_ID_INVALID;
        handle.generation = LAPLACE_GENERATION_INVALID;
    }

    return handle;
}

static laplace_branch_handle_t branch_create_root(test_branch_fixture_t* const f) {
    laplace_error_t error = LAPLACE_OK;
    laplace_branch_handle_t branch = laplace_branch_create(&f->branches,
                                                           (laplace_branch_handle_t){0},
                                                           &error);
    if (error != LAPLACE_OK || branch.id == LAPLACE_BRANCH_ID_INVALID ||
        !laplace_branch_is_active(&f->branches, branch)) {
        branch.id = LAPLACE_BRANCH_ID_INVALID;
        branch.generation = LAPLACE_BRANCH_GENERATION_INVALID;
    }

    return branch;
}

static int branch_register_predicates(test_branch_fixture_t* const f) {
    const laplace_exact_predicate_desc_t pred = {.arity = 2u, .flags = 0u, .fact_capacity = 64u};
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f->store, 1u, &pred) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&f->store, 2u, &pred) == LAPLACE_OK);
    return 0;
}

static int test_branch_create_and_info(void) {
    test_branch_fixture_t f;
    LAPLACE_TEST_ASSERT(branch_fixture_init(&f) == 0);

    const laplace_branch_handle_t branch = branch_create_root(&f);
    laplace_branch_info_t info;
    LAPLACE_TEST_ASSERT(laplace_branch_get_info(&f.branches, branch, &info) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(info.status == LAPLACE_BRANCH_STATUS_ACTIVE);
    LAPLACE_TEST_ASSERT(info.create_epoch == 1u);
    LAPLACE_TEST_ASSERT(info.parent.id == LAPLACE_BRANCH_ID_INVALID);
    return 0;
}

static int test_branch_local_fact_isolation_and_failure(void) {
    test_branch_fixture_t f;
    LAPLACE_TEST_ASSERT(branch_fixture_init(&f) == 0);
    LAPLACE_TEST_ASSERT(branch_register_predicates(&f) == 0);

    const laplace_entity_handle_t alice = branch_register_global_constant(&f, 1u);
    const laplace_entity_handle_t bob = branch_register_global_constant(&f, 1u);
    const laplace_branch_handle_t branch = branch_create_root(&f);
    laplace_provenance_id_t branch_prov = LAPLACE_PROVENANCE_ID_INVALID;
    LAPLACE_TEST_ASSERT(laplace_branch_insert_asserted_provenance(&f.branches, branch, &branch_prov) == LAPLACE_OK);

    const laplace_entity_handle_t args[2] = {alice, bob};
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_branch_assert_fact(&f.branches,
                                                   branch,
                                                   1u,
                                                   args,
                                                   2u,
                                                   branch_prov,
                                                   LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                   &row,
                                                   &fact_entity,
                                                   &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);
    const laplace_entity_id_t lookup[2] = {alice.id, bob.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 1u, lookup, 2u) == LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact_in_branch(&f.store, branch, 1u, lookup, 2u) == row);

    LAPLACE_TEST_ASSERT(laplace_branch_fail(&f.branches, branch) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact_in_branch(&f.store, branch, 1u, lookup, 2u) == LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&f.entity_pool, fact_entity));
    return 0;
}

static int test_branch_commit_and_deduplication(void) {
    test_branch_fixture_t f;
    LAPLACE_TEST_ASSERT(branch_fixture_init(&f) == 0);
    LAPLACE_TEST_ASSERT(branch_register_predicates(&f) == 0);

    const laplace_entity_handle_t alice = branch_register_global_constant(&f, 1u);
    const laplace_entity_handle_t bob = branch_register_global_constant(&f, 1u);
    const laplace_entity_id_t lookup[2] = {alice.id, bob.id};

    const laplace_branch_handle_t branch_a = branch_create_root(&f);
    const laplace_branch_handle_t branch_b = branch_create_root(&f);
    laplace_provenance_id_t prov_a = LAPLACE_PROVENANCE_ID_INVALID;
    laplace_provenance_id_t prov_b = LAPLACE_PROVENANCE_ID_INVALID;
    LAPLACE_TEST_ASSERT(laplace_branch_insert_asserted_provenance(&f.branches, branch_a, &prov_a) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_branch_insert_asserted_provenance(&f.branches, branch_b, &prov_b) == LAPLACE_OK);

    const laplace_entity_handle_t args[2] = {alice, bob};
    laplace_exact_fact_row_t row_a = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t entity_a = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_branch_assert_fact(&f.branches, branch_a, 1u, args, 2u, prov_a,
                                                   LAPLACE_EXACT_FACT_FLAG_ASSERTED, &row_a, &entity_a, &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);

    laplace_exact_fact_row_t row_b = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t entity_b = {0};
    inserted = false;
    LAPLACE_TEST_ASSERT(laplace_branch_assert_fact(&f.branches, branch_b, 1u, args, 2u, prov_b,
                                                   LAPLACE_EXACT_FACT_FLAG_ASSERTED, &row_b, &entity_b, &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);

    uint32_t promoted = 0u;
    uint32_t deduplicated = 0u;
    LAPLACE_TEST_ASSERT(laplace_branch_commit(&f.branches, branch_a, &promoted, &deduplicated) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(promoted == 1u);
    LAPLACE_TEST_ASSERT(deduplicated == 0u);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 1u, lookup, 2u) == row_a);

    promoted = 0u;
    deduplicated = 0u;
    LAPLACE_TEST_ASSERT(laplace_branch_commit(&f.branches, branch_b, &promoted, &deduplicated) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(promoted == 0u);
    LAPLACE_TEST_ASSERT(deduplicated == 1u);
    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&f.entity_pool, entity_b));
    return 0;
}

static int test_branch_exec_isolation_and_reclaim(void) {
    test_branch_fixture_t f;
    LAPLACE_TEST_ASSERT(branch_fixture_init(&f) == 0);
    LAPLACE_TEST_ASSERT(branch_register_predicates(&f) == 0);

    const laplace_entity_handle_t alice = branch_register_global_constant(&f, 1u);
    const laplace_entity_handle_t bob = branch_register_global_constant(&f, 1u);
    const laplace_branch_handle_t branch = branch_create_root(&f);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u;
    body[0].arity = 2u;
    body[0].terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    body[0].terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 2u;
    rule_desc.head.arity = 2u;
    rule_desc.head.terms[0] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 1u};
    rule_desc.head.terms[1] = (laplace_exact_term_t){.kind = LAPLACE_EXACT_TERM_VARIABLE, .value.variable = 2u};
    rule_desc.body_literals = body;
    rule_desc.body_count = 1u;

    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t validation;
    LAPLACE_TEST_ASSERT(laplace_exact_add_rule(&f.store, &rule_desc, &rule_id, &validation) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&f.exec) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_set_active_branch(&f.exec, branch) == LAPLACE_OK);

    laplace_provenance_id_t branch_prov = LAPLACE_PROVENANCE_ID_INVALID;
    LAPLACE_TEST_ASSERT(laplace_branch_insert_asserted_provenance(&f.branches, branch, &branch_prov) == LAPLACE_OK);

    const laplace_entity_handle_t args[2] = {alice, bob};
    laplace_exact_fact_row_t likes_row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t likes_entity = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_branch_assert_fact(&f.branches, branch, 1u, args, 2u, branch_prov,
                                                   LAPLACE_EXACT_FACT_FLAG_ASSERTED, &likes_row, &likes_entity, &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);
    LAPLACE_TEST_ASSERT(laplace_exec_mark_ready(&f.exec, likes_entity.id) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exec_run(&f.exec) == LAPLACE_EXEC_RUN_FIXPOINT);

    const laplace_entity_id_t lookup[2] = {alice.id, bob.id};
    const laplace_exact_fact_row_t friend_row = laplace_exact_find_fact_in_branch(&f.store, branch, 2u, lookup, 2u);
    LAPLACE_TEST_ASSERT(friend_row != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&f.store, 2u, lookup, 2u) == LAPLACE_EXACT_FACT_ROW_INVALID);

    LAPLACE_TEST_ASSERT(laplace_branch_fail(&f.branches, branch) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_branch_advance_epoch(&f.branches, NULL) == LAPLACE_OK);
    uint32_t reclaimed_branches = 0u;
    uint32_t reclaimed_entities = 0u;
    LAPLACE_TEST_ASSERT(laplace_branch_reclaim_closed(&f.branches, &reclaimed_branches, &reclaimed_entities) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(reclaimed_branches == 1u);
    LAPLACE_TEST_ASSERT(reclaimed_entities >= 1u);
    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&f.entity_pool, likes_entity));
    return 0;
}

int laplace_test_branch(void) {
    const laplace_test_case_t cases[] = {
        {"branch_create_and_info", test_branch_create_and_info},
        {"branch_local_fact_isolation_and_failure", test_branch_local_fact_isolation_and_failure},
        {"branch_commit_and_deduplication", test_branch_commit_and_deduplication},
        {"branch_exec_isolation_and_reclaim", test_branch_exec_isolation_and_reclaim},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int rc = cases[i].fn();
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}
