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

/*
 * Platform-neutral memory-mapped transport ABI.
 *
 * Purpose:
 *   Defines a backend-neutral transport boundary for cross-process
 *   command ingress and event egress using fixed-layout memory-mapped
 *   ring buffers.
 *
 * Transport model:
 *   Single-producer/single-consumer (SPSC) for both rings.
 *   - Ingress: external producer writes commands, kernel reads.
 *   - Egress:  kernel writes events, external consumer reads.
 *
 * Backend neutrality:
 *   This ABI is structurally valid whether the backing memory comes from:
 *   - Win32 page-file-backed shared memory (CreateFileMappingW)
 *   - POSIX shared memory (shm_open / mmap)
 *   - FPGA MMIO / BAR-mapped mailbox windows
 *   - FPGA BRAM / AXI memory-mapped ring buffers
 *
 *   Backend-specific mapping lifecycle (create/open/close) is implemented
 *   in separate backend source files (e.g. transport_win32.c).  Public
 *   APIs use backend-neutral types and identifiers only.
 *
 * Truth authority:
 *   Transport may NOT bypass exact symbolic validation.
 *   Commands request operations; the exact/execution APIs decide truth.
 *
 * ABI stability:
 *   The ABI is versioned via a magic number and version field in the
 *   mapping header.  Layout is explicit, fixed-size, and cache-line-aware.
 *   Endianness is declared in the header.  No variable-length payloads.
 *   No raw pointers cross the boundary.
 *
 * Memory-ordering policy:
 *   Producer:
 *     1. Write payload into the slot.
 *     2. Issue a release fence (__atomic_thread_fence(RELEASE) / MemoryBarrier).
 *     3. Advance head counter.
 *   Consumer:
 *     1. Issue an acquire fence (__atomic_thread_fence(ACQUIRE) / MemoryBarrier).
 *     2. Read head counter and check for available slots.
 *     3. Read payload from the slot.
 *     4. Issue a release fence.
 *     5. Advance tail counter.
 *   This release/acquire protocol ensures payload visibility before
 *   ownership transfer.  It is valid for host shared memory and
 *   conceptually equivalent for FPGA MMIO-visible ring state.
 */

enum {
    /*
     * Magic identifier for the transport mapping header.
     * "LAPL" in little-endian: 0x4C50414C.
     */
    LAPLACE_TRANSPORT_MAGIC = 0x4C50414Cu,

    /*
     * ABI version.  Incremented on any incompatible layout change.
     */
    LAPLACE_TRANSPORT_ABI_VERSION = 1u,

    /*
     * Number of command slots in the ingress ring.
     * Must be a power of two for efficient modular arithmetic.
     */
    LAPLACE_TRANSPORT_INGRESS_CAPACITY = 256u,

    /*
     * Number of event slots in the egress ring.
     * Must be a power of two for efficient modular arithmetic.
     */
    LAPLACE_TRANSPORT_EGRESS_CAPACITY = 256u,

    /*
     * Maximum payload size in bytes for a single command or event record.
     * Chosen to fit the largest command payload (add_rule) with headroom.
     * add_rule payload: 4 + (4 + MAX_ARITY*8) + MAX_BODY*(4 + MAX_ARITY*8)
     *                 = 4 + 68 + 8*68 = 616 bytes.
     */
    LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES = 672u,

    /*
     * Total size of a single command record (header + payload).
     * Must be a multiple of 64 bytes for cache-line alignment.
     * 32-byte header + 672-byte payload = 704 bytes.
     */
    LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE = 704u,

    /*
     * Total size of a single event record (header + payload).
     * Must be a multiple of 64 bytes for cache-line alignment.
     * 32-byte header + 672-byte payload = 704 bytes.
     */
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

/*
 * REGISTER_PREDICATE payload.
 * predicate_id: the numeric predicate identifier (1..MAX_PREDICATES-1)
 * arity:        column count (1..MAX_ARITY)
 * flags:        predicate flags
 * fact_capacity: per-predicate posting capacity hint (0 = default 256)
 */
typedef struct laplace_transport_cmd_register_predicate {
    uint16_t predicate_id;
    uint8_t  arity;
    uint8_t  reserved;
    uint32_t flags;
    uint32_t fact_capacity;
} laplace_transport_cmd_register_predicate_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_register_predicate_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "register_predicate payload must fit in max payload");

/*
 * REGISTER_CONSTANT payload.
 * entity_id + generation: handle of the pre-allocated entity.
 * type_id:  exact type identifier.
 * flags:    registration flags.
 */
