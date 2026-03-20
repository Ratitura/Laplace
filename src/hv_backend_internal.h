#ifndef LAPLACE_HV_BACKEND_INTERNAL_H
#define LAPLACE_HV_BACKEND_INTERNAL_H

#include <stdint.h>
#include "laplace/config.h"
#include "laplace/hv.h"

void laplace__hv_bind_words_scalar(uint64_t* restrict dst,
                                    const uint64_t* a,
                                    const uint64_t* b,
                                    uint32_t num_words);

uint32_t laplace__hv_xor_popcount_words_scalar(const uint64_t* a,
                                                const uint64_t* b,
                                                uint32_t num_words);

uint32_t laplace__hv_popcount_words_scalar(const uint64_t* words,
                                            uint32_t num_words);

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
