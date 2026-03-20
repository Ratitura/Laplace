#ifndef LAPLACE_TRANSPORT_H
#define LAPLACE_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/align.h"
#include "laplace/assert.h"
#include "laplace/config.h"
#include "laplace/errors.h"
#include "laplace/exact.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LAPLACE_TRANSPORT_MAGIC = 0x4C50414Cu,

    LAPLACE_TRANSPORT_ABI_VERSION = 1u,

    LAPLACE_TRANSPORT_INGRESS_CAPACITY = 256u,

    LAPLACE_TRANSPORT_EGRESS_CAPACITY = 256u,

    LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES = 672u,

    LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE = 704u,

    LAPLACE_TRANSPORT_EVENT_RECORD_SIZE = 704u
};

enum {
    LAPLACE_TRANSPORT_ENDIAN_LITTLE = 1u,
    LAPLACE_TRANSPORT_ENDIAN_BIG    = 2u
};

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LAPLACE_TRANSPORT_NATIVE_ENDIAN LAPLACE_TRANSPORT_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LAPLACE_TRANSPORT_NATIVE_ENDIAN LAPLACE_TRANSPORT_ENDIAN_BIG
#elif defined(_WIN32)
#define LAPLACE_TRANSPORT_NATIVE_ENDIAN LAPLACE_TRANSPORT_ENDIAN_LITTLE
#else
#error "Unknown byte order — define LAPLACE_TRANSPORT_NATIVE_ENDIAN explicitly"
#endif

LAPLACE_STATIC_ASSERT((LAPLACE_TRANSPORT_INGRESS_CAPACITY & (LAPLACE_TRANSPORT_INGRESS_CAPACITY - 1u)) == 0u,
                       "ingress capacity must be power of two");
LAPLACE_STATIC_ASSERT((LAPLACE_TRANSPORT_EGRESS_CAPACITY & (LAPLACE_TRANSPORT_EGRESS_CAPACITY - 1u)) == 0u,
                       "egress capacity must be power of two");
LAPLACE_STATIC_ASSERT(LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE % LAPLACE_CACHELINE_SIZE == 0u,
                       "command record must be cache-line aligned size");
LAPLACE_STATIC_ASSERT(LAPLACE_TRANSPORT_EVENT_RECORD_SIZE % LAPLACE_CACHELINE_SIZE == 0u,
                       "event record must be cache-line aligned size");
LAPLACE_STATIC_ASSERT(LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE >= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES + 32u,
                       "command record must fit header + max payload");
LAPLACE_STATIC_ASSERT(LAPLACE_TRANSPORT_EVENT_RECORD_SIZE >= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES + 32u,
                       "event record must fit header + max payload");

typedef enum laplace_transport_cmd_kind {
    LAPLACE_TRANSPORT_CMD_INVALID            = 0u,
    LAPLACE_TRANSPORT_CMD_PING               = 1u,
    LAPLACE_TRANSPORT_CMD_REGISTER_PREDICATE = 2u,
    LAPLACE_TRANSPORT_CMD_REGISTER_CONSTANT  = 3u,
    LAPLACE_TRANSPORT_CMD_ASSERT_FACT        = 4u,
    LAPLACE_TRANSPORT_CMD_ADD_RULE           = 5u,
    LAPLACE_TRANSPORT_CMD_BUILD_TRIGGER_INDEX = 6u,
    LAPLACE_TRANSPORT_CMD_EXEC_STEP          = 7u,
    LAPLACE_TRANSPORT_CMD_EXEC_RUN           = 8u,
    LAPLACE_TRANSPORT_CMD_QUERY_STATS        = 9u,
    LAPLACE_TRANSPORT_CMD_COUNT_             = 10u
} laplace_transport_cmd_kind_t;

typedef enum laplace_transport_evt_kind {
    LAPLACE_TRANSPORT_EVT_INVALID        = 0u,
    LAPLACE_TRANSPORT_EVT_ACK            = 1u,
    LAPLACE_TRANSPORT_EVT_ERROR          = 2u,
    LAPLACE_TRANSPORT_EVT_FACT_COMMITTED = 3u,
    LAPLACE_TRANSPORT_EVT_RULE_ACCEPTED  = 4u,
    LAPLACE_TRANSPORT_EVT_RULE_REJECTED  = 5u,
    LAPLACE_TRANSPORT_EVT_EXEC_STATUS    = 6u,
    LAPLACE_TRANSPORT_EVT_STATS_SNAPSHOT = 7u,
    LAPLACE_TRANSPORT_EVT_COUNT_         = 8u
} laplace_transport_evt_kind_t;

