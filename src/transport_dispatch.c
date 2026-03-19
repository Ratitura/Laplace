#include "laplace/transport_dispatch.h"

#include <string.h>

#include "laplace/observe.h"

laplace_error_t laplace_transport_ctx_init(laplace_transport_context_t* ctx,
                                            laplace_transport_mapping_t* mapping,
                                            laplace_exact_store_t* store,
                                            laplace_exec_context_t* exec_ctx,
                                            laplace_entity_pool_t* entity_pool) {
    if (ctx == NULL || mapping == NULL || store == NULL ||
        exec_ctx == NULL || entity_pool == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (mapping->view == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->mapping     = mapping;
    ctx->store       = store;
    ctx->exec_ctx    = exec_ctx;
    ctx->entity_pool = entity_pool;
    ctx->next_egress_sequence = 1u;
    return LAPLACE_OK;
}

void laplace_transport_ctx_reset(laplace_transport_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->next_egress_sequence = 1u;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
}

laplace_error_t laplace_transport_emit_event(laplace_transport_context_t* ctx,
                                              const laplace_transport_event_record_t* event) {
    LAPLACE_ASSERT(ctx != NULL && event != NULL);
    const laplace_error_t err = laplace_transport_egress_enqueue(ctx->mapping, event);
    if (err == LAPLACE_OK) {
        ctx->stats.events_emitted += 1u;

        if (ctx->observe != NULL) {
            laplace_observe_trace_transport_evt(ctx->observe, event->kind,
                event->status, event->correlation_id);
        }
    } else {
        ctx->stats.egress_full_drops += 1u;
    }
    return err;
}

static void transport_build_event(laplace_transport_event_record_t* evt,
                                   laplace_transport_evt_kind_t kind,
                                   laplace_transport_status_t status,
                                   laplace_transport_correlation_id_t corr_id,
                                   const void* payload,
                                   uint32_t payload_size,
                                   uint32_t sequence) {
    memset(evt, 0, sizeof(*evt));
    evt->kind           = (uint32_t)kind;
    evt->status         = (uint32_t)status;
    evt->correlation_id = corr_id;
    evt->payload_size   = payload_size;
    evt->sequence       = sequence;
    if (payload != NULL && payload_size > 0u) {
        LAPLACE_ASSERT(payload_size <= sizeof(evt->payload));
        memcpy(evt->payload, payload, payload_size);
    }
}

laplace_error_t laplace_transport_emit_ack(laplace_transport_context_t* ctx,
                                            laplace_transport_correlation_id_t corr_id,
                                            uint32_t detail) {
    LAPLACE_ASSERT(ctx != NULL);
    laplace_transport_event_record_t evt;
    laplace_transport_evt_ack_t ack_payload;
    memset(&ack_payload, 0, sizeof(ack_payload));
    ack_payload.detail = detail;
    transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_ACK, LAPLACE_TRANSPORT_STATUS_OK,
                           corr_id, &ack_payload, (uint32_t)sizeof(ack_payload),
                           ctx->next_egress_sequence++);
    return laplace_transport_emit_event(ctx, &evt);
}

laplace_error_t laplace_transport_emit_error(laplace_transport_context_t* ctx,
                                              laplace_transport_correlation_id_t corr_id,
                                              laplace_transport_status_t status,
                                              uint32_t detail) {
    LAPLACE_ASSERT(ctx != NULL);
    laplace_transport_event_record_t evt;
    laplace_transport_evt_error_t err_payload;
    memset(&err_payload, 0, sizeof(err_payload));
    err_payload.error_code = (uint32_t)status;
    err_payload.detail     = detail;
    transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_ERROR, status,
                           corr_id, &err_payload, (uint32_t)sizeof(err_payload),
                           ctx->next_egress_sequence++);

    if (ctx->observe != NULL) {
        laplace_observe_trace_transport_error(ctx->observe, detail,
            (uint32_t)status, corr_id);
    }

    return laplace_transport_emit_event(ctx, &evt);
}

static void transport_handle_ping(laplace_transport_context_t* ctx,
                                   const laplace_transport_command_record_t* cmd) {
    ctx->stats.ping_count += 1u;
    (void)laplace_transport_emit_ack(ctx, cmd->correlation_id, 0u);
}

