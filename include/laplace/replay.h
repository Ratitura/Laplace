#ifndef LAPLACE_REPLAY_H
#define LAPLACE_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/trace.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Replay session metadata.
 *
 * Purpose:
 *   Bounded envelope that describes the deterministic execution context
 *   for a session, making reruns auditable and reproducible.
 *
 * Invariants:
 *   - Replay metadata is explicit and bounded.
 *   - Replay metadata does not rely on wall-clock truth.
 *   - Wall-clock timestamps may exist as auxiliary diagnostics but must
 *     not be replay truth.
 *   - Deterministic sequence numbers are preferred over time-based identity.
 *   - Replay metadata describes state; it does not override execution.
 */

enum {
    /*
     * Maximum length of the backend diagnostic name string.
     * Includes null terminator.
     */
    LAPLACE_REPLAY_BACKEND_NAME_MAX = 32u,

    /*
     * Maximum length of the build mode string.
     * Includes null terminator.
     */
    LAPLACE_REPLAY_BUILD_MODE_MAX = 16u
};

typedef uint64_t laplace_replay_session_id_t;

typedef struct laplace_replay_metadata {
    /* Session identity */
    laplace_replay_session_id_t session_id;

    /* Build context */
    uint32_t abi_version;           /* LAPLACE_TRANSPORT_ABI_VERSION or equivalent */
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    char     build_mode[LAPLACE_REPLAY_BUILD_MODE_MAX];
    char     backend_name[LAPLACE_REPLAY_BACKEND_NAME_MAX];

    /* HV configuration */
    uint32_t hv_dimension;          /* LAPLACE_HV_DIM */
    uint32_t hv_words;              /* LAPLACE_HV_WORDS */

    /* Execution configuration */
    uint32_t exec_mode;             /* laplace_exec_mode_t at session start */
    uint32_t max_steps;
    uint32_t max_derivations;
    bool     semi_naive_enabled;

    /* Branch/epoch configuration */
    uint32_t max_branches;
    uint32_t branch_max_owned_facts;
    uint32_t branch_max_owned_entities;

    /* Deterministic seed roots */
    uint64_t seed_root;             /* primary deterministic seed, or 0 if unset */

    /* Sequence markers */
    laplace_trace_seq_t start_sequence;  /* trace sequence at session start */
    laplace_trace_seq_t end_sequence;    /* trace sequence at session end (0 until closed) */

    /* Transport correlation */
    uint64_t first_transport_correlation;  /* first transport correlation ID seen, or 0 */
    uint64_t last_transport_correlation;   /* last transport correlation ID seen, or 0 */

    /* Auxiliary wall-clock diagnostics (NOT replay truth) */
    uint64_t wall_start_ns;         /* QPC/clock_gettime at session start */
    uint64_t wall_end_ns;           /* QPC/clock_gettime at session end (0 until closed) */

    /* Status flags */
    bool     is_open;               /* session is active */
    bool     is_closed;             /* session has been closed */
    uint8_t  reserved_flags[6];
} laplace_replay_metadata_t;

/*
 * Initialize a replay metadata envelope with current build/config state.
 *
 * Captures:
 *   - version info
 *   - build mode (debug/release)
 *   - HV dimension
 *   - backend name
 *   - compile-time branch/epoch limits
 *   - session_id from a deterministic or caller-supplied value
 *
 * Returns LAPLACE_OK on success.
 */
laplace_error_t laplace_replay_init(laplace_replay_metadata_t* meta,
                                     laplace_replay_session_id_t session_id);

/*
 * Capture execution configuration into replay metadata.
 * Should be called after exec context configuration but before first run.
 */
void laplace_replay_capture_exec_config(laplace_replay_metadata_t* meta,
                                         uint32_t exec_mode,
                                         uint32_t max_steps,
                                         uint32_t max_derivations,
                                         bool semi_naive);

/*
 * Record seed root in replay metadata.
 */
void laplace_replay_set_seed(laplace_replay_metadata_t* meta, uint64_t seed);

/*
 * Record start sequence from trace buffer.
 */
void laplace_replay_mark_start(laplace_replay_metadata_t* meta,
                                laplace_trace_seq_t start_sequence);

/*
 * Close the replay session, recording end sequence and wall-clock.
 */
void laplace_replay_mark_end(laplace_replay_metadata_t* meta,
                              laplace_trace_seq_t end_sequence);

/*
 * Update transport correlation range.
 * Called each time a transport command is processed.
 */
void laplace_replay_update_transport_correlation(laplace_replay_metadata_t* meta,
                                                  uint64_t correlation_id);

/*
 * Query whether a replay session is open.
 */
bool laplace_replay_is_open(const laplace_replay_metadata_t* meta);

/*
 * Get the session ID.
 */
laplace_replay_session_id_t laplace_replay_get_session_id(const laplace_replay_metadata_t* meta);

#ifdef __cplusplus
}
#endif

#endif