typedef enum laplace_transport_status {
    LAPLACE_TRANSPORT_STATUS_OK                    = 0u,
    LAPLACE_TRANSPORT_STATUS_ERR_UNKNOWN_CMD       = 1u,
    LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PAYLOAD   = 2u,
    LAPLACE_TRANSPORT_STATUS_ERR_CAPACITY          = 3u,
    LAPLACE_TRANSPORT_STATUS_ERR_INVALID_PREDICATE = 4u,
    LAPLACE_TRANSPORT_STATUS_ERR_INVALID_ENTITY    = 5u,
    LAPLACE_TRANSPORT_STATUS_ERR_DUPLICATE         = 6u,
    LAPLACE_TRANSPORT_STATUS_ERR_VALIDATION_FAILED = 7u,
    LAPLACE_TRANSPORT_STATUS_ERR_EXEC_ERROR        = 8u,
    LAPLACE_TRANSPORT_STATUS_ERR_INTERNAL          = 9u,
    LAPLACE_TRANSPORT_STATUS_ERR_EGRESS_FULL       = 10u
} laplace_transport_status_t;

typedef uint64_t laplace_transport_correlation_id_t;

typedef struct laplace_transport_cmd_register_predicate {
    uint16_t predicate_id;
    uint8_t  arity;
    uint8_t  reserved;
    uint32_t flags;
    uint32_t fact_capacity;
} laplace_transport_cmd_register_predicate_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_register_predicate_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "register_predicate payload must fit in max payload");

typedef struct laplace_transport_cmd_register_constant {
    uint32_t entity_id;
    uint32_t generation;
    uint16_t type_id;
    uint16_t reserved;
    uint32_t flags;
} laplace_transport_cmd_register_constant_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_register_constant_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "register_constant payload must fit in max payload");

typedef struct laplace_transport_cmd_assert_fact {
    uint16_t predicate_id;
    uint8_t  arg_count;
    uint8_t  reserved;
    uint32_t flags;
    uint32_t args[LAPLACE_EXACT_MAX_ARITY];
} laplace_transport_cmd_assert_fact_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_assert_fact_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "assert_fact payload must fit in max payload");

typedef struct laplace_transport_term {
    uint8_t  kind;       /* 0=invalid, 1=variable, 2=constant */
    uint8_t  reserved;
    uint32_t value;      /* variable: var_id (16-bit range), constant: entity_id */
} laplace_transport_term_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_term_t) == 8u,
                       "transport term must be 8 bytes for alignment");

typedef struct laplace_transport_literal {
    uint16_t                  predicate_id;
    uint8_t                   arity;
    uint8_t                   reserved;
    laplace_transport_term_t  terms[LAPLACE_EXACT_MAX_ARITY];
} laplace_transport_literal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_literal_t) == 4u + LAPLACE_EXACT_MAX_ARITY * 8u,
                       "transport literal size must match");

typedef struct laplace_transport_cmd_add_rule {
    uint32_t                     body_count;
    laplace_transport_literal_t  head;
    laplace_transport_literal_t  body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
} laplace_transport_cmd_add_rule_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_add_rule_t) <= LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE - 32u,
                       "add_rule payload must fit in command record minus header");

typedef struct laplace_transport_cmd_exec_run {
    uint32_t max_steps;
    uint32_t max_derivations;
    uint8_t  mode;      /* 0=dense, 1=sparse */
    uint8_t  reserved[3];
} laplace_transport_cmd_exec_run_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_exec_run_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "exec_run payload must fit in max payload");

typedef struct laplace_transport_evt_ack {
    uint32_t detail;  /* command-specific detail (e.g., assigned ID) */
} laplace_transport_evt_ack_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_ack_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "ack payload must fit in max payload");

typedef struct laplace_transport_evt_error {
    uint32_t error_code;     /* laplace_transport_status_t */
    uint32_t detail;         /* additional context */
} laplace_transport_evt_error_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_error_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "error payload must fit in max payload");

typedef struct laplace_transport_evt_fact_committed {
    uint32_t fact_row;
    uint32_t entity_id;
    uint32_t entity_generation;
    uint16_t predicate_id;
    uint8_t  arg_count;
    uint8_t  inserted;         /* 1 if newly inserted, 0 if duplicate */
} laplace_transport_evt_fact_committed_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_fact_committed_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "fact_committed payload must fit in max payload");

