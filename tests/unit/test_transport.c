#include <string.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/transport.h"
#include "laplace/transport_dispatch.h"
#include "test_harness.h"

static void transport_build_command(laplace_transport_command_record_t* cmd,
                                     laplace_transport_cmd_kind_t kind,
                                     laplace_transport_correlation_id_t corr_id,
                                     const void* payload,
                                     uint32_t payload_size) {
    memset(cmd, 0, sizeof(*cmd));
    cmd->kind           = (uint32_t)kind;
    cmd->correlation_id = corr_id;
    cmd->payload_size   = payload_size;
    cmd->flags          = 0u;
    cmd->sequence       = 0u;
    if (payload != NULL && payload_size > 0u) {
        memcpy(cmd->payload, payload, payload_size);
    }
}

static int test_transport_create_close(void) {
    laplace_transport_mapping_t mapping;
    laplace_error_t err = laplace_transport_create(&mapping, "Local\\LaplaceTest_CreateClose");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(mapping.view != NULL);
    LAPLACE_TEST_ASSERT(mapping.backend_handle != NULL);
    LAPLACE_TEST_ASSERT(mapping.is_creator == true);
    LAPLACE_TEST_ASSERT(mapping.total_size == (uint32_t)LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE);

    /* Validate header */
    LAPLACE_TEST_ASSERT(laplace_transport_validate_header(&mapping));

    const laplace_transport_mapping_header_t* header = laplace_transport_get_header(&mapping);
    LAPLACE_TEST_ASSERT(header->magic == LAPLACE_TRANSPORT_MAGIC);
    LAPLACE_TEST_ASSERT(header->abi_version == LAPLACE_TRANSPORT_ABI_VERSION);
    LAPLACE_TEST_ASSERT(header->endian == LAPLACE_TRANSPORT_NATIVE_ENDIAN);
    LAPLACE_TEST_ASSERT(header->ingress_capacity == LAPLACE_TRANSPORT_INGRESS_CAPACITY);
    LAPLACE_TEST_ASSERT(header->egress_capacity == LAPLACE_TRANSPORT_EGRESS_CAPACITY);

    laplace_transport_close(&mapping);
    LAPLACE_TEST_ASSERT(mapping.view == NULL);
    LAPLACE_TEST_ASSERT(mapping.backend_handle == NULL);
    return 0;
}

static int test_transport_create_open_close(void) {
    laplace_transport_mapping_t creator;
    laplace_error_t err = laplace_transport_create(&creator, "Local\\LaplaceTest_OpenClose");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

    /* Open existing mapping */
    laplace_transport_mapping_t opener;
    err = laplace_transport_open(&opener, "Local\\LaplaceTest_OpenClose");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(opener.view != NULL);
    LAPLACE_TEST_ASSERT(opener.is_creator == false);
    LAPLACE_TEST_ASSERT(laplace_transport_validate_header(&opener));

    laplace_transport_close(&opener);
    laplace_transport_close(&creator);
    return 0;
}

static int test_transport_null_args(void) {
    laplace_error_t err = laplace_transport_create(NULL, "Local\\LaplaceTest_Null");
    LAPLACE_TEST_ASSERT(err == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_transport_mapping_t mapping;
    err = laplace_transport_create(&mapping, NULL);
    LAPLACE_TEST_ASSERT(err == LAPLACE_ERR_INVALID_ARGUMENT);

    err = laplace_transport_open(NULL, "Local\\LaplaceTest_Null");
    LAPLACE_TEST_ASSERT(err == LAPLACE_ERR_INVALID_ARGUMENT);

    /* Close on zeroed mapping is safe */
    memset(&mapping, 0, sizeof(mapping));
    laplace_transport_close(&mapping);
    laplace_transport_close(NULL);
    return 0;
}

static int test_transport_ingress_ring_basic(void) {
    laplace_transport_mapping_t mapping;
    laplace_error_t err = laplace_transport_create(&mapping, "Local\\LaplaceTest_IngressBasic");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

    const laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(&mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));
    LAPLACE_TEST_ASSERT(laplace_transport_ring_count(ring) == 0u);

    /* Enqueue a PING command */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, 42u, NULL, 0u);
    err = laplace_transport_ingress_enqueue(&mapping, &cmd);
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_count(ring) == 1u);
    LAPLACE_TEST_ASSERT(!laplace_transport_ring_is_empty(ring));

    /* Dequeue */
    laplace_transport_command_record_t out;
    err = laplace_transport_ingress_dequeue(&mapping, &out);
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(out.kind == (uint32_t)LAPLACE_TRANSPORT_CMD_PING);
    LAPLACE_TEST_ASSERT(out.correlation_id == 42u);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    /* Dequeue on empty ring */
    err = laplace_transport_ingress_dequeue(&mapping, &out);
    LAPLACE_TEST_ASSERT(err == LAPLACE_ERR_INVALID_STATE);

    laplace_transport_close(&mapping);
    return 0;
}

