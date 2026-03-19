#ifndef LAPLACE_HV_BACKEND_INTERNAL_H
#define LAPLACE_HV_BACKEND_INTERNAL_H

/*
 * Internal backend kernel interface for HV microkernels (Phase 03).
 *
 * This header declares the word-level kernel signatures used by hv.c
 * to dispatch first-wave operations to either scalar or ISPC backends.
 *
 * Architecture:
 *   - hv_scalar.c provides laplace__hv_*_scalar() implementations.
 *     These are always compiled and available.
 *   - hv_ispc_bridge.c provides laplace__hv_*_ispc() implementations
 *     that wrap ISPC-generated entry points.  Only compiled when
 *     LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC.
 *   - hv.c calls the active backend via inline dispatch helpers at
 *     the bottom of this header.
 *
 * This header is INTERNAL.  It must not be included by public headers
 * or by code outside src/.
 *
 * Scalar truth contract:
 *   Every ISPC kernel must produce bit-exact output matching its scalar
 *   counterpart for the same inputs.  Any mismatch is a bug in the ISPC
 *   kernel, not in the scalar reference.
 */

#include <stdint.h>
#include "laplace/config.h"
#include "laplace/hv.h"

/* ========================================================================= */
/*  Scalar backend kernels (always available)                                */
/* ========================================================================= */

/*
 * Scalar bind: dst[i] = a[i] ^ b[i] for i in [0, LAPLACE_HV_WORDS).
 */
void laplace__hv_bind_words_scalar(uint64_t* restrict dst,
                                    const uint64_t* a,
                                    const uint64_t* b,
                                    uint32_t num_words);

/*
 * Scalar XOR-popcount: returns popcount(a[i] ^ b[i]) summed over
 * i in [0, num_words).  Used by distance().
 */
uint32_t laplace__hv_xor_popcount_words_scalar(const uint64_t* a,
                                                const uint64_t* b,
                                                uint32_t num_words);

/*
 * Scalar popcount: returns popcount(words[i]) summed over i in [0, num_words).
 */
uint32_t laplace__hv_popcount_words_scalar(const uint64_t* words,
                                            uint32_t num_words);

/* ========================================================================= */
/*  ISPC backend kernels (only when ISPC backend is enabled)                 */
/* ========================================================================= */

#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC

void laplace__hv_bind_words_ispc(uint64_t* restrict dst,
                                  const uint64_t* a,
                                  const uint64_t* b,
                                  uint32_t num_words);

uint32_t laplace__hv_xor_popcount_words_ispc(const uint64_t* a,
                                              const uint64_t* b,
                                              uint32_t num_words);

uint32_t laplace__hv_popcount_words_ispc(const uint64_t* words,
                                          uint32_t num_words);

#endif /* LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC */

/* ========================================================================= */
/*  Backend dispatch (per-operation compile-time selection)                   */
/* ========================================================================= */

/*
 * Per-operation dispatch macros.
 *
 * When ISPC backend is available (LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC),
 * each operation independently selects its backend via the per-op policy
 * macros from config.h.  This allows optimal backend selection per operation.
 *
 * When ISPC is not available, all operations unconditionally use scalar.
 */

static inline void laplace__hv_bind_words(uint64_t* restrict dst,
                                           const uint64_t* a,
                                           const uint64_t* b,
                                           uint32_t num_words) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_BIND_USE_ISPC
    laplace__hv_bind_words_ispc(dst, a, b, num_words);
#else
    laplace__hv_bind_words_scalar(dst, a, b, num_words);
#endif
}

static inline uint32_t laplace__hv_xor_popcount_words(const uint64_t* a,
                                                       const uint64_t* b,
                                                       uint32_t num_words) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_XOR_POPCOUNT_USE_ISPC
    return laplace__hv_xor_popcount_words_ispc(a, b, num_words);
#else
    return laplace__hv_xor_popcount_words_scalar(a, b, num_words);
#endif
}

static inline uint32_t laplace__hv_popcount_words(const uint64_t* words,
                                                   uint32_t num_words) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_POPCOUNT_USE_ISPC
    return laplace__hv_popcount_words_ispc(words, num_words);
#else
    return laplace__hv_popcount_words_scalar(words, num_words);
#endif
}

#endif /* LAPLACE_HV_BACKEND_INTERNAL_H */
