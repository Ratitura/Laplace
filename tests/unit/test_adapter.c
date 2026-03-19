#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/adapter.h"
#include "laplace/adapter_facts.h"
#include "laplace/adapter_hv.h"
#include "laplace/adapter_rules.h"
#include "laplace/adapter_verify.h"
#include "laplace/arena.h"
#include "laplace/branch.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/hv.h"
#include "laplace/version.h"
#include "test_harness.h"

#define TEST_ADAPTER_ENTITY_CAPACITY  128u
#define TEST_ADAPTER_ENTITY_ARENA_BYTES (TEST_ADAPTER_ENTITY_CAPACITY * 96u)
#define TEST_ADAPTER_STORE_ARENA_BYTES  (2u * 1024u * 1024u)

static _Alignas(64) uint8_t g_adapter_entity_buf[TEST_ADAPTER_ENTITY_ARENA_BYTES];
static _Alignas(64) uint8_t g_adapter_store_buf[TEST_ADAPTER_STORE_ARENA_BYTES];

typedef struct test_adapter_fixture {
    laplace_arena_t       entity_arena;
    laplace_arena_t       store_arena;
    laplace_entity_pool_t entity_pool;
    laplace_exact_store_t store;
} test_adapter_fixture_t;

static int adapter_fixture_init(test_adapter_fixture_t* const f) {
    memset(f, 0, sizeof(*f));
    LAPLACE_TEST_ASSERT(
        laplace_arena_init(&f->entity_arena,
                           g_adapter_entity_buf,
                           sizeof(g_adapter_entity_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(
        laplace_arena_init(&f->store_arena,
                           g_adapter_store_buf,
                           sizeof(g_adapter_store_buf)) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(
        laplace_entity_pool_init(&f->entity_pool,
                                 &f->entity_arena,
                                 TEST_ADAPTER_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(
        laplace_exact_store_init(&f->store,
                                 &f->store_arena,
                                 &f->entity_pool) == LAPLACE_OK);
    return 0;
}

/*
 * Register a unary predicate (arity=1) with the given ID.
 */
static int register_unary_predicate(test_adapter_fixture_t* const f,
                                     const laplace_predicate_id_t pred_id) {
    const laplace_exact_predicate_desc_t desc = {
        .arity = 1,
        .flags = LAPLACE_EXACT_PREDICATE_FLAG_NONE,
        .fact_capacity = 64
    };
    LAPLACE_TEST_ASSERT(
        laplace_exact_register_predicate(&f->store, pred_id, &desc) == LAPLACE_OK);
    return 0;
}

/*
 * Register a binary predicate (arity=2) with the given ID.
 */
static int register_binary_predicate(test_adapter_fixture_t* const f,
                                      const laplace_predicate_id_t pred_id) {
    const laplace_exact_predicate_desc_t desc = {
        .arity = 2,
        .flags = LAPLACE_EXACT_PREDICATE_FLAG_NONE,
        .fact_capacity = 64
    };
    LAPLACE_TEST_ASSERT(
        laplace_exact_register_predicate(&f->store, pred_id, &desc) == LAPLACE_OK);
    return 0;
}

/*
 * Allocate an entity and register it as a constant.
 */
static int alloc_constant(test_adapter_fixture_t* const f,
                           laplace_entity_handle_t* const out) {
    *out = laplace_entity_pool_alloc(&f->entity_pool);
    LAPLACE_TEST_ASSERT(out->id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(
        laplace_entity_pool_set_state(&f->entity_pool, *out,
                                      LAPLACE_STATE_READY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(
        laplace_exact_register_constant(&f->store, *out,
                                        (laplace_exact_type_id_t)1u,
                                        0u) == LAPLACE_OK);
    return 0;
}

/* ====================================================================
 * Test: capability query
 * ==================================================================== */

static int test_adapter_capability(void) {
    laplace_adapter_capability_t cap;
    memset(&cap, 0xFF, sizeof(cap));

    laplace_adapter_query_capability(&cap);

    LAPLACE_TEST_ASSERT(cap.abi_version == LAPLACE_ADAPTER_ABI_VERSION);
    LAPLACE_TEST_ASSERT(cap.kernel_version_major == LAPLACE_VERSION_MAJOR);
    LAPLACE_TEST_ASSERT(cap.kernel_version_minor == LAPLACE_VERSION_MINOR);
    LAPLACE_TEST_ASSERT(cap.kernel_version_patch == LAPLACE_VERSION_PATCH);
    LAPLACE_TEST_ASSERT(cap.hv_dimension == LAPLACE_HV_DIM);
    LAPLACE_TEST_ASSERT(cap.hv_words == LAPLACE_HV_WORDS);
    LAPLACE_TEST_ASSERT(cap.max_predicates == LAPLACE_EXACT_MAX_PREDICATES);
    LAPLACE_TEST_ASSERT(cap.max_facts == LAPLACE_EXACT_MAX_FACTS);
    LAPLACE_TEST_ASSERT(cap.max_rules == LAPLACE_EXACT_MAX_RULES);
    LAPLACE_TEST_ASSERT(cap.max_arity == LAPLACE_EXACT_MAX_ARITY);
    LAPLACE_TEST_ASSERT(cap.max_rule_body_literals == LAPLACE_EXACT_MAX_RULE_BODY_LITERALS);
    LAPLACE_TEST_ASSERT(cap.max_rule_variables == LAPLACE_EXACT_MAX_RULE_VARIABLES);
    LAPLACE_TEST_ASSERT(cap.max_provenance_parents == LAPLACE_EXACT_MAX_PROVENANCE_PARENTS);
    LAPLACE_TEST_ASSERT(cap.max_branches == LAPLACE_BRANCH_MAX_BRANCHES);
    LAPLACE_TEST_ASSERT(cap.supported_artifact_kinds != 0);
    LAPLACE_TEST_ASSERT((cap.supported_artifact_kinds & LAPLACE_ADAPTER_ARTIFACT_RULE) != 0);
    LAPLACE_TEST_ASSERT((cap.supported_artifact_kinds & LAPLACE_ADAPTER_ARTIFACT_HV) != 0);
    LAPLACE_TEST_ASSERT((cap.supported_artifact_kinds & LAPLACE_ADAPTER_ARTIFACT_FACT) != 0);
    LAPLACE_TEST_ASSERT((cap.supported_artifact_kinds & LAPLACE_ADAPTER_ARTIFACT_VERIFY) != 0);
    LAPLACE_TEST_ASSERT(cap.reserved == 0);

    return 0;
}

/* ====================================================================
 * Test: version check
 * ==================================================================== */

static int test_adapter_version_check(void) {
    LAPLACE_TEST_ASSERT(laplace_adapter_check_version(LAPLACE_ADAPTER_ABI_VERSION) == true);
    LAPLACE_TEST_ASSERT(laplace_adapter_check_version(0) == false);
    LAPLACE_TEST_ASSERT(laplace_adapter_check_version(999) == false);
    return 0;
}

/* ====================================================================
 * Test: status strings
 * ==================================================================== */

static int test_adapter_status_strings(void) {
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string(LAPLACE_ADAPTER_OK) != NULL);
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string(LAPLACE_ADAPTER_ERR_NULL_ARGUMENT) != NULL);
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string(LAPLACE_ADAPTER_ERR_INVALID_VERSION) != NULL);
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string(LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH) != NULL);
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string(LAPLACE_ADAPTER_ERR_BATCH_OVERFLOW) != NULL);

    /* Verify strings are distinct. */
    LAPLACE_TEST_ASSERT(
        strcmp(laplace_adapter_status_string(LAPLACE_ADAPTER_OK),
               laplace_adapter_status_string(LAPLACE_ADAPTER_ERR_NULL_ARGUMENT)) != 0);

    /* Unknown status should still return a valid string. */
    LAPLACE_TEST_ASSERT(laplace_adapter_status_string((laplace_adapter_status_t)9999) != NULL);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - valid import
 * ==================================================================== */

static int test_adapter_rule_import_valid(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    /* Register predicates: parent/2, ancestor/2. */
    const laplace_predicate_id_t PARENT   = 1;
    const laplace_predicate_id_t ANCESTOR = 2;
    LAPLACE_TEST_ASSERT(register_binary_predicate(&f, PARENT) == 0);
    LAPLACE_TEST_ASSERT(register_binary_predicate(&f, ANCESTOR) == 0);

    /* Allocate constants. */
    laplace_entity_handle_t c_a, c_b;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c_a) == 0);
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c_b) == 0);

    /*
     * Rule: ancestor(X, Y) :- parent(X, Y).
     * Head: ancestor(X=var0, Y=var1)
     * Body: parent(X=var0, Y=var1)
     */
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version      = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count       = 1;
    artifact.compiler_id      = 42;
    artifact.compiler_version = 1;

    /* Head: ancestor(X=var1, Y=var2). */
    artifact.head.predicate_id = ANCESTOR;
    artifact.head.arity        = 2;
    artifact.head.terms[0]     = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};
    artifact.head.terms[1]     = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 2};

    /* Body[0]: parent(X=var1, Y=var2). */
    artifact.body[0].predicate_id = PARENT;
    artifact.body[0].arity        = 2;
    artifact.body[0].terms[0]     = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};
    artifact.body[0].terms[1]     = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 2};

    laplace_adapter_rule_import_result_t result;
    const laplace_adapter_status_t s =
        laplace_adapter_import_rule(&f.store, &artifact, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(result.rule_id != LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(result.validation_error == 0);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - invalid version rejection
 * ==================================================================== */

static int test_adapter_rule_import_invalid_version(void) {
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = 999;
    artifact.body_count  = 1;

    const laplace_adapter_status_t s =
        laplace_adapter_validate_rule_artifact(&artifact);
    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - invalid body count
 * ==================================================================== */

static int test_adapter_rule_import_invalid_body_count(void) {
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;

    /* Zero body count is invalid (rules require at least one body literal). */
    artifact.body_count = 0;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_rule_artifact(&artifact) == LAPLACE_ADAPTER_ERR_INVALID_FORMAT);

    /* Exceeding max body literals is invalid. */
    artifact.body_count = LAPLACE_EXACT_MAX_RULE_BODY_LITERALS + 1;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_rule_artifact(&artifact) == LAPLACE_ADAPTER_ERR_INVALID_FORMAT);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - invalid term kind
 * ==================================================================== */

static int test_adapter_rule_import_invalid_term(void) {
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count  = 1;
    artifact.head.predicate_id = 1;
    artifact.head.arity = 1;
    artifact.head.terms[0] = (laplace_adapter_term_t){.kind = 99, .value = 1};

    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_rule_artifact(&artifact) == LAPLACE_ADAPTER_ERR_INVALID_FORMAT);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - variable overflow (exceeds uint16_t)
 * ==================================================================== */

static int test_adapter_rule_import_variable_overflow(void) {
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count  = 1;
    artifact.head.predicate_id = 1;
    artifact.head.arity = 1;

    /* Variable value exceeding uint16_t range. */
    artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE,
        .value = (uint32_t)UINT16_MAX + 1u
    };
    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_rule_artifact(&artifact) == LAPLACE_ADAPTER_ERR_INVALID_FORMAT);

    /* Variable value 0 is LAPLACE_EXACT_VAR_ID_INVALID. */
    artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE,
        .value = 0
    };
    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_rule_artifact(&artifact) == LAPLACE_ADAPTER_ERR_INVALID_FORMAT);

    return 0;
}