static int test_transport_egress_ring_basic(void) {
    laplace_transport_mapping_t mapping;
    laplace_error_t err = laplace_transport_create(&mapping, "Local\\LaplaceTest_EgressBasic");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

    const laplace_transport_ring_header_t* ring = laplace_transport_get_egress_ring(&mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    /* Enqueue an ACK event */
    laplace_transport_event_record_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.kind           = (uint32_t)LAPLACE_TRANSPORT_EVT_ACK;
    evt.status         = (uint32_t)LAPLACE_TRANSPORT_STATUS_OK;
    evt.correlation_id = 99u;
    evt.payload_size   = 0u;
    evt.sequence       = 1u;

    err = laplace_transport_egress_enqueue(&mapping, &evt);
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_count(ring) == 1u);

    /* Dequeue */
    laplace_transport_event_record_t out_evt;
    err = laplace_transport_egress_dequeue(&mapping, &out_evt);
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(out_evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(out_evt.correlation_id == 99u);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    laplace_transport_close(&mapping);
    return 0;
}

static int test_transport_ring_full(void) {
    laplace_transport_mapping_t mapping;
    laplace_error_t err = laplace_transport_create(&mapping, "Local\\LaplaceTest_RingFull");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

    /* Fill ingress ring to capacity */
    laplace_transport_command_record_t cmd;
    for (uint32_t i = 0u; i < LAPLACE_TRANSPORT_INGRESS_CAPACITY; ++i) {
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, (uint64_t)i, NULL, 0u);
        err = laplace_transport_ingress_enqueue(&mapping, &cmd);
        LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
    }

    const laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(&mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_full(ring, LAPLACE_TRANSPORT_INGRESS_CAPACITY));

    /* Enqueue on full ring must fail */
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, 999u, NULL, 0u);
    err = laplace_transport_ingress_enqueue(&mapping, &cmd);
    LAPLACE_TEST_ASSERT(err == LAPLACE_ERR_CAPACITY_EXHAUSTED);

    /* Dequeue all, verify ordering */
    laplace_transport_command_record_t out;
    for (uint32_t i = 0u; i < LAPLACE_TRANSPORT_INGRESS_CAPACITY; ++i) {
        err = laplace_transport_ingress_dequeue(&mapping, &out);
        LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
        LAPLACE_TEST_ASSERT(out.correlation_id == (uint64_t)i);
    }
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    laplace_transport_close(&mapping);
    return 0;
}

static int test_transport_ring_wraparound(void) {
    laplace_transport_mapping_t mapping;
    laplace_error_t err = laplace_transport_create(&mapping, "Local\\LaplaceTest_RingWrap");
    LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

    /* Enqueue/dequeue many times to test wraparound */
    laplace_transport_command_record_t cmd;
    laplace_transport_command_record_t out;
    const uint32_t total = LAPLACE_TRANSPORT_INGRESS_CAPACITY * 3u;

    for (uint32_t i = 0u; i < total; ++i) {
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, (uint64_t)i, NULL, 0u);
        err = laplace_transport_ingress_enqueue(&mapping, &cmd);
        LAPLACE_TEST_ASSERT(err == LAPLACE_OK);

        err = laplace_transport_ingress_dequeue(&mapping, &out);
        LAPLACE_TEST_ASSERT(err == LAPLACE_OK);
        LAPLACE_TEST_ASSERT(out.correlation_id == (uint64_t)i);
    }

    const laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(&mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    laplace_transport_close(&mapping);
    return 0;
}

