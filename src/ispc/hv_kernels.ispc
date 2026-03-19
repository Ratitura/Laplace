/*
 * ISPC microkernels for HV word-level operations (Phase 03).
 *
 * These kernels operate on packed uint64 word arrays and are called
 * through the C bridge layer (hv_ispc_bridge.c).
 *
 * Correctness contract:
 *   Every kernel must produce bit-exact output matching the scalar
 *   reference in hv_scalar.c for the same inputs.  Any mismatch is
 *   a bug here, not in the scalar reference.
 *
 * Representation:
 *   HV words are uint64.  ISPC processes them as varying uint64.
 *   Default: 256 words per HV (16384 bits, Phase 03.2 standardization).
 *
 * Operations:
 *   1. bind       — dst[i] = a[i] ^ b[i]
 *   2. popcount   — sum of popcount(words[i])
 *   3. xor_popcount — sum of popcount(a[i] ^ b[i])
 *
 * Similarity is computed in the C bridge layer from xor_popcount
 * (division by DIM), so no separate ISPC kernel is needed for it.
 */

/*
 * Bind: dst[i] = a[i] ^ b[i] for i in [0, n).
 *
 * XOR is bit-exact regardless of SIMD lane width or execution order.
 */
export void laplace_ispc_bind_words(uniform uint64 dst[],
                                     uniform const uint64 a[],
                                     uniform const uint64 b[],
                                     uniform int32 n) {
    foreach (i = 0 ... n) {
        dst[i] = a[i] ^ b[i];
    }
}

/*
 * Portable popcount for a single uint64 value.
 *
 * Uses the Hamming weight tree reduction (bit-hack) approach.
 * This compiles to efficient SIMD popcount on targets with native
 * support and remains correct on all targets.
 *
 * The constant-based approach avoids any platform-dependent intrinsic
 * and produces bit-exact results matching __builtin_popcountll.
 */
inline int32 ispc_popcount64(uint64 v) {
    /* Hamming weight via parallel bit summation */
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    /* Multiply by 0x0101010101010101 and take top byte */
    v = v * 0x0101010101010101ULL;
    return (int32)(v >> 56);
}

/*
 * Popcount: returns sum of popcount(words[i]) for i in [0, n).
 *
 * Uses a two-phase approach:
 *   1. ISPC foreach loop computes per-lane partial sums (varying).
 *   2. reduce_add collapses to a single uniform result.
 *
 * The reduction is exact (integer addition is associative/commutative).
 */
export uniform int32 laplace_ispc_popcount_words(uniform const uint64 words[],
                                                  uniform int32 n) {
    int32 partial = 0;
    foreach (i = 0 ... n) {
        partial += ispc_popcount64(words[i]);
    }
    return reduce_add(partial);
}

/*
 * XOR-Popcount: returns sum of popcount(a[i] ^ b[i]) for i in [0, n).
 *
 * Used by distance() and similarity().
 * Same two-phase reduction approach as popcount.
 */
export uniform int32 laplace_ispc_xor_popcount_words(uniform const uint64 a[],
                                                      uniform const uint64 b[],
                                                      uniform int32 n) {
    int32 partial = 0;
    foreach (i = 0 ... n) {
        partial += ispc_popcount64(a[i] ^ b[i]);
    }
    return reduce_add(partial);
}
