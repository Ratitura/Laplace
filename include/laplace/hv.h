#ifndef LAPLACE_HV_H
#define LAPLACE_HV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packed binary hypervector representation.
 *
 * Design:
 *  - Binary/bipolar encoding: each dimension is one bit packed into uint64_t words.
 *  - Default dimension: 16384 bits = 256 * 64.
 *    Overridable at compile time via -DLAPLACE_HV_DIM=<N> (must be a positive
 *    multiple of 64).
 *  - Value type with fixed-size inline word array.  Can be embedded in SoA pools,
 *    allocated from arenas, or placed on the stack.
 *  - All operations are scalar reference implementations.
 *    SIMD/Highway backends are deferred to a later phase.
 *
 * Encoding semantics:
 *  - bind   = XOR   (self-inverse, commutative, associative)
 *  - unbind = XOR   (same operation for binary XOR encoding)
 *  - distance = Hamming distance via popcount
 *  - bundle = element-wise majority vote with deterministic tie-break
 *
 * Thread safety:
 *  - None.  Intended for shard-local usage.
 *
 * Scalar truth baseline:
 *  - This scalar implementation is the truth-preserving reference baseline.
 *  - Any future optimized backend is semantically subordinate to this scalar behavior.
 *  - Optimized implementations MUST match scalar outputs bit-exactly for
 *    all public operations and deterministic seeds.
 */

#ifndef LAPLACE_HV_DIM
#define LAPLACE_HV_DIM 16384u
#endif

#define LAPLACE_HV_BITS_PER_WORD  64u
#define LAPLACE_HV_WORDS          ((uint32_t)((LAPLACE_HV_DIM) / LAPLACE_HV_BITS_PER_WORD))
#define LAPLACE_HV_BYTES          ((uint32_t)(LAPLACE_HV_WORDS * sizeof(uint64_t)))
#define LAPLACE_HV_CACHELINES     ((uint32_t)(LAPLACE_HV_BYTES / 64u))

LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM > 0u,
                       "LAPLACE_HV_DIM must be positive");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM % LAPLACE_HV_BITS_PER_WORD == 0u,
                       "LAPLACE_HV_DIM must be a multiple of 64");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_WORDS == 256u || LAPLACE_HV_DIM != 16384u,
                       "default 16384-bit dimension must yield exactly 256 words");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_BYTES == LAPLACE_HV_WORDS * 8u,
                       "LAPLACE_HV_BYTES must equal LAPLACE_HV_WORDS * sizeof(uint64_t)");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM % 64u == 0u && LAPLACE_HV_BYTES % 64u == 0u,
                       "HV byte size must be a multiple of cache-line size (64)");

typedef struct laplace_hv {
    uint64_t words[LAPLACE_HV_WORDS];
} laplace_hv_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_hv_t) == (size_t)LAPLACE_HV_WORDS * sizeof(uint64_t),
                       "laplace_hv_t must be tightly packed");

void laplace_hv_zero(laplace_hv_t* dst);

/*
 * Fill with deterministic pseudo-random bits derived from `seed`.
 * Uses SplitMix64.  Same seed always produces the same vector (bit-exact).
 */
void laplace_hv_random(laplace_hv_t* dst, uint64_t seed);

/*
 * Copy src into dst.
 */
void laplace_hv_copy(laplace_hv_t* dst, const laplace_hv_t* src);

/*
 * Bind (XOR):  dst = a ^ b.
 * dst may alias a or b.
 */
void laplace_hv_bind(laplace_hv_t* dst,
                      const laplace_hv_t* a,
                      const laplace_hv_t* b);

/*
 * Unbind (XOR):  dst = a ^ b.
 * Semantically identical to bind for binary XOR encoding.
 * Provided as a distinct entry point for readability at call sites.
 */
static inline void laplace_hv_unbind(laplace_hv_t* dst,
                                      const laplace_hv_t* a,
                                      const laplace_hv_t* b) {
    laplace_hv_bind(dst, a, b);
}

/*
 * Hamming distance between two hypervectors.
 * Returns the number of bit positions where a and b differ.
 * Range: [0, LAPLACE_HV_DIM].
 */
uint32_t laplace_hv_distance(const laplace_hv_t* a,
                              const laplace_hv_t* b);

