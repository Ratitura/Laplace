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

/*
 * Transport command dispatch bridge.
 *
 * Purpose:
 *   Routes transport commands to exact/execution APIs and emits
 *   bounded events on the egress ring.
 *
 * Truth authority:
 *   All commands flow through exact/execution APIs.
 *   No semantic shortcuts bypass exact validation.
 *
 * Processing model:
 *   Synchronous per-command processing from the kernel side.
 *   The transport context borrows the transport mapping, exact store,
 *   and execution context.  It does not own them.
 */

typedef struct laplace_transport_stats {
    uint64_t commands_processed;
    uint64_t commands_failed;
    uint64_t events_emitted;
    uint64_t egress_full_drops;
    uint64_t ping_count;
} laplace_transport_stats_t;

typedef struct laplace_transport_context {
    /* Borrowed references (not owned) */
    laplace_transport_mapping_t* mapping;
    laplace_exact_store_t*       store;
    laplace_exec_context_t*      exec_ctx;
    laplace_entity_pool_t*       entity_pool;

    /* Egress sequence counter */
    uint32_t next_egress_sequence;

    /* Statistics */
    laplace_transport_stats_t stats;

    /* NULL disables tracing */
    laplace_observe_context_t* observe;
} laplace_transport_context_t;

/*
 * Initialize a transport context.
 *
 * Borrows the transport mapping, exact store, execution context, and entity pool.
 * None of these are owned by the transport context.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_ARGUMENT if any required pointer is NULL.
 */
laplace_error_t laplace_transport_ctx_init(laplace_transport_context_t* ctx,
                                            laplace_transport_mapping_t* mapping,
                                            laplace_exact_store_t* store,
                                            laplace_exec_context_t* exec_ctx,
                                            laplace_entity_pool_t* entity_pool);

/*
 * Reset transport statistics and egress sequence counter.
 */
void laplace_transport_ctx_reset(laplace_transport_context_t* ctx);

/*
 * Process a single command from the ingress ring.
 *
 * Dequeues one command, dispatches it to the appropriate exact/execution
 * API, and emits one or more events on the egress ring.
 *
 * Returns LAPLACE_OK if a command was processed.
 * Returns LAPLACE_ERR_INVALID_STATE if the ingress ring is empty.
 * Returns LAPLACE_ERR_INTERNAL on unexpected dispatch failure.
 */
laplace_error_t laplace_transport_process_one(laplace_transport_context_t* ctx);

/*
 * Process up to `max_commands` commands from the ingress ring.
 *
 * Returns the number of commands actually processed.
 * Stops early if the ring is empty.
 */
uint32_t laplace_transport_process_batch(laplace_transport_context_t* ctx, uint32_t max_commands);

/*
 * Process all commands currently in the ingress ring.
 *
 * Returns the number of commands processed.
 */
uint32_t laplace_transport_drain(laplace_transport_context_t* ctx);

/*
 * Emit an event record on the egress ring.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if the egress ring is full.
 */
laplace_error_t laplace_transport_emit_event(laplace_transport_context_t* ctx,
                                              const laplace_transport_event_record_t* event);

/*
 * Emit a simple ACK event for a command.
 */
laplace_error_t laplace_transport_emit_ack(laplace_transport_context_t* ctx,
                                            laplace_transport_correlation_id_t corr_id,
                                            uint32_t detail);

/*
 * Emit a simple ERROR event for a command.
 */
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
