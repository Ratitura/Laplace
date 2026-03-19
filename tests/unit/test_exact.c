#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/state.h"
#include "test_harness.h"

#define TEST_EXACT_ENTITY_CAPACITY 128u
#define TEST_EXACT_ENTITY_ARENA_BYTES (TEST_EXACT_ENTITY_CAPACITY * 96u)
#define TEST_EXACT_STORE_ARENA_BYTES  (2u * 1024u * 1024u)

static _Alignas(64) uint8_t g_exact_entity_buf[TEST_EXACT_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_exact_store_buf[TEST_EXACT_STORE_ARENA_BYTES];

typedef struct test_exact_fixture {
    laplace_arena_t       entity_arena;
    laplace_arena_t       store_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;
} test_exact_fixture_t;

static int exact_fixture_init(test_exact_fixture_t* const fixture) {
    memset(fixture, 0, sizeof(*fixture));
    LAPLACE_TEST_ASSERT(laplace_arena_init(&fixture->entity_arena, g_exact_entity_buf, sizeof(g_exact_entity_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&fixture->store_arena, g_exact_store_buf, sizeof(g_exact_store_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&fixture->entity_pool, &fixture->entity_arena, TEST_EXACT_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_store_init(&fixture->store, &fixture->store_arena, &fixture->entity_pool) == LAPLACE_OK);
    return 0;
}

static laplace_entity_handle_t exact_alloc_ready_entity(test_exact_fixture_t* const fixture) {
    laplace_entity_handle_t handle = laplace_entity_pool_alloc(&fixture->entity_pool);
    if (handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return handle;
    }

    if (laplace_entity_pool_set_state(&fixture->entity_pool, handle, LAPLACE_STATE_READY) != LAPLACE_OK) {
        handle.id = LAPLACE_ENTITY_ID_INVALID;
        handle.generation = LAPLACE_GENERATION_INVALID;
    }

    return handle;
}

static laplace_entity_handle_t exact_register_constant(test_exact_fixture_t* const fixture,
                                                       const laplace_exact_type_id_t type_id) {
    laplace_entity_handle_t handle = exact_alloc_ready_entity(fixture);
    if (handle.id == LAPLACE_ENTITY_ID_INVALID) {
        return handle;
    }

    if (laplace_exact_register_constant(&fixture->store, handle, type_id, 0u) != LAPLACE_OK) {
        handle.id = LAPLACE_ENTITY_ID_INVALID;
        handle.generation = LAPLACE_GENERATION_INVALID;
    }

    return handle;
}

static int exact_register_baseline_predicates(test_exact_fixture_t* const fixture) {
    const laplace_exact_predicate_desc_t parent_desc = {
        .arity = 2u,
        .flags = 0u,
        .fact_capacity = 32u
    };
    const laplace_exact_predicate_desc_t likes_desc = {
        .arity = 2u,
        .flags = 0u,
        .fact_capacity = 32u
    };
    const laplace_exact_predicate_desc_t ancestor_desc = {
        .arity = 2u,
        .flags = 0u,
        .fact_capacity = 32u
    };

    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture->store, 1u, &parent_desc) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture->store, 2u, &likes_desc) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture->store, 3u, &ancestor_desc) == LAPLACE_OK);
    return 0;
}

static laplace_provenance_id_t exact_insert_asserted_provenance(test_exact_fixture_t* const fixture) {
    laplace_provenance_id_t provenance_id = LAPLACE_PROVENANCE_ID_INVALID;
    const laplace_exact_provenance_desc_t desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_ASSERTED,
        .source_rule_id = LAPLACE_RULE_ID_INVALID,
        .parent_facts = NULL,
        .parent_count = 0u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    LAPLACE_TEST_ASSERT(laplace_exact_insert_provenance(&fixture->store, &desc, &provenance_id) == LAPLACE_OK);
    return provenance_id;
}

static int test_exact_predicate_registry(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);

    const laplace_exact_predicate_desc_t desc = {
        .arity = 2u,
        .flags = 0xA5u,
        .fact_capacity = 8u
    };

    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture.store, 7u, &desc) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_predicate_is_declared(&fixture.store, 7u));
    LAPLACE_TEST_ASSERT(laplace_exact_predicate_arity(&fixture.store, 7u) == 2u);
    LAPLACE_TEST_ASSERT(!laplace_exact_predicate_is_declared(&fixture.store, 8u));
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture.store, 7u, &desc) == LAPLACE_ERR_INVALID_STATE);

    const laplace_exact_predicate_desc_t bad_desc = {
        .arity = 0u,
        .flags = 0u,
        .fact_capacity = 1u
    };
    LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&fixture.store, 8u, &bad_desc) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_exact_constant_entity_metadata(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);

    const laplace_entity_handle_t constant = exact_register_constant(&fixture, 11u);
    const laplace_entity_exact_meta_t meta = laplace_entity_pool_get_exact_meta(&fixture.entity_pool, constant);
    LAPLACE_TEST_ASSERT(meta.role == LAPLACE_ENTITY_EXACT_ROLE_CONSTANT);
    LAPLACE_TEST_ASSERT(meta.type_id == 11u);
    LAPLACE_TEST_ASSERT(meta.fact_row == LAPLACE_EXACT_FACT_ROW_INVALID);
    return 0;
}