/*
 * Majority-vote bundle with deterministic tie-break.
 *
 * For each bit position p in [0, LAPLACE_HV_DIM):
 *   - Count how many input vectors have bit p set.
 *   - If count > count/2 (strict majority): output bit = 1.
 *   - If count < ceil(count/2): output bit = 0.
 *   - If count is even AND votes are exactly half (tie):
 *     output bit is determined by the tie-break vector generated from `tie_seed`.
 *
 * Tie-break vector: a pseudo-random HV produced by SplitMix64 seeded with
 * `tie_seed`.  Same seed → same tie-break → bit-exact determinism.
 *
 * count must be >= 1.
 * vectors is an array of `count` pointers to laplace_hv_t.
 * dst must not alias any input vector.
 *
 * Internally dispatches to specialized fast paths:
 *   count == 1  ->  copy
 *   count == 2  ->  word-level tie-break (no counters, no bit loops)
 *   count == 3  ->  bitwise majority (no counters, no bit loops)
 *   count == 4  ->  carry-chain counter (no per-bit loops)
 *   otherwise   ->  generic K-input bit-sliced parallel counter path
 */
void laplace_hv_bundle(laplace_hv_t* dst,
                        const laplace_hv_t* const* vectors,
                        uint32_t count,
                        uint64_t tie_seed);

/*
 * Reference bundle implementation (test oracle).
 *
 * Semantically identical to laplace_hv_bundle but uses the simplest possible
 * bit-by-bit scalar algorithm.  Intended for correctness verification only.
 * MUST NOT be used in production hot paths.
 *
 * Same inputs + same tie_seed -> bit-exact match with laplace_hv_bundle.
 *
 * Freeze contract:
 *  - This function is the canonical semantic oracle for bundle correctness
 *    during backend optimization work.
 */
void laplace_hv_bundle_reference(laplace_hv_t* dst,
                                  const laplace_hv_t* const* vectors,
                                  uint32_t count,
                                  uint64_t tie_seed);

/*
 * Force the generic bit-sliced scalar bundle path, bypassing all fast-path
 * dispatch.  Intended for testing and benchmarking only.
 *
 * Produces bit-exact results identical to laplace_hv_bundle and
 * laplace_hv_bundle_reference for all valid inputs.
 *
 * Supports count in [1, 31].
 */
void laplace_hv_bundle_generic(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

/*
 * Direct invocation of the bundle-of-2 fast path, bypassing dispatch.
 * Intended for benchmarking dispatch overhead only.
 *
 * count must be exactly 2.  Asserts if violated.
 * Produces bit-exact results identical to laplace_hv_bundle for count=2.
 */
void laplace_hv_bundle2_direct(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

/*
 * Direct invocation of the bundle-of-3 fast path, bypassing dispatch.
 * Intended for benchmarking dispatch overhead only.
 *
 * count must be exactly 3.  Asserts if violated.
 * tie_seed is accepted for API uniformity but is unused (no ties with k=3).
 * Produces bit-exact results identical to laplace_hv_bundle for count=3.
 */
void laplace_hv_bundle3_direct(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

uint32_t laplace_hv_popcount(const laplace_hv_t* hv);

bool laplace_hv_equal(const laplace_hv_t* a, const laplace_hv_t* b);

/*
 * Normalized similarity in [0.0, 1.0].
 * Computed as 1.0 - (hamming_distance / LAPLACE_HV_DIM).
 * 1.0 = identical, 0.0 = maximally dissimilar.
 */
double laplace_hv_similarity(const laplace_hv_t* a, const laplace_hv_t* b);

/*
 * Backend identity query.
 *
 * Returns a static string identifying the active HV backend:
 *   "scalar" — scalar-only reference backend
 *   "ispc"   — ISPC-accelerated first-wave backend
 *
 * This is the stable public diagnostic interface for backend identification.
 * Use this function instead of inspecting LAPLACE_HV_BACKEND macros.
 * The macro is private to the laplace_core build.
 */
const char* laplace_hv_backend_name(void);

/*
 * Per-operation backend identity queries.
 *
 * Each returns "scalar" or "ispc" indicating which backend is active for
 * that specific operation.  When per-op policy overrides are in effect,
 * these may differ from laplace_hv_backend_name().
 *
 * Example: ISPC backend is available but popcount is forced scalar:
 *   laplace_hv_backend_name()          → "ispc"
 *   laplace_hv_backend_name_bind()     → "ispc"
 *   laplace_hv_backend_name_distance() → "ispc"
 *   laplace_hv_backend_name_popcount() → "scalar"
 */
const char* laplace_hv_backend_name_bind(void);
const char* laplace_hv_backend_name_distance(void);
const char* laplace_hv_backend_name_popcount(void);

#ifdef __cplusplus
}
#endif

#endif