static void transport_handle_register_predicate(laplace_transport_context_t* ctx,
                                                  const laplace_transport_command_record_t* cmd) {
    if (cmd->payload_size < (uint32_t)sizeof(laplace_transport_cmd_register_predicate_t)) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD, 0u);
        return;
    }

    laplace_transport_cmd_register_predicate_t payload;
    memcpy(&payload, cmd->payload, sizeof(payload));

    const laplace_predicate_id_t pred_id = (laplace_predicate_id_t)payload.predicate_id;

    laplace_exact_predicate_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.arity         = payload.arity;
    desc.flags         = payload.flags;
    desc.fact_capacity = payload.fact_capacity > 0u ? payload.fact_capacity : 256u;

    const laplace_error_t err = laplace_exact_register_predicate(ctx->store, pred_id, &desc);
    if (err == LAPLACE_OK) {
        (void)laplace_transport_emit_ack(ctx, cmd->correlation_id, (uint32_t)pred_id);
    } else {
        laplace_transport_status_t status = LAPLACE_TRANSPORT_STATUS_ERR_INTERNAL;
        if (err == LAPLACE_ERR_CAPACITY_EXHAUSTED) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_CAPACITY;
        } else if (err == LAPLACE_ERR_INVALID_ARGUMENT) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD;
        }
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id, status, (uint32_t)err);
    }
}

static void transport_handle_register_constant(laplace_transport_context_t* ctx,
                                                 const laplace_transport_command_record_t* cmd) {
    if (cmd->payload_size < (uint32_t)sizeof(laplace_transport_cmd_register_constant_t)) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD, 0u);
        return;
    }

    laplace_transport_cmd_register_constant_t payload;
    memcpy(&payload, cmd->payload, sizeof(payload));

    laplace_entity_handle_t handle;
    handle.id         = (laplace_entity_id_t)payload.entity_id;
    handle.generation = (laplace_generation_t)payload.generation;

    if (!laplace_entity_pool_is_alive(ctx->entity_pool, handle)) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_ENTITY, payload.entity_id);
        return;
    }

    const laplace_error_t err = laplace_exact_register_constant(
        ctx->store, handle,
        (laplace_exact_type_id_t)payload.type_id,
        payload.flags
    );

    if (err == LAPLACE_OK) {
        (void)laplace_transport_emit_ack(ctx, cmd->correlation_id, payload.entity_id);
    } else {
        laplace_transport_status_t status = LAPLACE_TRANSPORT_STATUS_ERR_INTERNAL;
        if (err == LAPLACE_ERR_INVALID_ARGUMENT) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD;
        } else if (err == LAPLACE_ERR_INVALID_STATE) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_INVALID_ENTITY;
        }
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id, status, (uint32_t)err);
    }
}

static void transport_handle_assert_fact(laplace_transport_context_t* ctx,
                                          const laplace_transport_command_record_t* cmd) {
    if (cmd->payload_size < (uint32_t)sizeof(laplace_transport_cmd_assert_fact_t)) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD, 0u);
        return;
    }

    laplace_transport_cmd_assert_fact_t payload;
    memcpy(&payload, cmd->payload, sizeof(payload));

    const laplace_predicate_id_t pred_id = (laplace_predicate_id_t)payload.predicate_id;

    if (!laplace_exact_predicate_is_declared(ctx->store, pred_id)) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PREDICATE, (uint32_t)pred_id);
        return;
    }

    const uint8_t expected_arity = laplace_exact_predicate_arity(ctx->store, pred_id);
    if (payload.arg_count != expected_arity) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD, payload.arg_count);
        return;
    }

    /* Convert raw entity IDs to handles.
     * For simplicity, we look up current generation for each arg entity. */
    laplace_entity_handle_t arg_handles[LAPLACE_EXACT_MAX_ARITY];
    for (uint8_t i = 0u; i < payload.arg_count; ++i) {
        const uint32_t eid = payload.args[i];
        if (eid == LAPLACE_ENTITY_ID_INVALID || eid > ctx->entity_pool->capacity) {
            (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                               LAPLACE_TRANSPORT_STATUS_ERR_INVALID_ENTITY, eid);
            return;
        }
        const uint32_t slot = eid - 1u;
        arg_handles[i].id = (laplace_entity_id_t)eid;
        arg_handles[i].generation = laplace_entity_pool_generation(ctx->entity_pool, slot);
    }

    /* Insert provenance for asserted fact */
    laplace_exact_provenance_desc_t prov_desc;
    memset(&prov_desc, 0, sizeof(prov_desc));
    prov_desc.kind = LAPLACE_EXACT_PROVENANCE_ASSERTED;

    laplace_provenance_id_t prov_id = LAPLACE_PROVENANCE_ID_INVALID;
    laplace_error_t err = laplace_exact_insert_provenance(ctx->store, &prov_desc, &prov_id);
    if (err != LAPLACE_OK) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_CAPACITY, (uint32_t)err);
        return;
    }

    laplace_exact_fact_row_t fact_row = LAPLACE_EXACT_FACT_ROW_INVALID;
    laplace_entity_handle_t fact_entity = {LAPLACE_ENTITY_ID_INVALID, LAPLACE_GENERATION_INVALID};
    bool inserted = false;

    err = laplace_exact_assert_fact(
        ctx->store, pred_id, arg_handles, payload.arg_count,
        prov_id, payload.flags | LAPLACE_EXACT_FACT_FLAG_ASSERTED | LAPLACE_EXACT_FACT_FLAG_COMMITTED,
        &fact_row, &fact_entity, &inserted
    );

    if (err == LAPLACE_OK) {
        laplace_transport_event_record_t evt;
        laplace_transport_evt_fact_committed_t fc_payload;
        memset(&fc_payload, 0, sizeof(fc_payload));
        fc_payload.fact_row          = (uint32_t)fact_row;
        fc_payload.entity_id         = (uint32_t)fact_entity.id;
        fc_payload.entity_generation = (uint32_t)fact_entity.generation;
        fc_payload.predicate_id      = (uint16_t)pred_id;
        fc_payload.arg_count         = payload.arg_count;
        fc_payload.inserted          = inserted ? 1u : 0u;

        transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_FACT_COMMITTED, LAPLACE_TRANSPORT_STATUS_OK,
                               cmd->correlation_id, &fc_payload, (uint32_t)sizeof(fc_payload),
                               ctx->next_egress_sequence++);
        (void)laplace_transport_emit_event(ctx, &evt);
    } else {
        laplace_transport_status_t status = LAPLACE_TRANSPORT_STATUS_ERR_INTERNAL;
        if (err == LAPLACE_ERR_CAPACITY_EXHAUSTED) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_CAPACITY;
        } else if (err == LAPLACE_ERR_INVALID_ARGUMENT) {
            status = LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD;
        }
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id, status, (uint32_t)err);
    }
}

