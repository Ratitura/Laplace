#include "hv_backend_internal.h"

#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC

#include <stdint.h>
#include "laplace/assert.h"

#include "hv_kernels_ispc.h"

void laplace__hv_bind_words_ispc(uint64_t* restrict dst,
                                  const uint64_t* a,
                                  const uint64_t* b,
                                  uint32_t num_words) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    LAPLACE_ASSERT(num_words <= (uint32_t)INT32_MAX);

    laplace_ispc_bind_words(dst, a, b, (int32_t)num_words);
}

uint32_t laplace__hv_xor_popcount_words_ispc(const uint64_t* a,
                                              const uint64_t* b,
                                              uint32_t num_words) {
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    LAPLACE_ASSERT(num_words <= (uint32_t)INT32_MAX);

    const int32_t result = laplace_ispc_xor_popcount_words(a, b,
                                                            (int32_t)num_words);
    LAPLACE_ASSERT(result >= 0);
    return (uint32_t)result;
}

uint32_t laplace__hv_popcount_words_ispc(const uint64_t* words,
                                          uint32_t num_words) {
    LAPLACE_ASSERT(words != NULL);
    LAPLACE_ASSERT(num_words <= (uint32_t)INT32_MAX);

    const int32_t result = laplace_ispc_popcount_words(words,
                                                        (int32_t)num_words);
    LAPLACE_ASSERT(result >= 0);
    return (uint32_t)result;
}

#endif /* LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC */
