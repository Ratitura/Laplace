#include <stdio.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/transport.h"
#include "laplace/transport_dispatch.h"

#define BENCH_TRANSPORT_ARENA_SIZE (4u * 1024u * 1024u)
#define BENCH_TRANSPORT_ENTITY_CAPACITY 4096u

static uint8_t bench_transport_arena_buf[BENCH_TRANSPORT_ARENA_SIZE];

typedef struct bench_transport_env {
    laplace_arena_t              arena;
    laplace_entity_pool_t        entity_pool;
    laplace_exact_store_t        store;
    laplace_exec_context_t       exec_ctx;
    laplace_transport_mapping_t  mapping;
    laplace_transport_context_t  ctx;
    laplace_transport_command_record_t ping_cmd;
} bench_transport_env_t;

static bench_transport_env_t g_bench_env;
static volatile uint64_t g_bench_sink;

static int bench_transport_env_init(bench_transport_env_t* env) {
    memset(env, 0, sizeof(*env));

    if (laplace_arena_init(&env->arena, bench_transport_arena_buf, BENCH_TRANSPORT_ARENA_SIZE) != LAPLACE_OK) return 1;
    if (laplace_entity_pool_init(&env->entity_pool, &env->arena, BENCH_TRANSPORT_ENTITY_CAPACITY) != LAPLACE_OK) return 1;
    if (laplace_exact_store_init(&env->store, &env->arena, &env->entity_pool) != LAPLACE_OK) return 1;
    if (laplace_exec_init(&env->exec_ctx, &env->arena, &env->store, &env->entity_pool) != LAPLACE_OK) return 1;
    if (laplace_transport_create(&env->mapping, "Local\\LaplaceBench_Transport") != LAPLACE_OK) return 1;
    if (laplace_transport_ctx_init(&env->ctx, &env->mapping, &env->store,
                                    &env->exec_ctx, &env->entity_pool) != LAPLACE_OK) return 1;

    {
        laplace_exact_predicate_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.arity         = 2u;
        desc.flags         = 0u;
        desc.fact_capacity = 256u;
        (void)laplace_exact_register_predicate(&env->store, 1u, &desc);
    }

    (void)laplace_exec_build_trigger_index(&env->exec_ctx);

    memset(&env->ping_cmd, 0, sizeof(env->ping_cmd));
    env->ping_cmd.kind           = (uint32_t)LAPLACE_TRANSPORT_CMD_PING;
    env->ping_cmd.correlation_id = 1u;
    env->ping_cmd.payload_size   = 0u;

    return 0;
}

static void bench_transport_env_cleanup(bench_transport_env_t* env) {
    laplace_transport_close(&env->mapping);
}

static void bench_ingress_enqueue(void* context) {
    bench_transport_env_t* env = (bench_transport_env_t*)context;

    (void)laplace_transport_ingress_enqueue(&env->mapping, &env->ping_cmd);

    laplace_transport_command_record_t out;
    (void)laplace_transport_ingress_dequeue(&env->mapping, &out);
    g_bench_sink = out.correlation_id;
}

static void bench_egress_round(void* context) {
    bench_transport_env_t* env = (bench_transport_env_t*)context;

    laplace_transport_event_record_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.kind           = (uint32_t)LAPLACE_TRANSPORT_EVT_ACK;
    evt.correlation_id = 1u;
    evt.sequence       = 1u;

    (void)laplace_transport_egress_enqueue(&env->mapping, &evt);

    laplace_transport_event_record_t out;
    (void)laplace_transport_egress_dequeue(&env->mapping, &out);
    g_bench_sink = out.correlation_id;
}

static void bench_ping_roundtrip(void* context) {
    bench_transport_env_t* env = (bench_transport_env_t*)context;

    (void)laplace_transport_ingress_enqueue(&env->mapping, &env->ping_cmd);

    (void)laplace_transport_process_one(&env->ctx);

    laplace_transport_event_record_t evt;
    (void)laplace_transport_egress_dequeue(&env->mapping, &evt);
    g_bench_sink = evt.correlation_id;
}

static void bench_exec_run_roundtrip(void* context) {
    bench_transport_env_t* env = (bench_transport_env_t*)context;

    laplace_transport_cmd_exec_run_t run_payload;
    memset(&run_payload, 0, sizeof(run_payload));
    run_payload.max_steps       = 10u;
    run_payload.max_derivations = 10u;
    run_payload.mode            = 0u;

    laplace_transport_command_record_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind           = (uint32_t)LAPLACE_TRANSPORT_CMD_EXEC_RUN;
    cmd.correlation_id = 1u;
    cmd.payload_size   = (uint32_t)sizeof(run_payload);
    memcpy(cmd.payload, &run_payload, sizeof(run_payload));

    (void)laplace_transport_ingress_enqueue(&env->mapping, &cmd);
    (void)laplace_transport_process_one(&env->ctx);

    laplace_transport_event_record_t evt;
    (void)laplace_transport_egress_dequeue(&env->mapping, &evt);
    g_bench_sink = evt.correlation_id;
}

void laplace_bench_transport(void) {
    if (bench_transport_env_init(&g_bench_env) != 0) {
        fprintf(stderr, "[BENCH] Transport env init failed\n");
        return;
    }

    const laplace_bench_case_t benches[] = {
        {"transport_ingress_enqueue_dequeue", bench_ingress_enqueue,    &g_bench_env, 1000000u},
        {"transport_egress_enqueue_dequeue",  bench_egress_round,       &g_bench_env, 1000000u},
        {"transport_ping_roundtrip",          bench_ping_roundtrip,     &g_bench_env, 1000000u},
        {"transport_exec_run_roundtrip",      bench_exec_run_roundtrip, &g_bench_env, 500000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0; i < count; ++i) {
        (void)laplace_bench_run_case(&benches[i]);
    }

    bench_transport_env_cleanup(&g_bench_env);
}