static void transport_convert_literal(laplace_exact_literal_t* out,
                                       const laplace_transport_literal_t* in) {
    memset(out, 0, sizeof(*out));
    out->predicate = (laplace_predicate_id_t)in->predicate_id;
    out->arity     = in->arity;
    for (uint8_t i = 0u; i < in->arity && i < LAPLACE_EXACT_MAX_ARITY; ++i) {
        out->terms[i].kind = (laplace_exact_term_kind_t)in->terms[i].kind;
        if (in->terms[i].kind == (uint8_t)LAPLACE_EXACT_TERM_VARIABLE) {
            out->terms[i].value.variable = (laplace_exact_var_id_t)in->terms[i].value;
        } else if (in->terms[i].kind == (uint8_t)LAPLACE_EXACT_TERM_CONSTANT) {
            out->terms[i].value.constant = (laplace_entity_id_t)in->terms[i].value;
        }
    }
}

static void transport_handle_add_rule(laplace_transport_context_t* ctx,
                                       const laplace_transport_command_record_t* cmd) {
    if (cmd->payload_size < 4u) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD, 0u);
        return;
    }

    laplace_transport_cmd_add_rule_t rule_payload;
    memcpy(&rule_payload, cmd->payload, sizeof(rule_payload));

    if (rule_payload.body_count == 0u || rule_payload.body_count > LAPLACE_EXACT_MAX_RULE_BODY_LITERALS) {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD,
                                           rule_payload.body_count);
        return;
    }

    /* Convert to exact rule desc */
    laplace_exact_literal_t head;
    transport_convert_literal(&head, &rule_payload.head);

    laplace_exact_literal_t body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
    for (uint32_t i = 0u; i < rule_payload.body_count; ++i) {
        transport_convert_literal(&body[i], &rule_payload.body[i]);
    }

    laplace_exact_rule_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.head          = head;
    desc.body_literals = body;
    desc.body_count    = rule_payload.body_count;

    laplace_rule_id_t rule_id = LAPLACE_RULE_ID_INVALID;
    laplace_exact_rule_validation_result_t validation;
    memset(&validation, 0, sizeof(validation));

    const laplace_error_t err = laplace_exact_add_rule(ctx->store, &desc,
                                                        &rule_id, &validation);

    if (err == LAPLACE_OK && validation.error == LAPLACE_EXACT_RULE_VALIDATION_OK) {
        laplace_transport_event_record_t evt;
        laplace_transport_evt_rule_accepted_t acc_payload;
        memset(&acc_payload, 0, sizeof(acc_payload));
        acc_payload.rule_id = (uint32_t)rule_id;

        transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_RULE_ACCEPTED, LAPLACE_TRANSPORT_STATUS_OK,
                               cmd->correlation_id, &acc_payload, (uint32_t)sizeof(acc_payload),
                               ctx->next_egress_sequence++);
        (void)laplace_transport_emit_event(ctx, &evt);
    } else {
        laplace_transport_event_record_t evt;
        laplace_transport_evt_rule_rejected_t rej_payload;
        memset(&rej_payload, 0, sizeof(rej_payload));
        rej_payload.validation_error = (uint32_t)validation.error;
        rej_payload.literal_index    = validation.literal_index;
        rej_payload.term_index       = validation.term_index;

        transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_RULE_REJECTED,
                               LAPLACE_TRANSPORT_STATUS_ERR_VALIDATION_FAILED,
                               cmd->correlation_id, &rej_payload, (uint32_t)sizeof(rej_payload),
                               ctx->next_egress_sequence++);
        (void)laplace_transport_emit_event(ctx, &evt);
    }
}