typedef struct laplace_transport_cmd_register_constant {
    uint32_t entity_id;
    uint32_t generation;
    uint16_t type_id;
    uint16_t reserved;
    uint32_t flags;
} laplace_transport_cmd_register_constant_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_register_constant_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "register_constant payload must fit in max payload");

/*
 * ASSERT_FACT payload.
 * predicate_id:  target predicate.
 * arg_count:     number of argument entity IDs (1..MAX_ARITY).
 * flags:         fact flags (e.g., ASSERTED).
 * args:          array of entity IDs (not handles — generation not verified here).
 *
 * Note: For simplicity in the initial ABI, facts use raw entity IDs in args.
 * The command processing bridge resolves and validates entities.
 */
typedef struct laplace_transport_cmd_assert_fact {
    uint16_t predicate_id;
    uint8_t  arg_count;
    uint8_t  reserved;
    uint32_t flags;
    uint32_t args[LAPLACE_EXACT_MAX_ARITY];
} laplace_transport_cmd_assert_fact_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_assert_fact_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "assert_fact payload must fit in max payload");

/*
 * ADD_RULE payload.
 * Encodes a bounded positive Horn/Datalog rule in fixed-size form.
 *
 * Layout:
 *   head:  one literal (predicate_id, arity, terms[MAX_ARITY])
 *   body:  body_count literals (predicate_id, arity, terms[MAX_ARITY])
 *
 * Terms are encoded as (kind, value) pairs where kind=1 is variable,
 * kind=2 is constant.
 *   variable value: 16-bit variable ID (must be >= 1).
 *   constant value: 32-bit entity ID.
 */
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

/*
 * EXEC_RUN payload — optional budget overrides.
 */
typedef struct laplace_transport_cmd_exec_run {
    uint32_t max_steps;
    uint32_t max_derivations;
    uint8_t  mode;      /* 0=dense, 1=sparse */
    uint8_t  reserved[3];
} laplace_transport_cmd_exec_run_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_cmd_exec_run_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "exec_run payload must fit in max payload");

/*
 * ACK payload — confirms successful processing of a command.
 */
typedef struct laplace_transport_evt_ack {
    uint32_t detail;  /* command-specific detail (e.g., assigned ID) */
} laplace_transport_evt_ack_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_ack_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "ack payload must fit in max payload");

/*
 * ERROR payload.
 */
typedef struct laplace_transport_evt_error {
    uint32_t error_code;     /* laplace_transport_status_t */
    uint32_t detail;         /* additional context */
} laplace_transport_evt_error_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_error_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "error payload must fit in max payload");

/*
 * FACT_COMMITTED payload.
 */
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

/*
 * RULE_ACCEPTED payload.
 */
typedef struct laplace_transport_evt_rule_accepted {
    uint32_t rule_id;
} laplace_transport_evt_rule_accepted_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_rule_accepted_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "rule_accepted payload must fit in max payload");

/*
 * RULE_REJECTED payload.
 */
typedef struct laplace_transport_evt_rule_rejected {
    uint32_t validation_error;   /* laplace_exact_rule_validation_error_t */
    uint32_t literal_index;
    uint32_t term_index;
} laplace_transport_evt_rule_rejected_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_rule_rejected_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "rule_rejected payload must fit in max payload");

/*
 * EXEC_STATUS payload.
 */
typedef struct laplace_transport_evt_exec_status {
    uint32_t run_status;        /* laplace_exec_run_status_t or step result */
    uint64_t steps_executed;
    uint64_t facts_derived;
    uint64_t facts_deduplicated;
} laplace_transport_evt_exec_status_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_evt_exec_status_t) <= LAPLACE_TRANSPORT_MAX_PAYLOAD_BYTES,
                       "exec_status payload must fit in max payload");

/*
 * STATS_SNAPSHOT payload.
 */
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
    /* Payload area */
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
    /* Payload area */
    uint8_t                           payload[LAPLACE_TRANSPORT_EVENT_RECORD_SIZE - 32u];
} laplace_transport_event_record_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_transport_event_record_t) == LAPLACE_TRANSPORT_EVENT_RECORD_SIZE,
                       "event record must be exactly EVENT_RECORD_SIZE bytes");

/*
 * SPSC ring header.
 *
 * Producer owns `head` (write position).
 * Consumer owns `tail` (read position).
 * Each on its own cache line to avoid false sharing.
 *
 * Ring is empty when head == tail.
 * Ring is full when (head - tail) == capacity.
 * Positions are monotonically increasing; modular indexing via & (capacity - 1).
 */
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

