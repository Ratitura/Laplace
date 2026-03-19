#include "laplace/hv.h"
#include "hv_backend_internal.h"

#include <string.h>

/*
 * SplitMix64 PRNG.
 *
 * Deterministic, fast, statistically excellent for seeding and filling.
 * The state is mutated in place; the return value is the mixed output.
 * Same initial state always produces the same sequence (bit-exact).
 */
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

/*
 * Scalar HV freeze invariant:
 *   - The scalar HV implementation in this translation unit is the
 *     truth-preserving baseline for all future optimized backends.
 *   - Backend optimizations may change throughput only; they may not change
 *     bit-level semantics.
 *   - laplace_hv_bundle_reference is retained as the canonical oracle for
 *     bundle correctness validation.
 */

/*
 * Reference bundle implementation (test-visible).
 *
 * This is the simplest semantically trusted form of majority-vote bundling.
 * It uses a per-bit counter array and bit-by-bit extraction.  Correct but
 * structurally slow: O(WORDS * count * 64) bit-level iterations for the
 * tally phase alone.
 *
 * Preserved as a test oracle.
 * MUST NOT be used in production hot paths.
 */
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

/* ---------- internal helpers for bit-sliced parallel counting ---------- */

/*
 * Number of binary digits (bit-width) needed to represent the value n.
 * Returns ceil(log2(n+1)), minimum 1.
 *
 * Examples: bit_width(0)=1, bit_width(1)=1, bit_width(3)=2,
 *           bit_width(4)=3, bit_width(7)=3, bit_width(15)=4, bit_width(31)=5.
 */
static inline uint32_t laplace__bit_width(uint32_t n) {
    uint32_t w = 0u;
    while (n > 0u) {
        ++w;
        n >>= 1;
    }
    return w > 0u ? w : 1u;
}

/*
 * Maximum number of bit-slice planes.
 * 5 planes handle per-position counts up to 31 (supports bundle of up to 31 inputs).
 */
#define LAPLACE__HV_BUNDLE_MAX_PLANES 5u

/*
 * Parallel comparison: compute a 64-bit mask where bit b is set iff
 * the per-position count at column b is >= threshold.
 *
 * digit[0..num_planes-1] hold the bit-sliced per-position counts:
 *   digit[0] = LSB plane, digit[num_planes-1] = MSB plane.
 *
 * Algorithm: parallel binary subtraction of the constant `threshold` from
 * each column's count, tracking the borrow chain.  Positions with no final
 * borrow have count >= threshold.
 *
 * For each bit p of threshold:
 *   threshold_bit = 1: borrow_out = ~digit[p] | borrow_in
 *   threshold_bit = 0: borrow_out = ~digit[p] & borrow_in
 *
 * Cost: num_planes iterations, 2-3 bitwise ops per iteration.
 */
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

/* ---------- PRNG pre-batch helper ---------- */

/*
 * Pre-generate LAPLACE_HV_WORDS tie-break words into a caller-provided buffer.
 *
 * This removes the SplitMix64 sequential dependency chain from inner loops.
 * The PRNG state after this call is identical to having called splitmix64
 * LAPLACE_HV_WORDS times inline — same seed produces same sequence, so
 * bit-exact parity with the reference path is preserved.
 *
 * The buffer occupies 256 * 8 = 2048 bytes on stack (= 1 HV worth).
 * At 64-byte cache lines, this is 32 lines — trivially L1-resident.
 */
static inline void laplace__hv_prebatch_tie_words(uint64_t* const restrict tie_buf,
                                                   uint64_t tie_seed) {
    uint64_t prng = tie_seed;
    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        tie_buf[w] = laplace__splitmix64(&prng);
    }
}

/* ---------- specialized bundle fast paths ---------- */

/*
 * Bundle-of-2 fast path (even count, pure tie-break):
 *
 *   bundle2(a, b, tie) = (a & b) | ((a ^ b) & tie)
 *
 * Meaning per bit:
 *   - both set (a=1, b=1)  -> output 1  (agreement)
 *   - both clear (a=0, b=0) -> output 0  (agreement)
 *   - differ (a!=b)          -> use tie-break bit  (tie)
 *
 * Tie-break words are pre-generated before entering the word loop,
 * removing the PRNG sequential dependency from the critical path.
 * Same seed -> same sequence -> bit-exact determinism.
 *
 * Cost: LAPLACE_HV_WORDS iterations, 4 bitwise ops + 1 load per word.
 */
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