/* ====================================================================
 * Test: rule artifact - undeclared predicate rejection (kernel validation)
 * ==================================================================== */

static int test_adapter_rule_import_undeclared_predicate(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    /* Do NOT register any predicates. */
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count  = 1;

    artifact.head.predicate_id = 50;
    artifact.head.arity        = 1;
    artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};

    artifact.body[0].predicate_id = 51;
    artifact.body[0].arity        = 1;
    artifact.body[0].terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};

    laplace_adapter_rule_import_result_t result;
    const laplace_adapter_status_t s =
        laplace_adapter_import_rule(&f.store, &artifact, &result);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_VALIDATION_FAILED);
    LAPLACE_TEST_ASSERT(result.status == LAPLACE_ADAPTER_ERR_VALIDATION_FAILED);
    LAPLACE_TEST_ASSERT(result.validation_error != 0);

    return 0;
}

/* ====================================================================
 * Test: HV header - valid
 * ==================================================================== */

static int test_adapter_hv_validate_valid(void) {
    laplace_adapter_hv_header_t header;
    memset(&header, 0, sizeof(header));
    header.abi_version   = LAPLACE_ADAPTER_ABI_VERSION;
    header.hv_dimension  = LAPLACE_HV_DIM;
    header.hv_words      = LAPLACE_HV_WORDS;
    header.encoder_id    = 7;
    header.encoder_seed  = 0xDEADBEEF;

    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_hv_header(&header) == LAPLACE_ADAPTER_OK);

    return 0;
}