static void transport_handle_build_trigger_index(laplace_transport_context_t* ctx,
                                                   const laplace_transport_command_record_t* cmd) {
    const laplace_error_t err = laplace_exec_build_trigger_index(ctx->exec_ctx);
    if (err == LAPLACE_OK) {
        (void)laplace_transport_emit_ack(ctx, cmd->correlation_id, 0u);
    } else {
        (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                           LAPLACE_TRANSPORT_STATUS_ERR_INTERNAL, (uint32_t)err);
    }
}

static void transport_handle_exec_step(laplace_transport_context_t* ctx,
                                        const laplace_transport_command_record_t* cmd) {
    const laplace_error_t err = laplace_exec_step(ctx->exec_ctx);

    laplace_transport_event_record_t evt;
    laplace_transport_evt_exec_status_t status_payload;
    memset(&status_payload, 0, sizeof(status_payload));

    const laplace_exec_stats_t* stats = laplace_exec_get_stats(ctx->exec_ctx);
    status_payload.steps_executed      = stats->steps_executed;
    status_payload.facts_derived       = stats->facts_derived;
    status_payload.facts_deduplicated  = stats->facts_deduplicated;

    if (err == LAPLACE_OK) {
        status_payload.run_status = 0u; /* step success */
        transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_EXEC_STATUS, LAPLACE_TRANSPORT_STATUS_OK,
                               cmd->correlation_id, &status_payload, (uint32_t)sizeof(status_payload),
                               ctx->next_egress_sequence++);
    } else {
        status_payload.run_status = (uint32_t)LAPLACE_EXEC_RUN_ERROR;
        transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_EXEC_STATUS, LAPLACE_TRANSPORT_STATUS_ERR_EXEC_ERROR,
                               cmd->correlation_id, &status_payload, (uint32_t)sizeof(status_payload),
                               ctx->next_egress_sequence++);
    }
    (void)laplace_transport_emit_event(ctx, &evt);
}

static void transport_handle_exec_run(laplace_transport_context_t* ctx,
                                       const laplace_transport_command_record_t* cmd) {
    /* Optionally apply budget overrides from payload */
    if (cmd->payload_size >= (uint32_t)sizeof(laplace_transport_cmd_exec_run_t)) {
        laplace_transport_cmd_exec_run_t run_payload;
        memcpy(&run_payload, cmd->payload, sizeof(run_payload));

        if (run_payload.max_steps > 0u) {
            laplace_exec_set_max_steps(ctx->exec_ctx, run_payload.max_steps);
        }
        if (run_payload.max_derivations > 0u) {
            laplace_exec_set_max_derivations(ctx->exec_ctx, run_payload.max_derivations);
        }
        if (run_payload.mode <= (uint8_t)LAPLACE_EXEC_MODE_SPARSE) {
            laplace_exec_set_mode(ctx->exec_ctx, (laplace_exec_mode_t)run_payload.mode);
        }
    }

    const laplace_exec_run_status_t run_status = laplace_exec_run(ctx->exec_ctx);

    laplace_transport_event_record_t evt;
    laplace_transport_evt_exec_status_t status_payload;
    memset(&status_payload, 0, sizeof(status_payload));
    status_payload.run_status = (uint32_t)run_status;

    const laplace_exec_stats_t* stats = laplace_exec_get_stats(ctx->exec_ctx);
    status_payload.steps_executed     = stats->steps_executed;
    status_payload.facts_derived      = stats->facts_derived;
    status_payload.facts_deduplicated = stats->facts_deduplicated;

    laplace_transport_status_t transport_status = LAPLACE_TRANSPORT_STATUS_OK;
    if (run_status == LAPLACE_EXEC_RUN_ERROR) {
        transport_status = LAPLACE_TRANSPORT_STATUS_ERR_EXEC_ERROR;
    }

    transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_EXEC_STATUS, transport_status,
                           cmd->correlation_id, &status_payload, (uint32_t)sizeof(status_payload),
                           ctx->next_egress_sequence++);
    (void)laplace_transport_emit_event(ctx, &evt);
}