/*
 * Bundle-of-3 fast path:  maj3(a, b, c) = (a & b) | (a & c) | (b & c)
 *
 * For 3 inputs (odd count), no ties are possible — strict majority always
 * resolves.  The bitwise majority identity computes the exact result in a
 * single word-level pass: LAPLACE_HV_WORDS iterations with 5 bitwise ops per word.
 *
 * tie_seed is accepted for API uniformity but is unused (no ties with k=3).
 */
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

/*
 * Bundle-of-4 fast path (even count, carry-chain counter):
 *
 * For 4 inputs, uses a parallel 3-bit carry-chain adder to compute the
 * per-position vote count, then resolves majority (count >= 3) and
 * tie (count == 2) in word-level operations.
 *
 * Carry-chain addition of 4 single-bit inputs a,b,c,d:
 *   s0 = a ^ b          (half-add low)
 *   c0 = a & b          (half-add carry)
 *   s1 = c ^ d          (half-add low)
 *   c1 = c & d          (half-add carry)
 *   bit0 = s0 ^ s1      (total sum bit 0)
 *   carry = s0 & s1
 *   bit1 = c0 ^ c1 ^ carry
 *   bit2 = (c0 & c1) | ((c0 ^ c1) & carry)
 *
 * Resolution:
 *   majority = (bit1 & bit0) | bit2    (count in {3, 4})
 *   tie      = bit1 & ~bit0 & ~bit2    (count == 2)
 *   result   = majority | (tie & tie_word)
 *
 * Cost: LAPLACE_HV_WORDS iterations, ~19 bitwise ops + 1 load per word.
 */
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

        /* Parallel 3-bit carry-chain counter for 4 inputs. */
        const uint64_t s0        = wa ^ wb;
        const uint64_t c0        = wa & wb;
        const uint64_t s1        = wc ^ wd;
        const uint64_t c1        = wc & wd;
        const uint64_t bit0      = s0 ^ s1;
        const uint64_t carry_low = s0 & s1;
        const uint64_t c0_xor_c1 = c0 ^ c1;
        const uint64_t bit1      = c0_xor_c1 ^ carry_low;
        const uint64_t bit2      = (c0 & c1) | (c0_xor_c1 & carry_low);

        /* Majority: count >= 3  <=>  (bit1 & bit0) | bit2 */
        const uint64_t majority = (bit1 & bit0) | bit2;

        /* Tie: count == 2  <=>  bit1 & ~bit0 & ~bit2 */
        const uint64_t tie_mask = bit1 & ~bit0 & ~bit2;

        dst->words[w] = majority | (tie_mask & tie_buf[w]);
    }
}

/* ---------- generic K-input bundle (bit-sliced parallel counters) ---------- */

/*
 * Generic K-input bundle with bit-sliced parallel vote counting.
 *
 * This implementation replaces the bit-by-bit extraction loop
 * with a fundamentally different algorithm: bit-sliced parallel addition.
 *
 * Data structure: instead of uint32_t counts[64] (one scalar counter per bit
 * position), we use uint64_t digit[n] (one word per binary digit plane).
 * Each "column" across the digit planes holds the binary representation of
 * the vote count at that bit position.  All 64 bit positions within a word
 * are processed simultaneously by each bitwise operation.
 *
 * Accumulation: for each input vector, its word is added to the bit-sliced
 * counter using a parallel ripple-carry addition:
 *   carry = digit[0] & input_word
 *   digit[0] ^= input_word
 *   (propagate carry through higher planes, with early exit on zero carry)
 *
 * Resolution: count >= threshold is computed by parallel binary subtraction
 * (laplace__bitslice_ge).  Majority mask and tie mask are derived from two
 * threshold comparisons.
 *
 * Complexity per output word:
 *   Accumulation: K * n ops  (n = ceil(log2(K+1)), with early exit)
 *   Resolution:   2 * n ops  (two threshold comparisons + combine)
 *   Total: O(K * n) per word vs O(K * 64) for the bit-by-bit reference
 *
 * For K=7  (n=3): ~21 ops/word accumulation vs 448 for reference.
 * For K=15 (n=4): ~60 ops/word accumulation vs 960 for reference.
 *
 * Tie-break: SplitMix64 generates one pseudo-random word per output word,
 * in the same order as the reference path.
 *
 * Supports count in [1, 31].  Asserts if count exceeds 31.
 */