/* ====================================================================
 * Test: HV header - wrong version
 * ==================================================================== */

static int test_adapter_hv_validate_wrong_version(void) {
    laplace_adapter_hv_header_t header;
    memset(&header, 0, sizeof(header));
    header.abi_version  = 0;
    header.hv_dimension = LAPLACE_HV_DIM;
    header.hv_words     = LAPLACE_HV_WORDS;

    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_hv_header(&header) == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    return 0;
}

/* ====================================================================
 * Test: HV header - wrong dimension
 * ==================================================================== */

static int test_adapter_hv_validate_wrong_dimension(void) {
    laplace_adapter_hv_header_t header;
    memset(&header, 0, sizeof(header));
    header.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    header.hv_dimension = 1024;
    header.hv_words     = 16;

    LAPLACE_TEST_ASSERT(
        laplace_adapter_validate_hv_header(&header) == LAPLACE_ADAPTER_ERR_DIMENSION_MISMATCH);

    return 0;
}

/* ====================================================================
 * Test: HV ingest - valid payload
 * ==================================================================== */

static int test_adapter_hv_ingest(void) {
    laplace_adapter_hv_header_t header;
    memset(&header, 0, sizeof(header));
    header.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    header.hv_dimension = LAPLACE_HV_DIM;
    header.hv_words     = LAPLACE_HV_WORDS;

    /* Create a test payload with known pattern. */
    static uint64_t words[LAPLACE_HV_WORDS];
    for (uint32_t i = 0; i < LAPLACE_HV_WORDS; ++i) {
        words[i] = (uint64_t)i * 0x0101010101010101ULL;
    }

    laplace_hv_t hv;
    laplace_hv_zero(&hv);

    LAPLACE_TEST_ASSERT(
        laplace_adapter_hv_ingest(&header, words, &hv) == LAPLACE_ADAPTER_OK);

    /* Verify words were copied correctly. */
    for (uint32_t i = 0; i < LAPLACE_HV_WORDS; ++i) {
        LAPLACE_TEST_ASSERT(hv.words[i] == words[i]);
    }

    return 0;
}

