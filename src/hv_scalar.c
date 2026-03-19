#include "hv_backend_internal.h"

#include <stdint.h>

#include "laplace/assert.h"

static inline uint32_t laplace__popcount64_scalar(const uint64_t value) {
#if defined(__clang__) || defined(__GNUC__)
    return (uint32_t)__builtin_popcountll(value);
#elif defined(_MSC_VER)
    return (uint32_t)__popcnt64(value);
#else
    uint64_t v = value;
    uint32_t count = 0u;
    while (v) {
        v &= (v - 1u);
        ++count;
    }
    return count;
#endif
}

void laplace__hv_bind_words_scalar(uint64_t* restrict dst,
                                    const uint64_t* a,
                                    const uint64_t* b,
                                    uint32_t num_words) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    for (uint32_t i = 0u; i < num_words; ++i) {
        dst[i] = a[i] ^ b[i];
    }
}

uint32_t laplace__hv_xor_popcount_words_scalar(const uint64_t* a,
                                                const uint64_t* b,
                                                uint32_t num_words) {
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < num_words; ++i) {
        total += laplace__popcount64_scalar(a[i] ^ b[i]);
    }
    return total;
}

uint32_t laplace__hv_popcount_words_scalar(const uint64_t* words,
                                            uint32_t num_words) {
    LAPLACE_ASSERT(words != NULL);
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < num_words; ++i) {
        total += laplace__popcount64_scalar(words[i]);
    }
    return total;
}
