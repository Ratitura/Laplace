#include "laplace/hv.h"
#include "hv_backend_internal.h"

#include <string.h>

static inline uint64_t laplace__splitmix64(uint64_t* const state) {
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

void laplace_hv_zero(laplace_hv_t* const dst) {
    LAPLACE_ASSERT(dst != NULL);
    memset(dst->words, 0, sizeof(dst->words));
}

void laplace_hv_random(laplace_hv_t* const dst, uint64_t seed) {
    LAPLACE_ASSERT(dst != NULL);
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        dst->words[i] = laplace__splitmix64(&seed);
    }
}

void laplace_hv_copy(laplace_hv_t* const dst, const laplace_hv_t* const src) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(src != NULL);
    memcpy(dst->words, src->words, sizeof(dst->words));
}

void laplace_hv_bind(laplace_hv_t* const dst,
                      const laplace_hv_t* const a,
                      const laplace_hv_t* const b) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    laplace__hv_bind_words(dst->words, a->words, b->words, LAPLACE_HV_WORDS);
}

uint32_t laplace_hv_distance(const laplace_hv_t* const a,
                              const laplace_hv_t* const b) {
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    return laplace__hv_xor_popcount_words(a->words, b->words, LAPLACE_HV_WORDS);
}

void laplace_hv_bundle_reference(laplace_hv_t* const dst,
                                  const laplace_hv_t* const* const vectors,
                                  const uint32_t count,
                                  uint64_t tie_seed) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(vectors != NULL);
    LAPLACE_ASSERT(count >= 1u);

    const uint32_t half = count / 2u;
    const bool even = (count % 2u == 0u);
    uint64_t tie_prng = tie_seed;

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        uint32_t counts[64];
        memset(counts, 0, sizeof(counts));

        for (uint32_t v = 0u; v < count; ++v) {
            LAPLACE_ASSERT(vectors[v] != NULL);
            uint64_t word = vectors[v]->words[w];
            for (uint32_t b = 0u; b < 64u; ++b) {
                counts[b] += (uint32_t)(word & 1u);
                word >>= 1;
            }
        }

        const uint64_t tie_word = laplace__splitmix64(&tie_prng);

        uint64_t result = 0u;
        for (uint32_t b = 0u; b < 64u; ++b) {
            if (counts[b] > half) {
                result |= (UINT64_C(1) << b);
            } else if (even && counts[b] == half) {
                result |= tie_word & (UINT64_C(1) << b);
            }
        }

        dst->words[w] = result;
    }
}

static inline uint32_t laplace__bit_width(uint32_t n) {
    uint32_t w = 0u;
    while (n > 0u) {
        ++w;
        n >>= 1;
    }
    return w > 0u ? w : 1u;
}

#define LAPLACE__HV_BUNDLE_MAX_PLANES 5u

static inline uint64_t laplace__bitslice_ge(const uint64_t* const digit,
                                             const uint32_t num_planes,
                                             const uint32_t threshold) {
    uint64_t borrow = 0u;
    for (uint32_t p = 0u; p < num_planes; ++p) {
        if ((threshold >> p) & 1u) {
            borrow = ~digit[p] | borrow;
        } else {
            borrow = ~digit[p] & borrow;
        }
    }
    return ~borrow;
}

static inline void laplace__hv_prebatch_tie_words(uint64_t* const restrict tie_buf,
                                                   uint64_t tie_seed) {
    uint64_t prng = tie_seed;
    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        tie_buf[w] = laplace__splitmix64(&prng);
    }
}

static void laplace__hv_bundle2_words(laplace_hv_t* const restrict dst,
                                       const laplace_hv_t* const a,
                                       const laplace_hv_t* const b,
                                       uint64_t tie_seed) {
    uint64_t tie_buf[LAPLACE_HV_WORDS];
    laplace__hv_prebatch_tie_words(tie_buf, tie_seed);

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        const uint64_t wa = a->words[w];
        const uint64_t wb = b->words[w];
        dst->words[w] = (wa & wb) | ((wa ^ wb) & tie_buf[w]);
    }
}