static int test_exact_fact_insert_dedup_lookup_and_scan(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    const laplace_entity_handle_t alice = exact_register_constant(&fixture, 1u);
    const laplace_entity_handle_t bob = exact_register_constant(&fixture, 1u);
    const laplace_entity_handle_t carol = exact_register_constant(&fixture, 1u);
    const laplace_provenance_id_t asserted = exact_insert_asserted_provenance(&fixture);

    const laplace_entity_handle_t args1[2] = {alice, bob};
    laplace_exact_fact_row_t row1 = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity1 = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store,
                                                  1u,
                                                  args1,
                                                  2u,
                                                  asserted,
                                                  LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                  &row1,
                                                  &fact_entity1,
                                                  &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);
    LAPLACE_TEST_ASSERT(row1 == 1u);

    const laplace_exact_fact_t* const fact1 = laplace_exact_get_fact(&fixture.store, row1);
    LAPLACE_TEST_ASSERT(fact1 != NULL);
    LAPLACE_TEST_ASSERT(fact1->entity == fact_entity1.id);
    LAPLACE_TEST_ASSERT(fact1->predicate == 1u);
    LAPLACE_TEST_ASSERT(fact1->args[0] == alice.id);
    LAPLACE_TEST_ASSERT(fact1->args[1] == bob.id);

    const laplace_entity_exact_meta_t fact_meta = laplace_entity_pool_get_exact_meta(&fixture.entity_pool, fact_entity1);
    LAPLACE_TEST_ASSERT(fact_meta.role == LAPLACE_ENTITY_EXACT_ROLE_FACT);
    LAPLACE_TEST_ASSERT(fact_meta.fact_row == row1);

    laplace_exact_fact_row_t row1_dupe = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity_dupe = {0};
    inserted = true;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store,
                                                  1u,
                                                  args1,
                                                  2u,
                                                  asserted,
                                                  LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                  &row1_dupe,
                                                  &fact_entity_dupe,
                                                  &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(!inserted);
    LAPLACE_TEST_ASSERT(row1_dupe == row1);
    LAPLACE_TEST_ASSERT(fact_entity_dupe.id == fact_entity1.id);

    const laplace_entity_handle_t args2[2] = {alice, carol};
    laplace_exact_fact_row_t row2 = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity2 = {0};
    inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store,
                                                  1u,
                                                  args2,
                                                  2u,
                                                  asserted,
                                                  LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                  &row2,
                                                  &fact_entity2,
                                                  &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);
    LAPLACE_TEST_ASSERT(row2 == 2u);

    const laplace_entity_id_t lookup_args[2] = {alice.id, bob.id};
    LAPLACE_TEST_ASSERT(laplace_exact_find_fact(&fixture.store, 1u, lookup_args, 2u) == row1);

    const laplace_exact_predicate_view_t view = laplace_exact_predicate_rows(&fixture.store, 1u);
    LAPLACE_TEST_ASSERT(view.count == 2u);
    LAPLACE_TEST_ASSERT(view.rows[0] == row1);
    LAPLACE_TEST_ASSERT(view.rows[1] == row2);
    return 0;
}

static int test_exact_fact_requires_registered_constants(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    const laplace_entity_handle_t untyped = exact_alloc_ready_entity(&fixture);
    const laplace_entity_handle_t constant = exact_register_constant(&fixture, 1u);
    const laplace_provenance_id_t asserted = exact_insert_asserted_provenance(&fixture);

    const laplace_entity_handle_t args[2] = {untyped, constant};
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store,
                                                  1u,
                                                  args,
                                                  2u,
                                                  asserted,
                                                  LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                  &row,
                                                  &fact_entity,
                                                  &inserted) == LAPLACE_ERR_INVALID_STATE);
    return 0;
}