/*
 * Top-level transport mapping header.
 * Placed at byte offset 0 of the mapped region.
 *
 * Layout (from byte 0):
 *   [0..CACHELINE)                  mapping_header
 *   [header_end..ingress_ring)      ingress ring header (2 cache lines)
 *   [ingress_ring_header_end..)     ingress slots
 *   [ingress_end..egress_ring)      egress ring header (2 cache lines)
 *   [egress_ring_header_end..)      egress slots
 */
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

/*
 * Total mapped-memory region size:
 *   mapping_header (1 cache line)
 * + ingress ring header (2 cache lines)
 * + ingress slots (capacity * record_size)
 * + egress ring header (2 cache lines)
 * + egress slots (capacity * record_size)
 */
#define LAPLACE_TRANSPORT_TOTAL_MAPPING_SIZE                                      \
    (sizeof(laplace_transport_mapping_header_t)                                   \
     + sizeof(laplace_transport_ring_header_t)                                    \
     + (size_t)LAPLACE_TRANSPORT_INGRESS_CAPACITY * LAPLACE_TRANSPORT_COMMAND_RECORD_SIZE \
     + sizeof(laplace_transport_ring_header_t)                                    \
     + (size_t)LAPLACE_TRANSPORT_EGRESS_CAPACITY * LAPLACE_TRANSPORT_EVENT_RECORD_SIZE)

/*
 * Backend-neutral mapping handle.
 *
 * `view`           — pointer to the mapped memory region.
 * `backend_handle` — opaque backend-specific handle:
 *                     Win32: HANDLE from CreateFileMappingW, stored as void*.
 *                     POSIX: file descriptor, stored via cast.
 *                     FPGA:  implementation-defined.
 * `total_size`     — total mapping size in bytes.
 * `is_creator`     — true if this side created (owns) the mapping.
 *
 * No OS-specific types appear in this struct.  Backend details are opaque.
 */
typedef struct laplace_transport_mapping {
    void*    view;
    void*    backend_handle;
    uint32_t total_size;
    bool     is_creator;
} laplace_transport_mapping_t;

/*
 * Create a new transport mapping with the given name.
 * Initializes the mapping header, ring headers, and endianness.
 *
 * The `name` is a UTF-8 null-terminated string that identifies the
 * transport endpoint.  Backend implementations map it to their native
 * naming convention (e.g. Win32 wide-string object name, POSIX shm name).
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_ARGUMENT if out_mapping or name is NULL.
 * Returns LAPLACE_ERR_NOT_SUPPORTED on platforms without a backend.
 * Returns LAPLACE_ERR_INTERNAL if the backend OS calls fail.
 */
laplace_error_t laplace_transport_create(laplace_transport_mapping_t* out_mapping,
                                          const char* name);

/*
 * Open an existing transport mapping by name.
 * Validates magic, ABI version, endianness, and layout parameters.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_ARGUMENT if out_mapping or name is NULL.
 * Returns LAPLACE_ERR_INVALID_STATE if header validation fails.
 * Returns LAPLACE_ERR_NOT_SUPPORTED on platforms without a backend.
 * Returns LAPLACE_ERR_INTERNAL if the backend OS calls fail.
 */
laplace_error_t laplace_transport_open(laplace_transport_mapping_t* out_mapping,
                                        const char* name);

/*
 * Close a transport mapping, unmapping the view and releasing backend handles.
 * Safe to call on a zeroed/invalid mapping (no-op).
 */
void laplace_transport_close(laplace_transport_mapping_t* mapping);

/*
 * Validate that a mapping's header is coherent.
 * Returns true if magic, version, endianness, and layout fields match.
 */
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

/*
 * Enqueue a command into the ingress ring.
 * Copies the record into the next available slot with release semantics.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if the ring is full.
 */
laplace_error_t laplace_transport_ingress_enqueue(laplace_transport_mapping_t* mapping,
                                                   const laplace_transport_command_record_t* record);

/*
 * Dequeue a command from the ingress ring.
 * Copies the record out with acquire semantics and advances the tail.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_STATE if the ring is empty.
 */
laplace_error_t laplace_transport_ingress_dequeue(laplace_transport_mapping_t* mapping,
                                                   laplace_transport_command_record_t* out_record);

/*
 * Enqueue an event into the egress ring.
 * Copies the record into the next available slot with release semantics.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if the ring is full.
 */
laplace_error_t laplace_transport_egress_enqueue(laplace_transport_mapping_t* mapping,
                                                  const laplace_transport_event_record_t* record);

/*
 * Dequeue an event from the egress ring.
 * Copies the record out with acquire semantics and advances the tail.
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_INVALID_STATE if the ring is empty.
 */
laplace_error_t laplace_transport_egress_dequeue(laplace_transport_mapping_t* mapping,
                                                  laplace_transport_event_record_t* out_record);

#ifdef __cplusplus
}
#endif

#endif