static void laplace__hv_bundle3_words(laplace_hv_t* const restrict dst,
                                       const laplace_hv_t* const a,
                                       const laplace_hv_t* const b,
                                       const laplace_hv_t* const c) {
    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        const uint64_t wa = a->words[w];
        const uint64_t wb = b->words[w];
        const uint64_t wc = c->words[w];
        dst->words[w] = (wa & wb) | (wa & wc) | (wb & wc);
    }
}

static void laplace__hv_bundle4_words(laplace_hv_t* const restrict dst,
                                       const laplace_hv_t* const a,
                                       const laplace_hv_t* const b,
                                       const laplace_hv_t* const c,
                                       const laplace_hv_t* const d,
                                       uint64_t tie_seed) {
    uint64_t tie_buf[LAPLACE_HV_WORDS];
    laplace__hv_prebatch_tie_words(tie_buf, tie_seed);

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        const uint64_t wa = a->words[w];
        const uint64_t wb = b->words[w];
        const uint64_t wc = c->words[w];
        const uint64_t wd = d->words[w];

        const uint64_t s0        = wa ^ wb;
        const uint64_t c0        = wa & wb;
        const uint64_t s1        = wc ^ wd;
        const uint64_t c1        = wc & wd;
        const uint64_t bit0      = s0 ^ s1;
        const uint64_t carry_low = s0 & s1;
        const uint64_t c0_xor_c1 = c0 ^ c1;
        const uint64_t bit1      = c0_xor_c1 ^ carry_low;
        const uint64_t bit2      = (c0 & c1) | (c0_xor_c1 & carry_low);

        const uint64_t majority = (bit1 & bit0) | bit2;

        const uint64_t tie_mask = bit1 & ~bit0 & ~bit2;

        dst->words[w] = majority | (tie_mask & tie_buf[w]);
    }
}

static void laplace__hv_bundle_generic(laplace_hv_t* const restrict dst,
                                        const laplace_hv_t* const* const restrict vectors,
                                        const uint32_t count,
                                        uint64_t tie_seed) {
    const uint32_t half       = count / 2u;
    const bool     even       = (count % 2u == 0u);
    const uint32_t num_planes = laplace__bit_width(count);

    LAPLACE_ASSERT(num_planes <= LAPLACE__HV_BUNDLE_MAX_PLANES);

    uint64_t tie_buf[LAPLACE_HV_WORDS];
    if (even) {
        laplace__hv_prebatch_tie_words(tie_buf, tie_seed);
    }

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        uint64_t digit[LAPLACE__HV_BUNDLE_MAX_PLANES] = {0u, 0u, 0u, 0u, 0u};

        for (uint32_t v = 0u; v < count; ++v) {
            uint64_t carry = vectors[v]->words[w];
            for (uint32_t p = 0u; p < num_planes; ++p) {
                const uint64_t new_carry = digit[p] & carry;
                digit[p] ^= carry;
                carry = new_carry;
                if (carry == 0u) break;
            }
        }

        const uint64_t majority = laplace__bitslice_ge(digit, num_planes,
                                                        half + 1u);

        uint64_t result = majority;
        if (even) {
            const uint64_t ge_half = laplace__bitslice_ge(digit, num_planes,
                                                           half);
            const uint64_t tie_mask = ge_half & ~majority;
            result |= (tie_mask & tie_buf[w]);
        }

        dst->words[w] = result;
    }
}