static void laplace__hv_bundle_generic(laplace_hv_t* const restrict dst,
                                        const laplace_hv_t* const* const restrict vectors,
                                        const uint32_t count,
                                        uint64_t tie_seed) {
    const uint32_t half       = count / 2u;
    const bool     even       = (count % 2u == 0u);
    const uint32_t num_planes = laplace__bit_width(count);

    LAPLACE_ASSERT(num_planes <= LAPLACE__HV_BUNDLE_MAX_PLANES);

    /* Pre-generate all tie-break words to remove PRNG from inner loop.
     * For odd K, no ties are possible — skip the pre-batch entirely. */
    uint64_t tie_buf[LAPLACE_HV_WORDS];
    if (even) {
        laplace__hv_prebatch_tie_words(tie_buf, tie_seed);
    }

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        /* Bit-sliced counter: digit[p] holds bit p of the per-position count.
         * digit[0] = LSB, digit[num_planes-1] = MSB. */
        uint64_t digit[LAPLACE__HV_BUNDLE_MAX_PLANES] = {0u, 0u, 0u, 0u, 0u};

        /* Accumulate each input vector's word via parallel ripple-carry add. */
        for (uint32_t v = 0u; v < count; ++v) {
            uint64_t carry = vectors[v]->words[w];
            for (uint32_t p = 0u; p < num_planes; ++p) {
                const uint64_t new_carry = digit[p] & carry;
                digit[p] ^= carry;
                carry = new_carry;
                if (carry == 0u) break;
            }
        }

        /* Resolve majority: count > half  <=>  count >= half + 1 */
        const uint64_t majority = laplace__bitslice_ge(digit, num_planes,
                                                        half + 1u);

        uint64_t result = majority;
        if (even) {
            /* Tie: count == half  <=>  (count >= half) AND NOT (count >= half+1) */
            const uint64_t ge_half = laplace__bitslice_ge(digit, num_planes,
                                                           half);
            const uint64_t tie_mask = ge_half & ~majority;
            result |= (tie_mask & tie_buf[w]);
        }

        dst->words[w] = result;
    }
}

/*
 * Production bundle entry point with fast-path dispatch.
 *
 * Dispatch logic:
 *   count == 1  ->  copy (trivial case)
 *   count == 2  ->  bundle2 word-level with tie-break
 *   count == 3  ->  bundle3 bitwise majority (no counters, no bit loops)
 *   count == 4  ->  bundle4 carry-chain counter with tie-break
 *   otherwise   ->  generic K-input bit-sliced parallel counter path
 *
 * All paths produce bit-exact identical output to the reference
 * implementation for the same inputs and tie_seed.
 */
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

    /* Validate all input pointers before entering hot loop. */
    for (uint32_t v = 0u; v < count; ++v) {
        LAPLACE_ASSERT(vectors[v] != NULL);
    }

    laplace__hv_bundle_generic(dst, vectors, count, tie_seed);
}

/*
 * Force the generic bit-sliced scalar bundle path, bypassing all fast-path
 * dispatch.  Intended for testing and benchmarking only.
 *
 * Semantics are identical to laplace_hv_bundle (and to
 * laplace_hv_bundle_reference) for all valid inputs.  The only difference
 * is that no specialized fast paths are used regardless of count.
 *
 * This function exists so that:
 *   1. Benchmarks can measure generic-path cost at counts that normally
 *      hit fast paths (k=2, k=3, k=4).
 *   2. Tests can verify generic-path correctness at all counts independently.
 */
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

/*
 * Direct invocation of the bundle-of-2 fast path, bypassing dispatch.
 * Intended for benchmarking dispatch overhead measurement only.
 *
 * count must be exactly 2.  Asserts if violated.
 * Produces bit-exact results identical to laplace_hv_bundle for count=2.
 */
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

/*
 * Direct invocation of the bundle-of-3 fast path, bypassing dispatch.
 * Intended for benchmarking dispatch overhead measurement only.
 *
 * count must be exactly 3.  Asserts if violated.
 * tie_seed is accepted for API uniformity but is unused (no ties with k=3).
 * Produces bit-exact results identical to laplace_hv_bundle for count=3.
 */
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