/* ====================================================================
 * Test: HV ingest - null rejection
 * ==================================================================== */

static int test_adapter_hv_ingest_null(void) {
    laplace_adapter_hv_header_t header;
    memset(&header, 0, sizeof(header));
    header.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    header.hv_dimension = LAPLACE_HV_DIM;
    header.hv_words     = LAPLACE_HV_WORDS;

    laplace_hv_t hv;

    LAPLACE_TEST_ASSERT(
        laplace_adapter_hv_ingest(NULL, NULL, &hv) == LAPLACE_ADAPTER_ERR_NULL_ARGUMENT);
    LAPLACE_TEST_ASSERT(
        laplace_adapter_hv_ingest(&header, NULL, &hv) == LAPLACE_ADAPTER_ERR_NULL_ARGUMENT);

    return 0;
}

/* ====================================================================
 * Test: fact injection - valid
 * ==================================================================== */

static int test_adapter_fact_inject_valid(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_entity_handle_t c;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c) == 0);

    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    req.predicate_id    = PRED;
    req.arg_count       = 1;
    req.correlation_id  = 0x1234;
    req.args[0].id         = c.id;
    req.args[0].generation = c.generation;

    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&f.store, &req, &resp);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(resp.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(resp.inserted == true);
    LAPLACE_TEST_ASSERT(resp.fact_row != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(resp.entity_id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(resp.provenance_id != LAPLACE_PROVENANCE_ID_INVALID);
    LAPLACE_TEST_ASSERT(resp.correlation_id == 0x1234);

    return 0;
}

/* ====================================================================
 * Test: fact injection - duplicate detection
 * ==================================================================== */

static int test_adapter_fact_inject_duplicate(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_entity_handle_t c;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c) == 0);

    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    req.predicate_id    = PRED;
    req.arg_count       = 1;
    req.correlation_id  = 100;
    req.args[0].id         = c.id;
    req.args[0].generation = c.generation;

    /* First injection — should succeed. */
    laplace_adapter_fact_response_t resp1;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_inject_fact(&f.store, &req, &resp1) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(resp1.inserted == true);

    /* Second injection of the same fact — duplicate. */
    req.correlation_id = 200;
    laplace_adapter_fact_response_t resp2;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_inject_fact(&f.store, &req, &resp2) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(resp2.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(resp2.inserted == false);
    LAPLACE_TEST_ASSERT(resp2.correlation_id == 200);

    return 0;
}

/* ====================================================================
 * Test: fact injection - invalid predicate
 * ==================================================================== */

static int test_adapter_fact_inject_invalid_predicate(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    /* Do NOT register any predicates. */
    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    req.predicate_id = 99;
    req.arg_count    = 1;

    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&f.store, &req, &resp);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_PREDICATE_INVALID);
    LAPLACE_TEST_ASSERT(resp.status == LAPLACE_ADAPTER_ERR_PREDICATE_INVALID);

    return 0;
}

