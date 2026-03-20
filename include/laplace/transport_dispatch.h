#ifndef LAPLACE_TRANSPORT_DISPATCH_H
#define LAPLACE_TRANSPORT_DISPATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/exec.h"
#include "laplace/transport.h"
#include "laplace/types.h"

typedef struct laplace_observe_context laplace_observe_context_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_transport_stats {
    uint64_t commands_processed;
    uint64_t commands_failed;
    uint64_t events_emitted;
    uint64_t egress_full_drops;
    uint64_t ping_count;
} laplace_transport_stats_t;

typedef struct laplace_transport_context {
    laplace_transport_mapping_t* mapping;
    laplace_exact_store_t*       store;
    laplace_exec_context_t*      exec_ctx;
    laplace_entity_pool_t*       entity_pool;

    uint32_t next_egress_sequence;

    laplace_transport_stats_t stats;

    laplace_observe_context_t* observe;
} laplace_transport_context_t;

laplace_error_t laplace_transport_ctx_init(laplace_transport_context_t* ctx,
                                            laplace_transport_mapping_t* mapping,
                                            laplace_exact_store_t* store,
                                            laplace_exec_context_t* exec_ctx,
                                            laplace_entity_pool_t* entity_pool);

void laplace_transport_ctx_reset(laplace_transport_context_t* ctx);

laplace_error_t laplace_transport_process_one(laplace_transport_context_t* ctx);

uint32_t laplace_transport_process_batch(laplace_transport_context_t* ctx, uint32_t max_commands);

uint32_t laplace_transport_drain(laplace_transport_context_t* ctx);

laplace_error_t laplace_transport_emit_event(laplace_transport_context_t* ctx,
                                              const laplace_transport_event_record_t* event);

laplace_error_t laplace_transport_emit_ack(laplace_transport_context_t* ctx,
                                            laplace_transport_correlation_id_t corr_id,
                                            uint32_t detail);

laplace_error_t laplace_transport_emit_error(laplace_transport_context_t* ctx,
                                              laplace_transport_correlation_id_t corr_id,
                                              laplace_transport_status_t status,
                                              uint32_t detail);

const laplace_transport_stats_t* laplace_transport_get_stats(const laplace_transport_context_t* ctx);

void laplace_transport_ctx_bind_observe(laplace_transport_context_t* ctx,
                                        laplace_observe_context_t* observe);

#ifdef __cplusplus
}
#endif

#endif