#define TRANSPORT_TEST_ARENA_SIZE (4u * 1024u * 1024u)
#define TRANSPORT_TEST_ENTITY_CAPACITY 512u

static uint8_t transport_test_arena_buf[TRANSPORT_TEST_ARENA_SIZE];

typedef struct transport_test_env {
    laplace_arena_t              arena;
    laplace_entity_pool_t        entity_pool;
    laplace_exact_store_t        store;
    laplace_exec_context_t       exec_ctx;
    laplace_transport_mapping_t  mapping;
    laplace_transport_context_t  ctx;
} transport_test_env_t;

static int transport_test_env_init(transport_test_env_t* env, const char* name) {
    memset(env, 0, sizeof(*env));

    laplace_error_t err = laplace_arena_init(&env->arena, transport_test_arena_buf, TRANSPORT_TEST_ARENA_SIZE);
    if (err != LAPLACE_OK) return 1;

    err = laplace_entity_pool_init(&env->entity_pool, &env->arena, TRANSPORT_TEST_ENTITY_CAPACITY);
    if (err != LAPLACE_OK) return 1;

    err = laplace_exact_store_init(&env->store, &env->arena, &env->entity_pool);
    if (err != LAPLACE_OK) return 1;

    err = laplace_exec_init(&env->exec_ctx, &env->arena, &env->store, &env->entity_pool);
    if (err != LAPLACE_OK) return 1;

    err = laplace_transport_create(&env->mapping, name);
    if (err != LAPLACE_OK) return 1;

    err = laplace_transport_ctx_init(&env->ctx, &env->mapping, &env->store,
                                      &env->exec_ctx, &env->entity_pool);
    if (err != LAPLACE_OK) return 1;

    return 0;
}

static void transport_test_env_cleanup(transport_test_env_t* env) {
    laplace_transport_close(&env->mapping);
    laplace_arena_reset(&env->arena);
}