static int test_exact_rule_validation_and_storage(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    const laplace_entity_handle_t alice = exact_register_constant(&fixture, 1u);

    laplace_exact_literal_t body[2];
    memset(body, 0, sizeof(body));

    body[0].predicate = 1u;
    body[0].arity = 2u;
    body[0].terms[0].kind = LAPLACE_EXACT_TERM_VARIABLE;
    body[0].terms[0].value.variable = 1u;
    body[0].terms[1].kind = LAPLACE_EXACT_TERM_VARIABLE;
    body[0].terms[1].value.variable = 2u;

    body[1].predicate = 2u;
    body[1].arity = 2u;
    body[1].terms[0].kind = LAPLACE_EXACT_TERM_VARIABLE;
    body[1].terms[0].value.variable = 2u;
    body[1].terms[1].kind = LAPLACE_EXACT_TERM_CONSTANT;
    body[1].terms[1].value.constant = alice.id;

    laplace_exact_rule_desc_t rule_desc;
    memset(&rule_desc, 0, sizeof(rule_desc));
    rule_desc.head.predicate = 3u;
    rule_desc.head.arity = 2u;
    rule_desc.head.terms[0].kind = LAPLACE_EXACT_TERM_VARIABLE;
    rule_desc.head.terms[0].value.variable = 1u;
    rule_desc.head.terms[1].kind = LAPLACE_EXACT_TERM_VARIABLE;
    rule_desc.head.terms[1].value.variable = 2u;
    rule_desc.body_literals = body;
    rule_desc.body_count = 2u;

    const laplace_exact_rule_validation_result_t validation = laplace_exact_validate_rule(&fixture.store, &rule_desc);
    LAPLACE_TEST_ASSERT(validation.error == LAPLACE_EXACT_RULE_VALIDATION_OK);

    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t add_validation;
    LAPLACE_TEST_ASSERT(laplace_exact_add_rule(&fixture.store, &rule_desc, &rule_id, &add_validation) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(rule_id == 1u);
    LAPLACE_TEST_ASSERT(add_validation.error == LAPLACE_EXACT_RULE_VALIDATION_OK);

    const laplace_exact_rule_t* const rule = laplace_exact_get_rule(&fixture.store, rule_id);
    LAPLACE_TEST_ASSERT(rule != NULL);
    LAPLACE_TEST_ASSERT(rule->head.predicate == 3u);
    uint32_t body_count = 0u;
    const laplace_exact_literal_t* const stored_body = laplace_exact_rule_body_literals(&fixture.store, rule, &body_count);
    LAPLACE_TEST_ASSERT(stored_body != NULL);
    LAPLACE_TEST_ASSERT(body_count == 2u);
    LAPLACE_TEST_ASSERT(stored_body[1].predicate == 2u);
    LAPLACE_TEST_ASSERT(stored_body[1].terms[1].value.constant == alice.id);
    return 0;
}

static int test_exact_rule_validation_failures(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    laplace_exact_rule_desc_t missing_body_rule;
    memset(&missing_body_rule, 0, sizeof(missing_body_rule));
    missing_body_rule.head.predicate = 3u;
    missing_body_rule.head.arity = 2u;
    LAPLACE_TEST_ASSERT(laplace_exact_validate_rule(&fixture.store, &missing_body_rule).error == LAPLACE_EXACT_RULE_VALIDATION_BODY_REQUIRED);

    laplace_exact_literal_t body[1];
    memset(body, 0, sizeof(body));
    body[0].predicate = 1u;
    body[0].arity = 2u;
    body[0].terms[0].kind = LAPLACE_EXACT_TERM_VARIABLE;
    body[0].terms[0].value.variable = 1u;
    body[0].terms[1].kind = LAPLACE_EXACT_TERM_VARIABLE;
    body[0].terms[1].value.variable = 2u;

    laplace_exact_rule_desc_t unsafe_rule;
    memset(&unsafe_rule, 0, sizeof(unsafe_rule));
    unsafe_rule.head.predicate = 3u;
    unsafe_rule.head.arity = 2u;
    unsafe_rule.head.terms[0].kind = LAPLACE_EXACT_TERM_VARIABLE;
    unsafe_rule.head.terms[0].value.variable = 1u;
    unsafe_rule.head.terms[1].kind = LAPLACE_EXACT_TERM_VARIABLE;
    unsafe_rule.head.terms[1].value.variable = 3u;
    unsafe_rule.body_literals = body;
    unsafe_rule.body_count = 1u;
    LAPLACE_TEST_ASSERT(laplace_exact_validate_rule(&fixture.store, &unsafe_rule).error ==
                        LAPLACE_EXACT_RULE_VALIDATION_HEAD_VARIABLE_MISSING_FROM_BODY);

    laplace_exact_rule_desc_t null_body_rule = unsafe_rule;
    null_body_rule.body_literals = NULL;
    LAPLACE_TEST_ASSERT(laplace_exact_validate_rule(&fixture.store, &null_body_rule).error ==
                        LAPLACE_EXACT_RULE_VALIDATION_BODY_LITERALS_NULL);

    body[0].predicate = 99u;
    LAPLACE_TEST_ASSERT(laplace_exact_validate_rule(&fixture.store, &unsafe_rule).error ==
                        LAPLACE_EXACT_RULE_VALIDATION_BODY_PREDICATE_UNDECLARED);
    return 0;
}

static int test_exact_provenance_records(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    const laplace_entity_handle_t alice = exact_register_constant(&fixture, 1u);
    const laplace_entity_handle_t bob = exact_register_constant(&fixture, 1u);
    const laplace_provenance_id_t asserted = exact_insert_asserted_provenance(&fixture);

    const laplace_entity_handle_t args[2] = {alice, bob};
    laplace_exact_fact_row_t row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store,
                                                  1u,
                                                  args,
                                                  2u,
                                                  asserted,
                                                  LAPLACE_EXACT_FACT_FLAG_ASSERTED,
                                                  &row,
                                                  &fact_entity,
                                                  &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);

    const laplace_entity_id_t parent_ids[1] = {fact_entity.id};
    laplace_provenance_id_t derived = LAPLACE_PROVENANCE_ID_INVALID;
    const laplace_exact_provenance_desc_t derived_desc = {
        .kind = LAPLACE_EXACT_PROVENANCE_DERIVED,
        .source_rule_id = 42u,
        .parent_facts = parent_ids,
        .parent_count = 1u,
        .reserved_epoch = 0u,
        .reserved_branch = 0u
    };
    LAPLACE_TEST_ASSERT(laplace_exact_insert_provenance(&fixture.store, &derived_desc, &derived) == LAPLACE_OK);

    const laplace_exact_provenance_record_t* const derived_record = laplace_exact_get_provenance(&fixture.store, derived);
    LAPLACE_TEST_ASSERT(derived_record != NULL);
    LAPLACE_TEST_ASSERT(derived_record->kind == LAPLACE_EXACT_PROVENANCE_DERIVED);
    LAPLACE_TEST_ASSERT(derived_record->source_rule_id == 42u);
    LAPLACE_TEST_ASSERT(derived_record->tick > 0u);

    uint32_t parent_count = 0u;
    const laplace_entity_id_t* const parents = laplace_exact_get_provenance_parents(&fixture.store, derived, &parent_count);
    LAPLACE_TEST_ASSERT(parents != NULL);
    LAPLACE_TEST_ASSERT(parent_count == 1u);
    LAPLACE_TEST_ASSERT(parents[0] == fact_entity.id);
    return 0;
}

