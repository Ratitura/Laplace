#ifndef LAPLACE_TRACE_H
#define LAPLACE_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LAPLACE_TRACE_BUFFER_CAPACITY = 4096u,

    LAPLACE_TRACE_PAYLOAD_BYTES = 16u
};

LAPLACE_STATIC_ASSERT((LAPLACE_TRACE_BUFFER_CAPACITY & (LAPLACE_TRACE_BUFFER_CAPACITY - 1u)) == 0u,
                       "trace buffer capacity must be power of two");

typedef uint64_t laplace_trace_seq_t;

typedef enum laplace_trace_subsystem {
    LAPLACE_TRACE_SUBSYSTEM_NONE      = 0u,
    LAPLACE_TRACE_SUBSYSTEM_EXACT     = 1u,
    LAPLACE_TRACE_SUBSYSTEM_EXEC      = 2u,
    LAPLACE_TRACE_SUBSYSTEM_BRANCH    = 3u,
    LAPLACE_TRACE_SUBSYSTEM_TRANSPORT = 4u,
    LAPLACE_TRACE_SUBSYSTEM_OBSERVE   = 5u,
    LAPLACE_TRACE_SUBSYSTEM_COUNT_    = 6u
} laplace_trace_subsystem_t;

typedef enum laplace_trace_kind {
    LAPLACE_TRACE_KIND_INVALID          = 0u,

    LAPLACE_TRACE_KIND_FACT_ASSERTED    = 1u,
    LAPLACE_TRACE_KIND_FACT_DERIVED     = 2u,
    LAPLACE_TRACE_KIND_FACT_DUPLICATE   = 3u,
    LAPLACE_TRACE_KIND_RULE_ACCEPTED    = 4u,
    LAPLACE_TRACE_KIND_RULE_REJECTED    = 5u,

    LAPLACE_TRACE_KIND_EXEC_STEP        = 6u,
    LAPLACE_TRACE_KIND_EXEC_DERIVATION  = 7u,
    LAPLACE_TRACE_KIND_EXEC_FIXPOINT    = 8u,

    LAPLACE_TRACE_KIND_BRANCH_CREATE    = 9u,
    LAPLACE_TRACE_KIND_BRANCH_COMMIT    = 10u,
    LAPLACE_TRACE_KIND_BRANCH_FAIL      = 11u,
    LAPLACE_TRACE_KIND_EPOCH_ADVANCE    = 12u,

    LAPLACE_TRACE_KIND_TRANSPORT_CMD    = 13u,
    LAPLACE_TRACE_KIND_TRANSPORT_EVT    = 14u,
    LAPLACE_TRACE_KIND_TRANSPORT_ERROR  = 15u,

    LAPLACE_TRACE_KIND_COUNTER_SNAPSHOT = 16u,
    LAPLACE_TRACE_KIND_OVERFLOW_MARKER  = 17u,

    LAPLACE_TRACE_KIND_COUNT_           = 18u
} laplace_trace_kind_t;

typedef struct laplace_trace_fact_payload {
    laplace_exact_fact_row_t fact_row;
    laplace_predicate_id_t   predicate;
    uint16_t                 reserved;
} laplace_trace_fact_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_fact_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "fact payload must fit in trace payload");

typedef struct laplace_trace_rule_payload {
    laplace_rule_id_t rule_id;
    uint32_t          validation_error;
} laplace_trace_rule_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_rule_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "rule payload must fit in trace payload");

typedef struct laplace_trace_exec_payload {
    laplace_entity_id_t entity_id;
    uint32_t            rules_fired;
} laplace_trace_exec_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_exec_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "exec payload must fit in trace payload");

typedef struct laplace_trace_branch_payload {
    laplace_branch_id_t         branch_id;
    laplace_branch_generation_t branch_gen;
    uint32_t                    detail;
} laplace_trace_branch_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_branch_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "branch payload must fit in trace payload");

typedef struct laplace_trace_transport_payload {
    uint32_t cmd_or_evt_kind;
    uint32_t status;
    uint64_t correlation_id;
} laplace_trace_transport_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_transport_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "transport payload must fit in trace payload");

typedef struct laplace_trace_overflow_payload {
    uint64_t overflow_count;
    uint64_t reserved;
} laplace_trace_overflow_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_overflow_payload_t) <= LAPLACE_TRACE_PAYLOAD_BYTES,
                       "overflow payload must fit in trace payload");

typedef struct laplace_trace_record {
    laplace_trace_seq_t       sequence;        /* monotonic sequence number */

    uint16_t                  kind;            /* laplace_trace_kind_t */
    uint8_t                   subsystem;       /* laplace_trace_subsystem_t */
    uint8_t                   status;          /* result/error code (kind-specific) */

    laplace_entity_id_t       entity_id;       /* primary entity (or INVALID) */

    laplace_provenance_id_t   provenance_id;   /* provenance record (or INVALID) */

    laplace_rule_id_t         rule_id;         /* rule (or INVALID) */

    laplace_branch_id_t       branch_id;       /* branch context (or INVALID) */

    laplace_branch_generation_t branch_gen;    /* branch generation */

    laplace_epoch_id_t        epoch_id;        /* epoch context (or INVALID) */

    uint64_t                  correlation_id;  /* transport correlation (or 0) */

    laplace_tick_t            tick;            /* exact store tick at emission */

    union {
        laplace_trace_fact_payload_t      fact;
        laplace_trace_rule_payload_t      rule;
        laplace_trace_exec_payload_t      exec;
        laplace_trace_branch_payload_t    branch;
        laplace_trace_transport_payload_t transport;
        laplace_trace_overflow_payload_t  overflow;
        uint8_t                           raw[LAPLACE_TRACE_PAYLOAD_BYTES];
    } payload;
} laplace_trace_record_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_record_t) == 64u,
                       "trace record must be exactly 64 bytes");

typedef struct laplace_trace_buffer {
    laplace_trace_record_t* records;         /* [LAPLACE_TRACE_BUFFER_CAPACITY] */
    uint32_t                capacity;
    uint32_t                head;            /* next write position (modular) */
    uint32_t                count;           /* records written (saturates at capacity) */
    laplace_trace_seq_t     next_sequence;   /* monotonic sequence counter */
    uint64_t                overflow_count;  /* total records lost to overwrites */
    bool                    overflow_marked; /* has overflow marker been emitted this run? */
} laplace_trace_buffer_t;

struct laplace_arena;

laplace_error_t laplace_trace_buffer_init(laplace_trace_buffer_t* buf,
                                           struct laplace_arena* arena);

void laplace_trace_buffer_reset(laplace_trace_buffer_t* buf);

laplace_trace_seq_t laplace_trace_emit(laplace_trace_buffer_t* buf,
                                        const laplace_trace_record_t* record);

uint32_t laplace_trace_count(const laplace_trace_buffer_t* buf);

const laplace_trace_record_t* laplace_trace_get(const laplace_trace_buffer_t* buf,
                                                 uint32_t index);

uint64_t laplace_trace_overflow_count(const laplace_trace_buffer_t* buf);

laplace_trace_seq_t laplace_trace_next_sequence(const laplace_trace_buffer_t* buf);

#ifdef __cplusplus
}
#endif

#endif