/* ====================================================================
 * Test: fact injection - arity mismatch
 * ==================================================================== */

static int test_adapter_fact_inject_arity_mismatch(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    req.predicate_id = PRED;
    req.arg_count    = 2;  /* arity=1 predicate, but sending 2 args */

    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&f.store, &req, &resp);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_ARITY_MISMATCH);

    return 0;
}

/* ====================================================================
 * Test: fact injection - invalid entity reference
 * ==================================================================== */

static int test_adapter_fact_inject_invalid_entity(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    req.predicate_id = PRED;
    req.arg_count    = 1;
    /* Bogus entity reference. */
    req.args[0].id         = 9999;
    req.args[0].generation = 9999;

    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&f.store, &req, &resp);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_ENTITY_INVALID);

    return 0;
}

/* ====================================================================
 * Test: fact injection - invalid version
 * ==================================================================== */

static int test_adapter_fact_inject_invalid_version(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    laplace_adapter_fact_request_t req;
    memset(&req, 0, sizeof(req));
    req.abi_version = 0;

    laplace_adapter_fact_response_t resp;
    const laplace_adapter_status_t s =
        laplace_adapter_inject_fact(&f.store, &req, &resp);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    return 0;
}

/* ====================================================================
 * Test: fact injection - batch
 * ==================================================================== */

static int test_adapter_fact_inject_batch(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    /* Allocate 3 constants. */
    laplace_entity_handle_t entities[3];
    for (int i = 0; i < 3; ++i) {
        LAPLACE_TEST_ASSERT(alloc_constant(&f, &entities[i]) == 0);
    }

    /* Create batch of 3 fact requests. */
    laplace_adapter_fact_request_t reqs[3];
    memset(reqs, 0, sizeof(reqs));

    for (uint32_t i = 0; i < 3; ++i) {
        reqs[i].abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
        reqs[i].predicate_id    = PRED;
        reqs[i].arg_count       = 1;
        reqs[i].correlation_id  = (uint64_t)(i + 1) * 100;
        reqs[i].args[0].id         = entities[i].id;
        reqs[i].args[0].generation = entities[i].generation;
    }

    laplace_adapter_fact_response_t resps[3];
    const laplace_adapter_status_t s =
        laplace_adapter_inject_facts_batch(&f.store, reqs, 3, resps);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_OK);

    for (uint32_t i = 0; i < 3; ++i) {
        LAPLACE_TEST_ASSERT(resps[i].status == LAPLACE_ADAPTER_OK);
        LAPLACE_TEST_ASSERT(resps[i].inserted == true);
        LAPLACE_TEST_ASSERT(resps[i].correlation_id == (uint64_t)(i + 1) * 100);
    }

    return 0;
}

/* ====================================================================
 * Test: fact injection - batch overflow
 * ==================================================================== */

static int test_adapter_fact_inject_batch_overflow(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    laplace_adapter_fact_request_t reqs[1];
    laplace_adapter_fact_response_t resps[1];

    /* Count exceeds batch max. */
    const laplace_adapter_status_t s = laplace_adapter_inject_facts_batch(
        &f.store, reqs, LAPLACE_ADAPTER_FACT_BATCH_MAX + 1, resps);

    LAPLACE_TEST_ASSERT(s == LAPLACE_ADAPTER_ERR_BATCH_OVERFLOW);

    return 0;
}

/* ====================================================================
 * Test: verifier - fact exists (found)
 * ==================================================================== */