static int test_transport_ping(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_Ping") == 0);

    /* Enqueue PING */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, 100u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);

    /* Process */
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check egress */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 100u);

    const laplace_transport_stats_t* stats = laplace_transport_get_stats(&env.ctx);
    LAPLACE_TEST_ASSERT(stats->commands_processed == 1u);
    LAPLACE_TEST_ASSERT(stats->ping_count == 1u);
    LAPLACE_TEST_ASSERT(stats->events_emitted == 1u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_register_predicate(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_RegPred") == 0);

    /* Register predicate 1 with arity 2 */
    laplace_transport_cmd_register_predicate_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.predicate_id  = 1u;
    payload.arity         = 2u;
    payload.flags         = 0u;
    payload.fact_capacity = 256u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_REGISTER_PREDICATE, 200u,
                             &payload, (uint32_t)sizeof(payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check ACK */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 200u);

    /* Verify predicate is actually registered */
    LAPLACE_TEST_ASSERT(laplace_exact_predicate_is_declared(&env.store, 1u));
    LAPLACE_TEST_ASSERT(laplace_exact_predicate_arity(&env.store, 1u) == 2u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_register_constant(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_RegConst") == 0);

    /* Allocate an entity first */
    laplace_entity_handle_t ent = laplace_entity_pool_alloc(&env.entity_pool);
    LAPLACE_TEST_ASSERT(ent.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&env.entity_pool, ent, LAPLACE_STATE_READY) == LAPLACE_OK);

    /* Register constant via transport */
    laplace_transport_cmd_register_constant_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.entity_id  = (uint32_t)ent.id;
    payload.generation = (uint32_t)ent.generation;
    payload.type_id    = 1u;
    payload.flags      = 0u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_REGISTER_CONSTANT, 300u,
                             &payload, (uint32_t)sizeof(payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check ACK */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 300u);

    /* Verify constant registered */
    LAPLACE_TEST_ASSERT(laplace_exact_is_constant_entity(&env.store, ent.id));

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_assert_fact(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_AssertFact") == 0);

    /* Register predicate(1, arity=2) */
    {
        laplace_exact_predicate_desc_t desc = {.arity = 2, .flags = 0, .fact_capacity = 256};
        LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&env.store, 1u, &desc) == LAPLACE_OK);
    }

    /* Allocate two constants */
    laplace_entity_handle_t c1 = laplace_entity_pool_alloc(&env.entity_pool);
    laplace_entity_handle_t c2 = laplace_entity_pool_alloc(&env.entity_pool);
    LAPLACE_TEST_ASSERT(c1.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(c2.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&env.entity_pool, c1, LAPLACE_STATE_READY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&env.entity_pool, c2, LAPLACE_STATE_READY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_constant(&env.store, c1, 1u, 0u) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_exact_register_constant(&env.store, c2, 1u, 0u) == LAPLACE_OK);

    /* Assert fact(c1, c2) via transport */
    laplace_transport_cmd_assert_fact_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.predicate_id = 1u;
    payload.arg_count    = 2u;
    payload.flags        = 0u;
    payload.args[0]      = (uint32_t)c1.id;
    payload.args[1]      = (uint32_t)c2.id;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_ASSERT_FACT, 400u,
                             &payload, (uint32_t)sizeof(payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check FACT_COMMITTED event */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_FACT_COMMITTED);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 400u);

    laplace_transport_evt_fact_committed_t fc;
    memcpy(&fc, evt.payload, sizeof(fc));
    LAPLACE_TEST_ASSERT(fc.predicate_id == 1u);
    LAPLACE_TEST_ASSERT(fc.arg_count == 2u);
    LAPLACE_TEST_ASSERT(fc.inserted == 1u);
    LAPLACE_TEST_ASSERT(fc.entity_id != LAPLACE_ENTITY_ID_INVALID);

    /* Verify fact in store */
    laplace_entity_id_t args[2] = {c1.id, c2.id};
    laplace_exact_fact_row_t row = laplace_exact_find_fact(&env.store, 1u, args, 2u);
    LAPLACE_TEST_ASSERT(row != LAPLACE_EXACT_FACT_ROW_INVALID);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_assert_fact_invalid_predicate(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_FactInvPred") == 0);

    /* Assert fact on undeclared predicate */
    laplace_transport_cmd_assert_fact_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.predicate_id = 99u;
    payload.arg_count    = 1u;
    payload.args[0]      = 1u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_ASSERT_FACT, 401u,
                             &payload, (uint32_t)sizeof(payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check ERROR event */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ERROR);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PREDICATE);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 401u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_unknown_command(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_UnkCmd") == 0);

    /* Send unknown command kind */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, (laplace_transport_cmd_kind_t)255u, 500u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check ERROR event */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ERROR);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_ERR_UNKNOWN_CMD);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 500u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_query_stats(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_Stats") == 0);

    /* Register a predicate to have non-zero stats */
    {
        laplace_exact_predicate_desc_t desc = {.arity = 1, .flags = 0, .fact_capacity = 256};
        LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&env.store, 1u, &desc) == LAPLACE_OK);
    }

    /* Query stats via transport */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_QUERY_STATS, 600u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check STATS_SNAPSHOT */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_STATS_SNAPSHOT);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 600u);

    laplace_transport_evt_stats_snapshot_t snap;
    memcpy(&snap, evt.payload, sizeof(snap));
    LAPLACE_TEST_ASSERT(snap.predicate_count == 1u);
    LAPLACE_TEST_ASSERT(snap.entity_capacity == TRANSPORT_TEST_ENTITY_CAPACITY);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_build_trigger_index(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_BuildTrig") == 0);

    /* Build trigger index (no rules, should succeed) */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_BUILD_TRIGGER_INDEX, 700u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 700u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_exec_step_no_ready(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_ExecStep") == 0);

    /* Build trigger index first */
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&env.exec_ctx) == LAPLACE_OK);

    /* Exec step with no ready entities */
    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_EXEC_STEP, 800u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_EXEC_STATUS);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_ERR_EXEC_ERROR);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 800u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_exec_run_fixpoint(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_ExecRun") == 0);

    /* Build trigger index */
    LAPLACE_TEST_ASSERT(laplace_exec_build_trigger_index(&env.exec_ctx) == LAPLACE_OK);

    /* Exec run with no ready entities → fixpoint immediately */
    laplace_transport_cmd_exec_run_t run_payload;
    memset(&run_payload, 0, sizeof(run_payload));
    run_payload.max_steps       = 100u;
    run_payload.max_derivations = 100u;
    run_payload.mode            = 0u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_EXEC_RUN, 900u,
                             &run_payload, (uint32_t)sizeof(run_payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_EXEC_STATUS);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 900u);

    laplace_transport_evt_exec_status_t exec_status;
    memcpy(&exec_status, evt.payload, sizeof(exec_status));
    LAPLACE_TEST_ASSERT(exec_status.run_status == (uint32_t)LAPLACE_EXEC_RUN_FIXPOINT);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_batch_processing(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_Batch") == 0);

    /* Enqueue 5 PINGs */
    laplace_transport_command_record_t cmd;
    for (uint32_t i = 0u; i < 5u; ++i) {
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, (uint64_t)(1000u + i), NULL, 0u);
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }

    /* Process batch */
    const uint32_t processed = laplace_transport_process_batch(&env.ctx, 10u);
    LAPLACE_TEST_ASSERT(processed == 5u);

    /* Dequeue all events and verify */
    laplace_transport_event_record_t evt;
    for (uint32_t i = 0u; i < 5u; ++i) {
        LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
        LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
        LAPLACE_TEST_ASSERT(evt.correlation_id == (uint64_t)(1000u + i));
    }

    const laplace_transport_stats_t* stats = laplace_transport_get_stats(&env.ctx);
    LAPLACE_TEST_ASSERT(stats->commands_processed == 5u);
    LAPLACE_TEST_ASSERT(stats->ping_count == 5u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_drain(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_Drain") == 0);

    /* Enqueue 3 PINGs */
    laplace_transport_command_record_t cmd;
    for (uint32_t i = 0u; i < 3u; ++i) {
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_PING, (uint64_t)(2000u + i), NULL, 0u);
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }

    /* Drain */
    const uint32_t processed = laplace_transport_drain(&env.ctx);
    LAPLACE_TEST_ASSERT(processed == 3u);

    /* Ingress should be empty */
    const laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(&env.mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(ring));

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_add_rule_accepted(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_AddRule") == 0);

    /* Register predicates: parent(2), ancestor(2) */
    {
        laplace_exact_predicate_desc_t desc = {.arity = 2, .flags = 0, .fact_capacity = 256};
        LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&env.store, 1u, &desc) == LAPLACE_OK);
        LAPLACE_TEST_ASSERT(laplace_exact_register_predicate(&env.store, 2u, &desc) == LAPLACE_OK);
    }

    /* Rule: ancestor(X,Y) :- parent(X,Y).
     * Head: pred=2, arity=2, terms=[var(1), var(2)]
     * Body[0]: pred=1, arity=2, terms=[var(1), var(2)]
     */
    laplace_transport_cmd_add_rule_t rule_payload;
    memset(&rule_payload, 0, sizeof(rule_payload));
    rule_payload.body_count = 1u;

    /* Head */
    rule_payload.head.predicate_id   = 2u;
    rule_payload.head.arity          = 2u;
    rule_payload.head.terms[0].kind  = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.head.terms[0].value = 1u;
    rule_payload.head.terms[1].kind  = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.head.terms[1].value = 2u;

    /* Body[0] */
    rule_payload.body[0].predicate_id   = 1u;
    rule_payload.body[0].arity          = 2u;
    rule_payload.body[0].terms[0].kind  = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.body[0].terms[0].value = 1u;
    rule_payload.body[0].terms[1].kind  = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.body[0].terms[1].value = 2u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_ADD_RULE, 1100u,
                             &rule_payload, (uint32_t)sizeof(rule_payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check RULE_ACCEPTED */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_RULE_ACCEPTED);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_OK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 1100u);

    laplace_transport_evt_rule_accepted_t acc;
    memcpy(&acc, evt.payload, sizeof(acc));
    LAPLACE_TEST_ASSERT(acc.rule_id != LAPLACE_RULE_ID_INVALID);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_add_rule_rejected(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_AddRuleRej") == 0);

    /* Try to add rule with undeclared predicate — should be rejected */
    laplace_transport_cmd_add_rule_t rule_payload;
    memset(&rule_payload, 0, sizeof(rule_payload));
    rule_payload.body_count            = 1u;
    rule_payload.head.predicate_id     = 99u;
    rule_payload.head.arity            = 1u;
    rule_payload.head.terms[0].kind    = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.head.terms[0].value   = 1u;
    rule_payload.body[0].predicate_id  = 98u;
    rule_payload.body[0].arity         = 1u;
    rule_payload.body[0].terms[0].kind = (uint8_t)LAPLACE_EXACT_TERM_VARIABLE;
    rule_payload.body[0].terms[0].value = 1u;

    laplace_transport_command_record_t cmd;
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_ADD_RULE, 1200u,
                             &rule_payload, (uint32_t)sizeof(rule_payload));
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_OK);

    /* Check RULE_REJECTED */
    laplace_transport_event_record_t evt;
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_RULE_REJECTED);
    LAPLACE_TEST_ASSERT(evt.status == (uint32_t)LAPLACE_TRANSPORT_STATUS_ERR_VALIDATION_FAILED);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 1200u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_process_empty_ingress(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_Empty") == 0);

    /* Process on empty ingress */
    LAPLACE_TEST_ASSERT(laplace_transport_process_one(&env.ctx) == LAPLACE_ERR_INVALID_STATE);

    /* Batch on empty */
    LAPLACE_TEST_ASSERT(laplace_transport_process_batch(&env.ctx, 10u) == 0u);

    /* Drain on empty */
    LAPLACE_TEST_ASSERT(laplace_transport_drain(&env.ctx) == 0u);

    transport_test_env_cleanup(&env);
    return 0;
}