typedef struct laplace_transport_evt_rule_accepted {
    uint32_t rule_id;
} laplace_transport_evt_rule_accepted_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_rule_accepted_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "rule_accepted payload must fit in max payload");

typedef struct laplace_transport_evt_rule_rejected {
    uint32_t validation_error;   /* laplace_exact_rule_validation_error_t */
    uint32_t literal_index;
    uint32_t term_index;
} laplace_transport_evt_rule_rejected_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_rule_rejected_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "rule_rejected payload must fit in max payload");

typedef struct laplace_transport_evt_exec_status {
    uint32_t run_status;        /* laplace_exec_run_status_t or step result */
    uint64_t steps_executed;
    uint64_t facts_derived;
    uint64_t facts_deduplicated;
} laplace_transport_evt_exec_status_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_exec_status_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "exec_status payload must fit in max payload");

typedef struct laplace_transport_evt_stats_snapshot {
    uint32_t predicate_count;
    uint32_t fact_count;
    uint32_t rule_count;
    uint32_t entity_alive_count;
    uint32_t entity_capacity;
    uint64_t exec_steps;
    uint64_t exec_facts_derived;
} laplace_transport_evt_stats_snapshot_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_stats_snapshot_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "stats_snapshot payload must fit in max payload");

typedef struct laplace_transport_command_record {
    uint32_t                          kind;           /* laplace_transport_cmd_kind_t */
    uint32_t                          payload_size;   /* actual payload bytes used */
    laplace_transport_correlation_id_t correlation_id;
    uint32_t                          flags;
    uint32_t                          sequence;       /* producer-assigned sequence number */
    uint8_t                           reserved[8];
    uint8_t                           payload[LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE - 32u];
} laplace_transport_command_record_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_command_record_t) == LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE,
                       "command record must be exactly COMMAND_RECORD_SIZE bytes");

typedef struct laplace_transport_event_record {
    uint32_t                          kind;           /* laplace_transport_evt_kind_t */
    uint32_t                          status;         /* laplace_transport_status_t */
    laplace_transport_correlation_id_t correlation_id;
    uint32_t                          payload_size;
    uint32_t                          sequence;       /* kernel-assigned sequence number */
    uint8_t                           reserved[4];
    uint8_t                           payload[LAPLACE_TRANSPORT_EVENT_RECORD_SIZE - 32u];
} laplace_transport_event_record_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_event_record_t) == LAPLACE_TRANSPORT_EVENT_RECORD_SIZE,
                       "event record must be exactly EVENT_RECORD_SIZE bytes");

typedef struct laplace_transport_ring_header {
    LAPLACE_ALIGNAS(LAPLACE_CACHELINE_SIZE)
    volatile uint32_t head;        /* producer write position */
    uint8_t           pad_head[LAPLACE_CACHELINE_SIZE - sizeof(uint32_t)];

    LAPLACE_ALIGNAS(LAPLACE_CACHELINE_SIZE)
    volatile uint32_t tail;        /* consumer read position */
    uint8_t           pad_tail[LAPLACE_CACHELINE_SIZE - sizeof(uint32_t)];
} laplace_transport_ring_header_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_ring_header_t) == 2u * LAPLACE_CACHELINE_SIZE,
                       "ring header must be exactly 2 cache lines");

typedef struct laplace_transport_mapping_header {
    LAPLACE_ALIGNAS(LAPLACE_CACHELINE_SIZE)
    uint32_t magic;
    uint32_t abi_version;
    uint32_t endian;               /* LAPLACE_TRANSPORT_ENDIAN_LITTLE / _BIG */
    uint32_t ingress_capacity;
    uint32_t egress_capacity;
    uint32_t command_record_size;
    uint32_t event_record_size;
    uint32_t total_mapping_size;
    uint32_t flags;
    uint32_t creator_pid;
    uint32_t reserved[6];
} laplace_transport_mapping_header_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_mapping_header_t) == LAPLACE_CACHELINE_SIZE,
                       "mapping header must be exactly one cache line");