static int test_exact_deterministic_repeated_sequence(void) {
    test_exact_fixture_t fixture;
    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    laplace_entity_handle_t alice = exact_register_constant(&fixture, 1u);
    laplace_entity_handle_t bob = exact_register_constant(&fixture, 1u);
    laplace_provenance_id_t asserted = exact_insert_asserted_provenance(&fixture);

    const laplace_entity_handle_t args[2] = {alice, bob};
    laplace_exact_fact_row_t first_row = 0u;
    laplace_entity_handle_t first_fact = {0};
    bool inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store, 1u, args, 2u, asserted, 0u, &first_row, &first_fact, &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);

    LAPLACE_TEST_ASSERT(exact_fixture_init(&fixture) == 0);
    LAPLACE_TEST_ASSERT(exact_register_baseline_predicates(&fixture) == 0);

    alice = exact_register_constant(&fixture, 1u);
    bob = exact_register_constant(&fixture, 1u);
    asserted = exact_insert_asserted_provenance(&fixture);

    const laplace_entity_handle_t replay_args[2] = {alice, bob};
    laplace_exact_fact_row_t second_row = 0u;
    laplace_entity_handle_t second_fact = {0};
    inserted = false;
    LAPLACE_TEST_ASSERT(laplace_exact_assert_fact(&fixture.store, 1u, replay_args, 2u, asserted, 0u, &second_row, &second_fact, &inserted) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(inserted);
    LAPLACE_TEST_ASSERT(first_row == second_row);
    LAPLACE_TEST_ASSERT(first_fact.id == second_fact.id);
    return 0;
}

int laplace_test_exact(void) {
    const laplace_test_case_t subtests[] = {
        {"exact_predicate_registry", test_exact_predicate_registry},
        {"exact_constant_entity_metadata", test_exact_constant_entity_metadata},
        {"exact_fact_insert_dedup_lookup_and_scan", test_exact_fact_insert_dedup_lookup_and_scan},
        {"exact_fact_requires_registered_constants", test_exact_fact_requires_registered_constants},
        {"exact_rule_validation_and_storage", test_exact_rule_validation_and_storage},
        {"exact_rule_validation_failures", test_exact_rule_validation_failures},
        {"exact_provenance_records", test_exact_provenance_records},
        {"exact_deterministic_repeated_sequence", test_exact_deterministic_repeated_sequence},
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