static int test_adapter_verify_fact_exists_found(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_entity_handle_t c;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c) == 0);

    /* Inject a fact via the adapter. */
    laplace_adapter_fact_request_t freq;
    memset(&freq, 0, sizeof(freq));
    freq.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    freq.predicate_id    = PRED;
    freq.arg_count       = 1;
    freq.args[0].id         = c.id;
    freq.args[0].generation = c.generation;

    laplace_adapter_fact_response_t fresp;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_inject_fact(&f.store, &freq, &fresp) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(fresp.inserted == true);

    /* Now verify it exists. */
    laplace_adapter_verify_fact_query_t vq;
    memset(&vq, 0, sizeof(vq));
    vq.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    vq.predicate_id    = PRED;
    vq.arg_count       = 1;
    vq.correlation_id  = 555;
    vq.args[0]         = c.id;

    laplace_adapter_verify_fact_result_t vr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_fact_exists(&f.store, &vq, &vr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(vr.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(vr.found == true);
    LAPLACE_TEST_ASSERT(vr.fact_row != LAPLACE_EXACT_FACT_ROW_INVALID);
    LAPLACE_TEST_ASSERT(vr.provenance_id != LAPLACE_PROVENANCE_ID_INVALID);
    LAPLACE_TEST_ASSERT(vr.correlation_id == 555);

    return 0;
}

/* ====================================================================
 * Test: verifier - fact exists (not found)
 * ==================================================================== */

static int test_adapter_verify_fact_exists_not_found(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_entity_handle_t c;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c) == 0);

    /* Query for a fact that was never asserted. */
    laplace_adapter_verify_fact_query_t vq;
    memset(&vq, 0, sizeof(vq));
    vq.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    vq.predicate_id    = PRED;
    vq.arg_count       = 1;
    vq.correlation_id  = 777;
    vq.args[0]         = c.id;

    laplace_adapter_verify_fact_result_t vr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_fact_exists(&f.store, &vq, &vr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(vr.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(vr.found == false);
    LAPLACE_TEST_ASSERT(vr.correlation_id == 777);

    return 0;
}

/* ====================================================================
 * Test: verifier - provenance query
 * ==================================================================== */

static int test_adapter_verify_provenance(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_entity_handle_t c;
    LAPLACE_TEST_ASSERT(alloc_constant(&f, &c) == 0);

    /* Inject a fact to get a provenance record. */
    laplace_adapter_fact_request_t freq;
    memset(&freq, 0, sizeof(freq));
    freq.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    freq.predicate_id    = PRED;
    freq.arg_count       = 1;
    freq.args[0].id         = c.id;
    freq.args[0].generation = c.generation;

    laplace_adapter_fact_response_t fresp;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_inject_fact(&f.store, &freq, &fresp) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(fresp.inserted == true);

    /* Query the provenance. */
    laplace_adapter_verify_provenance_query_t pq;
    memset(&pq, 0, sizeof(pq));
    pq.abi_version     = LAPLACE_ADAPTER_ABI_VERSION;
    pq.provenance_id   = fresp.provenance_id;
    pq.correlation_id  = 888;

    laplace_adapter_verify_provenance_result_t pr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_provenance(&f.store, &pq, &pr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(pr.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(pr.found == true);
    LAPLACE_TEST_ASSERT(pr.kind == (uint8_t)LAPLACE_EXACT_PROVENANCE_ASSERTED);
    LAPLACE_TEST_ASSERT(pr.source_rule_id == LAPLACE_RULE_ID_INVALID);
    LAPLACE_TEST_ASSERT(pr.parent_count == 0);
    LAPLACE_TEST_ASSERT(pr.correlation_id == 888);

    return 0;
}

/* ====================================================================
 * Test: verifier - provenance not found
 * ==================================================================== */

static int test_adapter_verify_provenance_not_found(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    laplace_adapter_verify_provenance_query_t pq;
    memset(&pq, 0, sizeof(pq));
    pq.abi_version   = LAPLACE_ADAPTER_ABI_VERSION;
    pq.provenance_id = 9999;
    pq.correlation_id = 111;

    laplace_adapter_verify_provenance_result_t pr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_provenance(&f.store, &pq, &pr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(pr.found == false);
    LAPLACE_TEST_ASSERT(pr.correlation_id == 111);

    return 0;
}

/* ====================================================================
 * Test: verifier - rule status query
 * ==================================================================== */

static int test_adapter_verify_rule(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    const laplace_predicate_id_t P = 1;
    const laplace_predicate_id_t Q = 2;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, P) == 0);
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, Q) == 0);

    /* Import a rule via the adapter: Q(X) :- P(X). */
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count  = 1;
    artifact.head.predicate_id = Q;
    artifact.head.arity = 1;
    artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};
    artifact.body[0].predicate_id = P;
    artifact.body[0].arity = 1;
    artifact.body[0].terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};

    laplace_adapter_rule_import_result_t import_result;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_import_rule(&f.store, &artifact, &import_result) == LAPLACE_ADAPTER_OK);

    /* Query the rule via the verifier. */
    laplace_adapter_verify_rule_query_t rq;
    memset(&rq, 0, sizeof(rq));
    rq.abi_version    = LAPLACE_ADAPTER_ABI_VERSION;
    rq.rule_id        = import_result.rule_id;
    rq.correlation_id = 999;

    laplace_adapter_verify_rule_result_t rr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_rule(&f.store, &rq, &rr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(rr.status == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(rr.found == true);
    LAPLACE_TEST_ASSERT(rr.rule_status == (uint8_t)LAPLACE_EXACT_RULE_STATUS_VALID);
    LAPLACE_TEST_ASSERT(rr.head_predicate_id == Q);
    LAPLACE_TEST_ASSERT(rr.head_arity == 1);
    LAPLACE_TEST_ASSERT(rr.body_count == 1);
    LAPLACE_TEST_ASSERT(rr.correlation_id == 999);

    return 0;
}

/* ====================================================================
 * Test: verifier - rule not found
 * ==================================================================== */

static int test_adapter_verify_rule_not_found(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    laplace_adapter_verify_rule_query_t rq;
    memset(&rq, 0, sizeof(rq));
    rq.abi_version    = LAPLACE_ADAPTER_ABI_VERSION;
    rq.rule_id        = 9999;
    rq.correlation_id = 222;

    laplace_adapter_verify_rule_result_t rr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_rule(&f.store, &rq, &rr) == LAPLACE_ADAPTER_OK);
    LAPLACE_TEST_ASSERT(rr.found == false);
    LAPLACE_TEST_ASSERT(rr.correlation_id == 222);

    return 0;
}