static int test_transport_integration_sequence(void) {
    transport_test_env_t env;
    LAPLACE_TEST_ASSERT(transport_test_env_init(&env, "Local\\LaplaceTest_IntSeq") == 0);

    laplace_transport_command_record_t cmd;

    /* 1. Register predicate(1, arity=2) */
    {
        laplace_transport_cmd_register_predicate_t p;
        memset(&p, 0, sizeof(p));
        p.predicate_id = 1u;
        p.arity        = 2u;
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_REGISTER_PREDICATE, 10u,
                                 &p, (uint32_t)sizeof(p));
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }

    /* 2. Allocate and register two constants */
    laplace_entity_handle_t c1 = laplace_entity_pool_alloc(&env.entity_pool);
    laplace_entity_handle_t c2 = laplace_entity_pool_alloc(&env.entity_pool);
    LAPLACE_TEST_ASSERT(c1.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(c2.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&env.entity_pool, c1, LAPLACE_STATE_READY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&env.entity_pool, c2, LAPLACE_STATE_READY) == LAPLACE_OK);

    {
        laplace_transport_cmd_register_constant_t rc;
        memset(&rc, 0, sizeof(rc));
        rc.entity_id  = (uint32_t)c1.id;
        rc.generation = (uint32_t)c1.generation;
        rc.type_id    = 1u;
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_REGISTER_CONSTANT, 11u,
                                 &rc, (uint32_t)sizeof(rc));
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }
    {
        laplace_transport_cmd_register_constant_t rc;
        memset(&rc, 0, sizeof(rc));
        rc.entity_id  = (uint32_t)c2.id;
        rc.generation = (uint32_t)c2.generation;
        rc.type_id    = 1u;
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_REGISTER_CONSTANT, 12u,
                                 &rc, (uint32_t)sizeof(rc));
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }

    /* 3. Assert fact(c1, c2) */
    {
        laplace_transport_cmd_assert_fact_t af;
        memset(&af, 0, sizeof(af));
        af.predicate_id = 1u;
        af.arg_count    = 2u;
        af.args[0]      = (uint32_t)c1.id;
        af.args[1]      = (uint32_t)c2.id;
        transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_ASSERT_FACT, 13u,
                                 &af, (uint32_t)sizeof(af));
        LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);
    }

    /* 4. Query stats */
    transport_build_command(&cmd, LAPLACE_TRANSPORT_CMD_QUERY_STATS, 14u, NULL, 0u);
    LAPLACE_TEST_ASSERT(laplace_transport_ingress_enqueue(&env.mapping, &cmd) == LAPLACE_OK);

    /* Process all */
    const uint32_t processed = laplace_transport_drain(&env.ctx);
    LAPLACE_TEST_ASSERT(processed == 5u);

    /* Verify events in order */
    laplace_transport_event_record_t evt;

    /* Event 1: register predicate ACK */
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 10u);

    /* Event 2: register constant 1 ACK */
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 11u);

    /* Event 3: register constant 2 ACK */
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_ACK);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 12u);

    /* Event 4: fact committed */
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_FACT_COMMITTED);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 13u);

    /* Event 5: stats snapshot */
    LAPLACE_TEST_ASSERT(laplace_transport_egress_dequeue(&env.mapping, &evt) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(evt.kind == (uint32_t)LAPLACE_TRANSPORT_EVT_STATS_SNAPSHOT);
    LAPLACE_TEST_ASSERT(evt.correlation_id == 14u);

    laplace_transport_evt_stats_snapshot_t snap;
    memcpy(&snap, evt.payload, sizeof(snap));
    LAPLACE_TEST_ASSERT(snap.predicate_count == 1u);
    LAPLACE_TEST_ASSERT(snap.fact_count == 1u);

    /* All events consumed */
    const laplace_transport_ring_header_t* egress_ring = laplace_transport_get_egress_ring(&env.mapping);
    LAPLACE_TEST_ASSERT(laplace_transport_ring_is_empty(egress_ring));

    transport_test_env_cleanup(&env);
    return 0;
}