void laplace_hv_bundle(laplace_hv_t* const dst,
                        const laplace_hv_t* const* const vectors,
                        const uint32_t count,
                        uint64_t tie_seed) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(vectors != NULL);
    LAPLACE_ASSERT(count >= 1u);

    if (count == 1u) {
        LAPLACE_ASSERT(vectors[0] != NULL);
        laplace_hv_copy(dst, vectors[0]);
        return;
    }

    if (count == 2u) {
        LAPLACE_ASSERT(vectors[0] != NULL);
        LAPLACE_ASSERT(vectors[1] != NULL);
        laplace__hv_bundle2_words(dst, vectors[0], vectors[1], tie_seed);
        return;
    }

    if (count == 3u) {
        LAPLACE_ASSERT(vectors[0] != NULL);
        LAPLACE_ASSERT(vectors[1] != NULL);
        LAPLACE_ASSERT(vectors[2] != NULL);
        laplace__hv_bundle3_words(dst, vectors[0], vectors[1], vectors[2]);
        return;
    }

    if (count == 4u) {
        LAPLACE_ASSERT(vectors[0] != NULL);
        LAPLACE_ASSERT(vectors[1] != NULL);
        LAPLACE_ASSERT(vectors[2] != NULL);
        LAPLACE_ASSERT(vectors[3] != NULL);
        laplace__hv_bundle4_words(dst, vectors[0], vectors[1],
                                   vectors[2], vectors[3], tie_seed);
        return;
    }

    for (uint32_t v = 0u; v < count; ++v) {
        LAPLACE_ASSERT(vectors[v] != NULL);
    }

    laplace__hv_bundle_generic(dst, vectors, count, tie_seed);
}

void laplace_hv_bundle_generic(laplace_hv_t* const dst,
                                const laplace_hv_t* const* const vectors,
                                const uint32_t count,
                                uint64_t tie_seed) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(vectors != NULL);
    LAPLACE_ASSERT(count >= 1u);

    for (uint32_t v = 0u; v < count; ++v) {
        LAPLACE_ASSERT(vectors[v] != NULL);
    }

    laplace__hv_bundle_generic(dst, vectors, count, tie_seed);
}

void laplace_hv_bundle2_direct(laplace_hv_t* const dst,
                                const laplace_hv_t* const* const vectors,
                                const uint32_t count,
                                uint64_t tie_seed) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(vectors != NULL);
    LAPLACE_ASSERT(count == 2u);
    LAPLACE_ASSERT(vectors[0] != NULL);
    LAPLACE_ASSERT(vectors[1] != NULL);
    (void)count;
    laplace__hv_bundle2_words(dst, vectors[0], vectors[1], tie_seed);
}

void laplace_hv_bundle3_direct(laplace_hv_t* const dst,
                                const laplace_hv_t* const* const vectors,
                                const uint32_t count,
                                uint64_t tie_seed) {
    LAPLACE_ASSERT(dst != NULL);
    LAPLACE_ASSERT(vectors != NULL);
    LAPLACE_ASSERT(count == 3u);
    LAPLACE_ASSERT(vectors[0] != NULL);
    LAPLACE_ASSERT(vectors[1] != NULL);
    LAPLACE_ASSERT(vectors[2] != NULL);
    (void)count;
    (void)tie_seed;
    laplace__hv_bundle3_words(dst, vectors[0], vectors[1], vectors[2]);
}

uint32_t laplace_hv_popcount(const laplace_hv_t* const hv) {
    LAPLACE_ASSERT(hv != NULL);
    return laplace__hv_popcount_words(hv->words, LAPLACE_HV_WORDS);
}

bool laplace_hv_equal(const laplace_hv_t* const a,
                       const laplace_hv_t* const b) {
    LAPLACE_ASSERT(a != NULL);
    LAPLACE_ASSERT(b != NULL);
    return memcmp(a->words, b->words, sizeof(a->words)) == 0;
}

double laplace_hv_similarity(const laplace_hv_t* const a,
                              const laplace_hv_t* const b) {
    const uint32_t dist = laplace_hv_distance(a, b);
    return 1.0 - ((double)dist / (double)LAPLACE_HV_DIM);
}

const char* laplace_hv_backend_name(void) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC
    return "ispc";
#else
    return "scalar";
#endif
}

const char* laplace_hv_backend_name_bind(void) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_BIND_USE_ISPC
    return "ispc";
#else
    return "scalar";
#endif
}

const char* laplace_hv_backend_name_distance(void) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_XOR_POPCOUNT_USE_ISPC
    return "ispc";
#else
    return "scalar";
#endif
}

const char* laplace_hv_backend_name_popcount(void) {
#if LAPLACE_HV_BACKEND == LAPLACE_HV_BACKEND_ISPC && LAPLACE_HV_OP_POPCOUNT_USE_ISPC
    return "ispc";
#else
    return "scalar";
#endif
}