/* ====================================================================
 * Test: verifier - invalid version
 * ==================================================================== */

static int test_adapter_verify_invalid_version(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    laplace_adapter_verify_fact_query_t vq;
    memset(&vq, 0, sizeof(vq));
    vq.abi_version = 0;

    laplace_adapter_verify_fact_result_t vr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_fact_exists(&f.store, &vq, &vr) == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    laplace_adapter_verify_provenance_query_t pq;
    memset(&pq, 0, sizeof(pq));
    pq.abi_version = 0;

    laplace_adapter_verify_provenance_result_t pr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_provenance(&f.store, &pq, &pr) == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    laplace_adapter_verify_rule_query_t rq;
    memset(&rq, 0, sizeof(rq));
    rq.abi_version = 0;

    laplace_adapter_verify_rule_result_t rr;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_verify_rule(&f.store, &rq, &rr) == LAPLACE_ADAPTER_ERR_INVALID_VERSION);

    return 0;
}

/* ====================================================================
 * Test: no semantic bypass
 *
 * Verifies that adapter-injected rules and facts still pass through
 * full kernel validation — the adapter cannot bypass exact semantics.
 * ==================================================================== */

static int test_adapter_no_semantic_bypass(void) {
    test_adapter_fixture_t f;
    LAPLACE_TEST_ASSERT(adapter_fixture_init(&f) == 0);

    /*
     * Attempt 1: Import a rule referencing undeclared predicates.
     * The kernel's exact validation must reject it even though the
     * adapter-level format is valid.
     */
    laplace_adapter_rule_artifact_t artifact;
    memset(&artifact, 0, sizeof(artifact));
    artifact.abi_version = LAPLACE_ADAPTER_ABI_VERSION;
    artifact.body_count  = 1;
    artifact.head.predicate_id = 100;
    artifact.head.arity = 1;
    artifact.head.terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};
    artifact.body[0].predicate_id = 101;
    artifact.body[0].arity = 1;
    artifact.body[0].terms[0] = (laplace_adapter_term_t){
        .kind = LAPLACE_EXACT_TERM_VARIABLE, .value = 1};

    laplace_adapter_rule_import_result_t rresult;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_import_rule(&f.store, &artifact, &rresult) ==
        LAPLACE_ADAPTER_ERR_VALIDATION_FAILED);

    /*
     * Attempt 2: Inject a fact with a non-existent entity.
     * The adapter's entity validation must reject it.
     */
    const laplace_predicate_id_t PRED = 1;
    LAPLACE_TEST_ASSERT(register_unary_predicate(&f, PRED) == 0);

    laplace_adapter_fact_request_t freq;
    memset(&freq, 0, sizeof(freq));
    freq.abi_version  = LAPLACE_ADAPTER_ABI_VERSION;
    freq.predicate_id = PRED;
    freq.arg_count    = 1;
    freq.args[0].id         = 50000;
    freq.args[0].generation = 1;

    laplace_adapter_fact_response_t fresp;
    LAPLACE_TEST_ASSERT(
        laplace_adapter_inject_fact(&f.store, &freq, &fresp) ==
        LAPLACE_ADAPTER_ERR_ENTITY_INVALID);

    return 0;
}

