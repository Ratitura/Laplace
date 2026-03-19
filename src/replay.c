#include "laplace/replay.h"

#include <string.h>

#include "laplace/branch.h"
#include "laplace/hv.h"
#include "laplace/version.h"

/* Platform timer for auxiliary wall-clock diagnostics */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static uint64_t replay_wall_clock_ns(void) {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    const uint64_t ticks = (uint64_t)counter.QuadPart;
    const uint64_t hz    = (uint64_t)frequency.QuadPart;
    return (ticks / hz) * 1000000000ULL + ((ticks % hz) * 1000000000ULL) / hz;
}
#else
#include <time.h>
static uint64_t replay_wall_clock_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

laplace_error_t laplace_replay_init(laplace_replay_metadata_t* const meta,
                                     const laplace_replay_session_id_t session_id) {
    if (meta == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(meta, 0, sizeof(*meta));

    /* Session identity */
    meta->session_id = session_id;

    /* Build context */
    meta->abi_version    = 1u;  /* LAPLACE_TRANSPORT_ABI_VERSION */
    meta->version_major  = laplace_version_major();
    meta->version_minor  = laplace_version_minor();
    meta->version_patch  = laplace_version_patch();

#if LAPLACE_DEBUG
    memcpy(meta->build_mode, "debug", 6u); /* includes NUL */
#else
    memcpy(meta->build_mode, "release", 8u); /* includes NUL */
#endif

    const char* backend = laplace_hv_backend_name();
    {
        uint32_t i = 0u;
        for (; i < LAPLACE_REPLAY_BACKEND_NAME_MAX - 1u && backend[i] != '\0'; ++i) {
            meta->backend_name[i] = backend[i];
        }
        meta->backend_name[i] = '\0';
    }

    /* HV configuration */
    meta->hv_dimension = LAPLACE_HV_DIM;
    meta->hv_words     = LAPLACE_HV_WORDS;

    /* Branch/epoch compile-time limits */
    meta->max_branches              = LAPLACE_BRANCH_MAX_BRANCHES;
    meta->branch_max_owned_facts    = LAPLACE_BRANCH_MAX_OWNED_FACTS_PER_BRANCH;
    meta->branch_max_owned_entities = LAPLACE_BRANCH_MAX_OWNED_ENTITIES_PER_BRANCH;

    /* Wall-clock diagnostic (NOT replay truth) */
    meta->wall_start_ns = replay_wall_clock_ns();

    meta->is_open   = true;
    meta->is_closed = false;

    return LAPLACE_OK;
}

void laplace_replay_capture_exec_config(laplace_replay_metadata_t* const meta,
                                         const uint32_t exec_mode,
                                         const uint32_t max_steps,
                                         const uint32_t max_derivations,
                                         const bool semi_naive) {
    if (meta == NULL) {
        return;
    }
    meta->exec_mode        = exec_mode;
    meta->max_steps        = max_steps;
    meta->max_derivations  = max_derivations;
    meta->semi_naive_enabled = semi_naive;
}

void laplace_replay_set_seed(laplace_replay_metadata_t* const meta, const uint64_t seed) {
    if (meta == NULL) {
        return;
    }
    meta->seed_root = seed;
}

void laplace_replay_mark_start(laplace_replay_metadata_t* const meta,
                                const laplace_trace_seq_t start_sequence) {
    if (meta == NULL) {
        return;
    }
    meta->start_sequence = start_sequence;
    meta->wall_start_ns  = replay_wall_clock_ns();
}

void laplace_replay_mark_end(laplace_replay_metadata_t* const meta,
                              const laplace_trace_seq_t end_sequence) {
    if (meta == NULL) {
        return;
    }
    meta->end_sequence = end_sequence;
    meta->wall_end_ns  = replay_wall_clock_ns();
    meta->is_open      = false;
    meta->is_closed    = true;
}

void laplace_replay_update_transport_correlation(laplace_replay_metadata_t* const meta,
                                                  const uint64_t correlation_id) {
    if (meta == NULL || correlation_id == 0u) {
        return;
    }
    if (meta->first_transport_correlation == 0u) {
        meta->first_transport_correlation = correlation_id;
    }
    meta->last_transport_correlation = correlation_id;
}

bool laplace_replay_is_open(const laplace_replay_metadata_t* const meta) {
    if (meta == NULL) {
        return false;
    }
    return meta->is_open && !meta->is_closed;
}

laplace_replay_session_id_t laplace_replay_get_session_id(const laplace_replay_metadata_t* const meta) {
    if (meta == NULL) {
        return 0u;
    }
    return meta->session_id;
}