#define LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE                                      \
    (sizeof(laplace_transport_mapping_header_t)                                   \
     + sizeof(laplace_transport_ring_header_t)                                    \
     + (size_t)LAPLACE_TRANSPORT_INGRESS_CAPACITY * LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE \
     + sizeof(laplace_transport_ring_header_t)                                    \
     + (size_t)LAPLACE_TRANSPORT_EGRESS_CAPACITY * LAPLACE_TRANSPORT_EVENT_RECORD_SIZE)

typedef struct laplace_transport_mapping {
    void*    view;
    void*    backend_handle;
    uint32_t total_size;
    bool     is_creator;
} laplace_transport_mapping_t;

laplace_error_t laplace_transport_create(laplace_transport_mapping_t* out_mapping,
                                          const char* name);

laplace_error_t laplace_transport_open(laplace_transport_mapping_t* out_mapping,
                                        const char* name);

void laplace_transport_close(laplace_transport_mapping_t* mapping);

bool laplace_transport_validate_header(const laplace_transport_mapping_t* mapping);

static inline laplace_transport_mapping_header_t*
laplace_transport_get_header(const laplace_transport_mapping_t* mapping) {
    LAPLACE_ASSERT(mapping != NULL && mapping->view != NULL);
    return (laplace_transport_mapping_header_t*)mapping->view;
}

static inline laplace_transport_ring_header_t*
laplace_transport_get_ingress_ring(const laplace_transport_mapping_t* mapping) {
    LAPLACE_ASSERT(mapping != NULL && mapping->view != NULL);
    uint8_t* base = (uint8_t*)mapping->view;
    return (laplace_transport_ring_header_t*)(base + sizeof(laplace_transport_mapping_header_t));
}

static inline laplace_transport_command_record_t*
laplace_transport_get_ingress_slots(const laplace_transport_mapping_t* mapping) {
    LAPLACE_ASSERT(mapping != NULL && mapping->view != NULL);
    uint8_t* base = (uint8_t*)mapping->view;
    size_t offset = sizeof(laplace_transport_mapping_header_t) + sizeof(laplace_transport_ring_header_t);
    return (laplace_transport_command_record_t*)(base + offset);
}

static inline laplace_transport_ring_header_t*
laplace_transport_get_egress_ring(const laplace_transport_mapping_t* mapping) {
    LAPLACE_ASSERT(mapping != NULL && mapping->view != NULL);
    uint8_t* base = (uint8_t*)mapping->view;
    size_t offset = sizeof(laplace_transport_mapping_header_t)
                  + sizeof(laplace_transport_ring_header_t)
                  + (size_t)LAPLACE_TRANSPORT_INGRESS_CAPACITY * LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE;
    return (laplace_transport_ring_header_t*)(base + offset);
}

static inline laplace_transport_event_record_t*
laplace_transport_get_egress_slots(const laplace_transport_mapping_t* mapping) {
    LAPLACE_ASSERT(mapping != NULL && mapping->view != NULL);
    uint8_t* base = (uint8_t*)mapping->view;
    size_t offset = sizeof(laplace_transport_mapping_header_t)
                  + sizeof(laplace_transport_ring_header_t)
                  + (size_t)LAPLACE_TRANSPORT_INGRESS_CAPACITY * LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE
                  + sizeof(laplace_transport_ring_header_t);
    return (laplace_transport_event_record_t*)(base + offset);
}

static inline uint32_t
laplace_transport_ring_count(const laplace_transport_ring_header_t* ring) {
    LAPLACE_ASSERT(ring != NULL);
    return ring->head - ring->tail;
}

static inline bool
laplace_transport_ring_is_empty(const laplace_transport_ring_header_t* ring) {
    return laplace_transport_ring_count(ring) == 0u;
}

static inline bool
laplace_transport_ring_is_full(const laplace_transport_ring_header_t* ring, uint32_t capacity) {
    return laplace_transport_ring_count(ring) >= capacity;
}

laplace_error_t laplace_transport_ingress_enqueue(laplace_transport_mapping_t* mapping,
                                                   const laplace_transport_command_record_t* record);

laplace_error_t laplace_transport_ingress_dequeue(laplace_transport_mapping_t* mapping,
                                                   laplace_transport_command_record_t* out_record);

laplace_error_t laplace_transport_egress_enqueue(laplace_transport_mapping_t* mapping,
                                                  const laplace_transport_event_record_t* record);

laplace_error_t laplace_transport_egress_dequeue(laplace_transport_mapping_t* mapping,
                                                  laplace_transport_event_record_t* out_record);

#ifdef __cplusplus
}
#endif

#endif