/* ====================================================================
 * Test: struct size stability
 *
 * Verifies that all adapter artifact struct sizes match expectations.
 * This acts as a compile-time + runtime ABI stability check.
 * ==================================================================== */

static int test_adapter_format_stability(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_capability_t) == 64);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_term_t) == 8);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_literal_t) ==
                         4u + LAPLACE_EXACT_MAX_ARITY * 8u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_rule_import_result_t) == 24);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_hv_header_t) == 32);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_hv_result_t) == 8);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_entity_ref_t) == 8);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_fact_response_t) == 32);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_fact_query_t) ==
                         16u + LAPLACE_EXACT_MAX_ARITY * 4u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_fact_result_t) == 32);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_provenance_query_t) == 16);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_provenance_result_t) == 64);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_rule_query_t) == 16);
    LAPLACE_TEST_ASSERT(sizeof(laplace_adapter_verify_rule_result_t) == 24);

    return 0;
}

/* ====================================================================
 * Test entry point
 * ==================================================================== */

int laplace_test_adapter(void) {
    typedef int (*subtest_fn)(void);
    typedef struct {
        const char* name;
        subtest_fn  fn;
    } subtest_t;

    const subtest_t subtests[] = {
        {"capability_query",               test_adapter_capability},
        {"version_check",                  test_adapter_version_check},
        {"status_strings",                 test_adapter_status_strings},
        {"rule_import_valid",              test_adapter_rule_import_valid},
        {"rule_import_invalid_version",    test_adapter_rule_import_invalid_version},
        {"rule_import_invalid_body_count", test_adapter_rule_import_invalid_body_count},
        {"rule_import_invalid_term",       test_adapter_rule_import_invalid_term},
        {"rule_import_variable_overflow",  test_adapter_rule_import_variable_overflow},
        {"rule_import_undeclared_predicate",test_adapter_rule_import_undeclared_predicate},
        {"hv_validate_valid",              test_adapter_hv_validate_valid},
        {"hv_validate_wrong_version",      test_adapter_hv_validate_wrong_version},
        {"hv_validate_wrong_dimension",    test_adapter_hv_validate_wrong_dimension},
        {"hv_ingest",                      test_adapter_hv_ingest},
        {"hv_ingest_null",                 test_adapter_hv_ingest_null},
        {"fact_inject_valid",              test_adapter_fact_inject_valid},
        {"fact_inject_duplicate",          test_adapter_fact_inject_duplicate},
        {"fact_inject_invalid_predicate",  test_adapter_fact_inject_invalid_predicate},
        {"fact_inject_arity_mismatch",     test_adapter_fact_inject_arity_mismatch},
        {"fact_inject_invalid_entity",     test_adapter_fact_inject_invalid_entity},
        {"fact_inject_invalid_version",    test_adapter_fact_inject_invalid_version},
        {"fact_inject_batch",              test_adapter_fact_inject_batch},
        {"fact_inject_batch_overflow",     test_adapter_fact_inject_batch_overflow},
        {"verify_fact_exists_found",       test_adapter_verify_fact_exists_found},
        {"verify_fact_exists_not_found",   test_adapter_verify_fact_exists_not_found},
        {"verify_provenance",             test_adapter_verify_provenance},
        {"verify_provenance_not_found",   test_adapter_verify_provenance_not_found},
        {"verify_rule",                   test_adapter_verify_rule},
        {"verify_rule_not_found",         test_adapter_verify_rule_not_found},
        {"verify_invalid_version",        test_adapter_verify_invalid_version},
        {"no_semantic_bypass",            test_adapter_no_semantic_bypass},
        {"format_stability",              test_adapter_format_stability},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    int failures = 0;

    for (size_t i = 0; i < count; ++i) {
        const int result = subtests[i].fn();
        if (result != 0) {
            fprintf(stderr, "  [FAIL] adapter/%s\n", subtests[i].name);
            ++failures;
        }
    }

    if (failures > 0) {
        fprintf(stderr, "  adapter: %d/%zu subtests failed\n", failures, count);
    }

    return failures;
}