int laplace_test_transport(void) {
    const laplace_test_case_t subtests[] = {
        {"transport_create_close",           test_transport_create_close},
        {"transport_create_open_close",      test_transport_create_open_close},
        {"transport_null_args",              test_transport_null_args},
        {"transport_ingress_ring_basic",     test_transport_ingress_ring_basic},
        {"transport_egress_ring_basic",      test_transport_egress_ring_basic},
        {"transport_ring_full",              test_transport_ring_full},
        {"transport_ring_wraparound",        test_transport_ring_wraparound},
        {"transport_ping",                   test_transport_ping},
        {"transport_register_predicate",     test_transport_register_predicate},
        {"transport_register_constant",      test_transport_register_constant},
        {"transport_assert_fact",            test_transport_assert_fact},
        {"transport_assert_fact_invalid_pred", test_transport_assert_fact_invalid_predicate},
        {"transport_unknown_command",        test_transport_unknown_command},
        {"transport_query_stats",            test_transport_query_stats},
        {"transport_build_trigger_index",    test_transport_build_trigger_index},
        {"transport_exec_step_no_ready",     test_transport_exec_step_no_ready},
        {"transport_exec_run_fixpoint",      test_transport_exec_run_fixpoint},
        {"transport_batch_processing",       test_transport_batch_processing},
        {"transport_drain",                  test_transport_drain},
        {"transport_add_rule_accepted",      test_transport_add_rule_accepted},
        {"transport_add_rule_rejected",      test_transport_add_rule_rejected},
        {"transport_process_empty_ingress",  test_transport_process_empty_ingress},
        {"transport_integration_sequence",   test_transport_integration_sequence},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    int failures = 0;

    for (size_t i = 0; i < count; ++i) {
        const int result = subtests[i].fn();
        if (result != 0) {
            fprintf(stderr, "  [FAIL] %s\n", subtests[i].name);
            ++failures;
        }
    }

    return failures;
}