static void transport_handle_query_stats(laplace_transport_context_t* ctx,
                                          const laplace_transport_command_record_t* cmd) {
    laplace_transport_event_record_t evt;
    laplace_transport_evt_stats_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));

    snap.predicate_count    = ctx->store->predicate_count;
    snap.fact_count         = ctx->store->fact_count;
    snap.rule_count         = ctx->store->rule_count;
    snap.entity_alive_count = ctx->entity_pool->alive_count;
    snap.entity_capacity    = ctx->entity_pool->capacity;

    const laplace_exec_stats_t* exec_stats = laplace_exec_get_stats(ctx->exec_ctx);
    snap.exec_steps         = exec_stats->steps_executed;
    snap.exec_facts_derived = exec_stats->facts_derived;

    transport_build_event(&evt, LAPLACE_TRANSPORT_EVT_STATS_SNAPSHOT, LAPLACE_TRANSPORT_STATUS_OK,
                           cmd->correlation_id, &snap, (uint32_t)sizeof(snap),
                           ctx->next_egress_sequence++);
    (void)laplace_transport_emit_event(ctx, &evt);
}

static void transport_dispatch_command(laplace_transport_context_t* ctx,
                                        const laplace_transport_command_record_t* cmd) {
    switch ((laplace_transport_cmd_kind_t)cmd->kind) {
        case LAPLACE_TRANSPORT_CMD_PING:
            transport_handle_ping(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_REGISTER_PREDICATE:
            transport_handle_register_predicate(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_REGISTER_CONSTANT:
            transport_handle_register_constant(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_ASSERT_FACT:
            transport_handle_assert_fact(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_ADD_RULE:
            transport_handle_add_rule(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_BUILD_TRIGGER_INDEX:
            transport_handle_build_trigger_index(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_EXEC_STEP:
            transport_handle_exec_step(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_EXEC_RUN:
            transport_handle_exec_run(ctx, cmd);
            break;
        case LAPLACE_TRANSPORT_CMD_QUERY_STATS:
            transport_handle_query_stats(ctx, cmd);
            break;
        default:
            (void)laplace_transport_emit_error(ctx, cmd->correlation_id,
                                               LAPLACE_TRANSPORT_STATUS_ERR_UNKNOWN_CMD,
                                               cmd->kind);
            break;
    }
}

laplace_error_t laplace_transport_process_one(laplace_transport_context_t* ctx) {
    if (ctx == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_transport_command_record_t cmd;
    const laplace_error_t err = laplace_transport_ingress_dequeue(ctx->mapping, &cmd);
    if (err != LAPLACE_OK) {
        return err;  /* LAPLACE_ERR_INVALID_STATE if empty */
    }

    transport_dispatch_command(ctx, &cmd);
    ctx->stats.commands_processed += 1u;

    if (ctx->observe != NULL) {
        laplace_observe_trace_transport_cmd(ctx->observe, cmd.kind,
            cmd.correlation_id);
    }

    return LAPLACE_OK;
}

uint32_t laplace_transport_process_batch(laplace_transport_context_t* ctx, uint32_t max_commands) {
    if (ctx == NULL || max_commands == 0u) {
        return 0u;
    }

    uint32_t processed = 0u;
    for (uint32_t i = 0u; i < max_commands; ++i) {
        if (laplace_transport_process_one(ctx) != LAPLACE_OK) {
            break;
        }
        ++processed;
    }
    return processed;
}

uint32_t laplace_transport_drain(laplace_transport_context_t* ctx) {
    if (ctx == NULL) {
        return 0u;
    }

    uint32_t processed = 0u;
    while (laplace_transport_process_one(ctx) == LAPLACE_OK) {
        ++processed;
    }
    return processed;
}

const laplace_transport_stats_t* laplace_transport_get_stats(const laplace_transport_context_t* ctx) {
    LAPLACE_ASSERT(ctx != NULL);
    return &ctx->stats;
}

void laplace_transport_ctx_bind_observe(laplace_transport_context_t* const ctx,
                                        laplace_observe_context_t* const observe) {
    if (ctx != NULL) {
        ctx->observe = observe;
    }
}
