#ifndef LAPLACE_ADAPTER_HV_H
#define LAPLACE_ADAPTER_HV_H

#include "laplace/adapter.h"
#include "laplace/hv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- HV payload header (32 bytes) ----------
 *
 * Metadata for an externally-produced hypervector blob.
 * The actual HV words are passed separately as a contiguous array
 * of exactly hv_words × uint64_t elements.
 * Encoder provenance fields are optional metadata (may be zero).
 */
typedef struct laplace_adapter_hv_header {
    uint32_t abi_version;       /*  0 -  3 */
    uint32_t hv_dimension;      /*  4 -  7 */
    uint32_t hv_words;          /*  8 - 11 */
    uint32_t encoder_id;        /* 12 - 15  (optional encoder provenance) */
    uint32_t encoder_version;   /* 16 - 19  (optional encoder provenance) */
    uint32_t reserved;          /* 20 - 23 */
    uint64_t encoder_seed;      /* 24 - 31  (deterministic encoder metadata) */
} laplace_adapter_hv_header_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_hv_header_t) == 32u,
                       "laplace_adapter_hv_header_t must be exactly 32 bytes");

/* ---------- HV ingest result (8 bytes) ---------- */

typedef struct laplace_adapter_hv_result {
    laplace_adapter_status_t status;    /* 0 - 3 */
    uint32_t reserved;                  /* 4 - 7 */
} laplace_adapter_hv_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_hv_result_t) == 8u,
                       "laplace_adapter_hv_result_t must be exactly 8 bytes");

/*
 * Validate the HV payload header without accessing the words.
 * Checks ABI version and dimension/word count consistency against
 * the compile-time LAPLACE_HV_DIM / LAPLACE_HV_WORDS constants.
 */
laplace_adapter_status_t laplace_adapter_validate_hv_header(
    const laplace_adapter_hv_header_t* header);

/*
 * Ingest an externally-produced HV payload into a kernel HV struct.
 * Validates the header, then copies exactly LAPLACE_HV_WORDS words
 * from the provided array into out_hv.
 *
 * words must point to at least header->hv_words contiguous uint64_t
 * elements.  Passing NULL for words is an error.
 */
laplace_adapter_status_t laplace_adapter_hv_ingest(
    const laplace_adapter_hv_header_t* header,
    const uint64_t* words,
    laplace_hv_t* out_hv);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_HV_H */
