#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/hv.h"
#include "test_harness.h"

static void hv_fill_ones(laplace_hv_t* dst) {
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        dst->words[i] = UINT64_MAX;
    }
}

static int test_hv_zero(void) {
    laplace_hv_t hv;
    laplace_hv_random(&hv, 42u);
    laplace_hv_zero(&hv);
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&hv) == 0u);
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        LAPLACE_TEST_ASSERT(hv.words[i] == 0u);
    }
    return 0;
}

static int test_hv_random_determinism(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 12345u);
    laplace_hv_random(&b, 12345u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&a, &b));

    laplace_hv_random(&b, 12346u);
    LAPLACE_TEST_ASSERT(!laplace_hv_equal(&a, &b));
    return 0;
}

static int test_hv_random_density(void) {
    laplace_hv_t hv;
    laplace_hv_random(&hv, 999u);
    const uint32_t pc = laplace_hv_popcount(&hv);
    const uint32_t expected = LAPLACE_HV_DIM / 2u;
    const uint32_t tolerance = 384u; /* ~6 sigma */
    LAPLACE_TEST_ASSERT(pc > expected - tolerance);
    LAPLACE_TEST_ASSERT(pc < expected + tolerance);
    return 0;
}

static int test_hv_copy(void) {
    laplace_hv_t src, dst;
    laplace_hv_random(&src, 777u);
    laplace_hv_zero(&dst);
    laplace_hv_copy(&dst, &src);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&dst, &src));
    return 0;
}

static int test_hv_bind_self_inverse(void) {
    laplace_hv_t a, b, bound, recovered;
    laplace_hv_random(&a, 100u);
    laplace_hv_random(&b, 200u);

    laplace_hv_bind(&bound, &a, &b);
    laplace_hv_bind(&recovered, &bound, &b);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&recovered, &a));

    laplace_hv_bind(&recovered, &bound, &a);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&recovered, &b));
    return 0;
}

static int test_hv_bind_commutativity(void) {
    laplace_hv_t a, b, ab, ba;
    laplace_hv_random(&a, 300u);
    laplace_hv_random(&b, 400u);

    laplace_hv_bind(&ab, &a, &b);
    laplace_hv_bind(&ba, &b, &a);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&ab, &ba));
    return 0;
}

static int test_hv_bind_associativity(void) {
    laplace_hv_t a, b, c, ab, ab_c, bc, a_bc;
    laplace_hv_random(&a, 500u);
    laplace_hv_random(&b, 600u);
    laplace_hv_random(&c, 700u);

    laplace_hv_bind(&ab, &a, &b);
    laplace_hv_bind(&ab_c, &ab, &c);

    laplace_hv_bind(&bc, &b, &c);
    laplace_hv_bind(&a_bc, &a, &bc);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&ab_c, &a_bc));
    return 0;
}

static int test_hv_bind_with_zero(void) {
    laplace_hv_t a, zero, result;
    laplace_hv_random(&a, 800u);
    laplace_hv_zero(&zero);

    laplace_hv_bind(&result, &a, &zero);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));
    return 0;
}

static int test_hv_bind_with_self(void) {
    laplace_hv_t a, result;
    laplace_hv_random(&a, 900u);

    laplace_hv_bind(&result, &a, &a);
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&result) == 0u);
    return 0;
}

static int test_hv_bind_in_place(void) {
    laplace_hv_t a_orig, a, b;
    laplace_hv_random(&a, 1000u);
    laplace_hv_copy(&a_orig, &a);
    laplace_hv_random(&b, 1001u);

    laplace_hv_bind(&a, &a, &b); /* in-place on a */

    laplace_hv_t recovered;
    laplace_hv_bind(&recovered, &a, &b);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&recovered, &a_orig));
    return 0;
}

static int test_hv_unbind_is_bind(void) {
    laplace_hv_t a, b, bound, unbound;
    laplace_hv_random(&a, 1100u);
    laplace_hv_random(&b, 1200u);

    laplace_hv_bind(&bound, &a, &b);
    laplace_hv_unbind(&unbound, &bound, &b);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&unbound, &a));
    return 0;
}

static int test_hv_distance_identity(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 2000u);
    LAPLACE_TEST_ASSERT(laplace_hv_distance(&a, &a) == 0u);
    return 0;
}

static int test_hv_distance_complement(void) {
    laplace_hv_t a, not_a;
    laplace_hv_random(&a, 2100u);
    laplace_hv_copy(&not_a, &a);
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        not_a.words[i] = ~not_a.words[i];
    }
    LAPLACE_TEST_ASSERT(laplace_hv_distance(&a, &not_a) == LAPLACE_HV_DIM);
    return 0;
}

static int test_hv_distance_symmetry(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 2200u);
    laplace_hv_random(&b, 2300u);
    LAPLACE_TEST_ASSERT(laplace_hv_distance(&a, &b) == laplace_hv_distance(&b, &a));
    return 0;
}

static int test_hv_distance_triangle_inequality(void) {
    laplace_hv_t a, b, c;
    laplace_hv_random(&a, 2400u);
    laplace_hv_random(&b, 2500u);
    laplace_hv_random(&c, 2600u);

    const uint32_t dab = laplace_hv_distance(&a, &b);
    const uint32_t dbc = laplace_hv_distance(&b, &c);
    const uint32_t dac = laplace_hv_distance(&a, &c);

    LAPLACE_TEST_ASSERT(dac <= dab + dbc);
    return 0;
}

static int test_hv_distance_zero_vectors(void) {
    laplace_hv_t zero, ones;
    laplace_hv_zero(&zero);
    hv_fill_ones(&ones);
    LAPLACE_TEST_ASSERT(laplace_hv_distance(&zero, &zero) == 0u);
    LAPLACE_TEST_ASSERT(laplace_hv_distance(&zero, &ones) == LAPLACE_HV_DIM);
    return 0;
}

static int test_hv_distance_random_near_half(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 2700u);
    laplace_hv_random(&b, 2800u);
    const uint32_t dist = laplace_hv_distance(&a, &b);
    const uint32_t expected = LAPLACE_HV_DIM / 2u;
    const uint32_t tolerance = 384u; /* ~6σ for binomial(16384, 0.5) */
    LAPLACE_TEST_ASSERT(dist > expected - tolerance);
    LAPLACE_TEST_ASSERT(dist < expected + tolerance);
    return 0;
}

static int test_hv_similarity(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 3000u);
    laplace_hv_copy(&b, &a);
    LAPLACE_TEST_ASSERT(laplace_hv_similarity(&a, &b) == 1.0);

    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        b.words[i] = ~b.words[i];
    }
    LAPLACE_TEST_ASSERT(laplace_hv_similarity(&a, &b) == 0.0);

    laplace_hv_random(&b, 3001u);
    const double sim = laplace_hv_similarity(&a, &b);
    LAPLACE_TEST_ASSERT(sim > 0.35 && sim < 0.65); /* near 0.5 */
    return 0;
}

static int test_hv_popcount(void) {
    laplace_hv_t hv;

    laplace_hv_zero(&hv);
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&hv) == 0u);

    hv_fill_ones(&hv);
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&hv) == LAPLACE_HV_DIM);

    laplace_hv_zero(&hv);
    hv.words[0] = 1u;
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&hv) == 1u);

    laplace_hv_zero(&hv);
    hv.words[LAPLACE_HV_WORDS - 1u] = UINT64_C(1) << 63;
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&hv) == 1u);

    return 0;
}

static int test_hv_equal(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 4000u);
    laplace_hv_copy(&b, &a);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&a, &b));

    b.words[LAPLACE_HV_WORDS / 2u] ^= 1u;
    LAPLACE_TEST_ASSERT(!laplace_hv_equal(&a, &b));
    return 0;
}

static int bundle_ref_vs_opt(const laplace_hv_t* const* vectors,
                              uint32_t count,
                              uint64_t tie_seed) {
    laplace_hv_t ref_result, opt_result;
    laplace_hv_bundle_reference(&ref_result, vectors, count, tie_seed);
    laplace_hv_bundle(&opt_result, vectors, count, tie_seed);
    if (!laplace_hv_equal(&ref_result, &opt_result)) {
        return 1;
    }
    return 0;
}

static int bundle_generic_vs_ref(const laplace_hv_t* const* vectors,
                                  uint32_t count,
                                  uint64_t tie_seed) {
    laplace_hv_t generic_result, ref_result;
    laplace_hv_bundle_generic(&generic_result, vectors, count, tie_seed);
    laplace_hv_bundle_reference(&ref_result, vectors, count, tie_seed);
    if (!laplace_hv_equal(&generic_result, &ref_result)) {
        return 1;
    }
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k1(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 9000u);
    const laplace_hv_t* ptrs[1] = { &a };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 1u, 0u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 1u, 999u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k2(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 9100u);
    laplace_hv_random(&b, 9101u);
    const laplace_hv_t* ptrs[2] = { &a, &b };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 2u, 42u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 2u, 7777u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k3(void) {
    laplace_hv_t vecs[3];
    const laplace_hv_t* ptrs[3];
    for (uint32_t i = 0u; i < 3u; ++i) {
        laplace_hv_random(&vecs[i], 9200u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 3u, 0u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 3u, 12345u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k7(void) {
    laplace_hv_t vecs[7];
    const laplace_hv_t* ptrs[7];
    for (uint32_t i = 0u; i < 7u; ++i) {
        laplace_hv_random(&vecs[i], 9300u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 7u, 0u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k15(void) {
    laplace_hv_t vecs[15];
    const laplace_hv_t* ptrs[15];
    for (uint32_t i = 0u; i < 15u; ++i) {
        laplace_hv_random(&vecs[i], 9400u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 15u, 0u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k4_even(void) {
    laplace_hv_t vecs[4];
    const laplace_hv_t* ptrs[4];
    for (uint32_t i = 0u; i < 4u; ++i) {
        laplace_hv_random(&vecs[i], 9500u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 4u, 42u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 4u, 9999u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k8_even(void) {
    laplace_hv_t vecs[8];
    const laplace_hv_t* ptrs[8];
    for (uint32_t i = 0u; i < 8u; ++i) {
        laplace_hv_random(&vecs[i], 9600u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 8u, 314u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_identical(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 9700u);

    const laplace_hv_t* ptrs3[3] = { &a, &a, &a };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs3, 3u, 0u) == 0);

    const laplace_hv_t* ptrs4[4] = { &a, &a, &a, &a };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs4, 4u, 42u) == 0);

    const laplace_hv_t* ptrs5[5] = { &a, &a, &a, &a, &a };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs5, 5u, 0u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_zeros(void) {
    laplace_hv_t z;
    laplace_hv_zero(&z);
    const laplace_hv_t* ptrs[3] = { &z, &z, &z };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 3u, 0u) == 0);

    const laplace_hv_t* ptrs2[2] = { &z, &z };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs2, 2u, 42u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_ones(void) {
    laplace_hv_t o;
    hv_fill_ones(&o);
    const laplace_hv_t* ptrs[3] = { &o, &o, &o };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 3u, 0u) == 0);

    const laplace_hv_t* ptrs2[2] = { &o, &o };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs2, 2u, 42u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_mixed_edge(void) {
    laplace_hv_t z, o, r;
    laplace_hv_zero(&z);
    hv_fill_ones(&o);
    laplace_hv_random(&r, 9800u);

    const laplace_hv_t* ptrs3[3] = { &z, &o, &r };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs3, 3u, 0u) == 0);

    const laplace_hv_t* ptrs2[2] = { &z, &o };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs2, 2u, 77u) == 0);

    const laplace_hv_t* ptrs4[4] = { &z, &o, &r, &r };
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs4, 4u, 88u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_multiseed(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        const uint32_t count = (uint32_t)(seed % 7u) + 1u; /* 1..7 */
        laplace_hv_t vecs[7];
        const laplace_hv_t* ptrs[7];
        for (uint32_t i = 0u; i < count; ++i) {
            laplace_hv_random(&vecs[i], seed * 10u + (uint64_t)i + 10000u);
            ptrs[i] = &vecs[i];
        }
        LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, count, seed + 500u) == 0);
    }
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k5_odd(void) {
    laplace_hv_t vecs[5];
    const laplace_hv_t* ptrs[5];
    for (uint32_t i = 0u; i < 5u; ++i) {
        laplace_hv_random(&vecs[i], 9900u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 5u, 0u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 5u, 7777u) == 0);
    return 0;
}

static int test_hv_bundle_ref_vs_opt_k6_even(void) {
    laplace_hv_t vecs[6];
    const laplace_hv_t* ptrs[6];
    for (uint32_t i = 0u; i < 6u; ++i) {
        laplace_hv_random(&vecs[i], 9950u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 6u, 42u) == 0);
    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 6u, 12345u) == 0);
    return 0;
}

static int test_hv_bundle_generic_vs_ref_sweep(void) {
    for (uint32_t k = 1u; k <= 15u; ++k) {
        laplace_hv_t vecs[15];
        const laplace_hv_t* ptrs[15];
        for (uint32_t i = 0u; i < k; ++i) {
            laplace_hv_random(&vecs[i], (uint64_t)k * 100u + (uint64_t)i + 20000u);
            ptrs[i] = &vecs[i];
        }
        LAPLACE_TEST_ASSERT(bundle_generic_vs_ref(ptrs, k, k + 1000u) == 0);
    }
    return 0;
}

static int test_hv_bundle_generic_vs_ref_even_ties(void) {
    for (uint32_t k = 2u; k <= 8u; k += 2u) {
        laplace_hv_t vecs[8];
        const laplace_hv_t* ptrs[8];
        for (uint32_t i = 0u; i < k; ++i) {
            laplace_hv_random(&vecs[i], (uint64_t)k * 200u + (uint64_t)i + 30000u);
            ptrs[i] = &vecs[i];
        }
        LAPLACE_TEST_ASSERT(bundle_generic_vs_ref(ptrs, k, 42u) == 0);
        LAPLACE_TEST_ASSERT(bundle_generic_vs_ref(ptrs, k, 999u) == 0);
    }
    return 0;
}

static int test_hv_bundle2_identity(void) {
    laplace_hv_t a, b, result;
    laplace_hv_random(&a, 11000u);
    laplace_hv_random(&b, 11001u);

    const laplace_hv_t* ptrs[2] = { &a, &b };
    laplace_hv_bundle(&result, ptrs, 2u, 42u);

    laplace_hv_t tie_vec;
    laplace_hv_random(&tie_vec, 42u); /* SplitMix64 with seed=42 produces this */

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        const uint64_t expected = (a.words[w] & b.words[w]) |
                                   ((a.words[w] ^ b.words[w]) & tie_vec.words[w]);
        LAPLACE_TEST_ASSERT(result.words[w] == expected);
    }
    return 0;
}

static int test_hv_bundle4_explicit(void) {
    laplace_hv_t v0, v1, v2, v3, result;
    laplace_hv_zero(&v0);
    laplace_hv_zero(&v1);
    laplace_hv_zero(&v2);
    laplace_hv_zero(&v3);

    v0.words[0] |= UINT64_C(1) << 0;
    v1.words[0] |= UINT64_C(1) << 0;
    v2.words[0] |= UINT64_C(1) << 0;
    v3.words[0] |= UINT64_C(1) << 0;

    v0.words[0] |= UINT64_C(1) << 1;
    v1.words[0] |= UINT64_C(1) << 1;
    v2.words[0] |= UINT64_C(1) << 1;

    v0.words[0] |= UINT64_C(1) << 2;
    v1.words[0] |= UINT64_C(1) << 2;

    v0.words[0] |= UINT64_C(1) << 3;

    const laplace_hv_t* ptrs[4] = { &v0, &v1, &v2, &v3 };
    laplace_hv_bundle(&result, ptrs, 4u, 42u);

    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 0)) != 0u); /* count=4 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 1)) != 0u); /* count=3 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 3)) == 0u); /* count=1 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 4)) == 0u); /* count=0 */

    laplace_hv_t result2;
    laplace_hv_bundle(&result2, ptrs, 4u, 42u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &result2));

    LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 4u, 42u) == 0);
    return 0;
}

static void hv_fill_alternating(laplace_hv_t* dst, uint64_t pattern) {
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        dst->words[i] = pattern;
    }
}

static int test_hv_bundle_alternating(void) {
    laplace_hv_t a, b, c;
    hv_fill_alternating(&a, UINT64_C(0xAAAAAAAAAAAAAAAA)); /* 1010... */
    hv_fill_alternating(&b, UINT64_C(0x5555555555555555)); /* 0101... */
    laplace_hv_random(&c, 12000u);

    {
        const laplace_hv_t* ptrs[2] = { &a, &b };
        LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 2u, 42u) == 0);
        LAPLACE_TEST_ASSERT(bundle_generic_vs_ref(ptrs, 2u, 42u) == 0);
    }

    {
        const laplace_hv_t* ptrs[3] = { &a, &a, &b };
        laplace_hv_t result;
        laplace_hv_bundle(&result, ptrs, 3u, 0u);
        LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));
        LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 3u, 0u) == 0);
    }

    {
        const laplace_hv_t* ptrs[4] = { &a, &b, &c, &c };
        LAPLACE_TEST_ASSERT(bundle_ref_vs_opt(ptrs, 4u, 88u) == 0);
        LAPLACE_TEST_ASSERT(bundle_generic_vs_ref(ptrs, 4u, 88u) == 0);
    }

    return 0;
}

static int test_hv_bundle2_direct_parity(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 13000u);
    laplace_hv_random(&b, 13001u);
    const laplace_hv_t* ptrs[2] = { &a, &b };

    laplace_hv_t dispatched, direct, ref;
    laplace_hv_bundle(&dispatched, ptrs, 2u, 42u);
    laplace_hv_bundle2_direct(&direct, ptrs, 2u, 42u);
    laplace_hv_bundle_reference(&ref, ptrs, 2u, 42u);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&dispatched, &direct));
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&direct, &ref));

    laplace_hv_bundle(&dispatched, ptrs, 2u, 9999u);
    laplace_hv_bundle2_direct(&direct, ptrs, 2u, 9999u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&dispatched, &direct));
    return 0;
}

static int test_hv_bundle3_direct_parity(void) {
    laplace_hv_t vecs[3];
    const laplace_hv_t* ptrs[3];
    for (uint32_t i = 0u; i < 3u; ++i) {
        laplace_hv_random(&vecs[i], 13100u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }

    laplace_hv_t dispatched, direct, ref;
    laplace_hv_bundle(&dispatched, ptrs, 3u, 42u);
    laplace_hv_bundle3_direct(&direct, ptrs, 3u, 42u);
    laplace_hv_bundle_reference(&ref, ptrs, 3u, 42u);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&dispatched, &direct));
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&direct, &ref));
    return 0;
}

static int test_hv_bundle_single(void) {
    laplace_hv_t a, result;
    laplace_hv_random(&a, 5000u);

    const laplace_hv_t* ptrs[1] = { &a };
    laplace_hv_bundle(&result, ptrs, 1u, 0u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));

    laplace_hv_bundle(&result, ptrs, 1u, 999u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));
    return 0;
}

static int test_hv_bundle_identical(void) {
    laplace_hv_t a, result;
    laplace_hv_random(&a, 5100u);

    const laplace_hv_t* ptrs3[3] = { &a, &a, &a };
    laplace_hv_bundle(&result, ptrs3, 3u, 0u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));

    const laplace_hv_t* ptrs5[5] = { &a, &a, &a, &a, &a };
    laplace_hv_bundle(&result, ptrs5, 5u, 0u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &a));
    return 0;
}

static int test_hv_bundle_odd_majority(void) {
    laplace_hv_t a, b, result;
    laplace_hv_random(&a, 5200u);
    laplace_hv_random(&b, 5201u);

    const laplace_hv_t* ptrs[3] = { &a, &a, &b };
    laplace_hv_bundle(&result, ptrs, 3u, 0u);

    const uint32_t da = laplace_hv_distance(&result, &a);
    const uint32_t db = laplace_hv_distance(&result, &b);
    LAPLACE_TEST_ASSERT(da < db);

    LAPLACE_TEST_ASSERT(da == 0u); /* 2-out-of-3 votes: a always wins */
    return 0;
}

static int test_hv_bundle_tie_break_determinism(void) {
    laplace_hv_t a, b, r1, r2;
    laplace_hv_random(&a, 5300u);
    laplace_hv_random(&b, 5301u);

    const laplace_hv_t* ptrs[2] = { &a, &b };
    laplace_hv_bundle(&r1, ptrs, 2u, 42u);
    laplace_hv_bundle(&r2, ptrs, 2u, 42u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&r1, &r2));
    return 0;
}

static int test_hv_bundle_tie_break_seed_sensitivity(void) {
    laplace_hv_t a, b, r1, r2;
    laplace_hv_random(&a, 5400u);
    laplace_hv_random(&b, 5401u);

    const laplace_hv_t* ptrs[2] = { &a, &b };
    laplace_hv_bundle(&r1, ptrs, 2u, 1u);
    laplace_hv_bundle(&r2, ptrs, 2u, 2u);
    LAPLACE_TEST_ASSERT(!laplace_hv_equal(&r1, &r2));
    return 0;
}

static int test_hv_bundle_even_count_ties(void) {
    laplace_hv_t a, b, r1, r2;
    laplace_hv_random(&a, 5500u);
    laplace_hv_random(&b, 5501u);

    const laplace_hv_t* ptrs[2] = { &a, &b };
    laplace_hv_bundle(&r1, ptrs, 2u, 100u);
    laplace_hv_bundle(&r2, ptrs, 2u, 200u);

    for (uint32_t w = 0u; w < LAPLACE_HV_WORDS; ++w) {
        const uint64_t agree_mask = ~(a.words[w] ^ b.words[w]); /* 1 where a==b */
        LAPLACE_TEST_ASSERT((r1.words[w] & agree_mask) == (r2.words[w] & agree_mask));
        const uint64_t both_one = a.words[w] & b.words[w];
        LAPLACE_TEST_ASSERT((r1.words[w] & both_one) == both_one);
        LAPLACE_TEST_ASSERT((r2.words[w] & both_one) == both_one);
        const uint64_t both_zero = ~a.words[w] & ~b.words[w];
        LAPLACE_TEST_ASSERT((r1.words[w] & both_zero) == 0u);
        LAPLACE_TEST_ASSERT((r2.words[w] & both_zero) == 0u);
    }
    return 0;
}

static int test_hv_bundle_large_odd(void) {
    laplace_hv_t vecs[7];
    const laplace_hv_t* ptrs[7];
    for (uint32_t i = 0u; i < 7u; ++i) {
        laplace_hv_random(&vecs[i], 6000u + (uint64_t)i);
        ptrs[i] = &vecs[i];
    }

    laplace_hv_t result;
    laplace_hv_bundle(&result, ptrs, 7u, 0u);

    const uint32_t half = LAPLACE_HV_DIM / 2u;
    for (uint32_t i = 0u; i < 7u; ++i) {
        const uint32_t d = laplace_hv_distance(&result, &vecs[i]);
        LAPLACE_TEST_ASSERT(d < half); /* closer than random chance */
    }
    return 0;
}

static int test_hv_bundle_preserves_agreement(void) {
    laplace_hv_t v0, v1, v2;
    laplace_hv_zero(&v0);
    laplace_hv_zero(&v1);
    laplace_hv_zero(&v2);
    v0.words[0] = 1u;
    v1.words[0] = 1u;
    v2.words[0] = 1u;

    const laplace_hv_t* ptrs[3] = { &v0, &v1, &v2 };
    laplace_hv_t result;
    laplace_hv_bundle(&result, ptrs, 3u, 0u);
    LAPLACE_TEST_ASSERT((result.words[0] & 1u) == 1u);

    LAPLACE_TEST_ASSERT((result.words[0] & 2u) == 0u);
    return 0;
}

static int test_hv_bundle_two_of_three(void) {
    laplace_hv_t v0, v1, v2, result;
    laplace_hv_zero(&v0);
    laplace_hv_zero(&v1);
    laplace_hv_zero(&v2);

    v0.words[0] |= UINT64_C(1) << 0;
    v1.words[0] |= UINT64_C(1) << 0;

    v1.words[0] |= UINT64_C(1) << 1;

    v0.words[0] |= UINT64_C(1) << 2;
    v1.words[0] |= UINT64_C(1) << 2;
    v2.words[0] |= UINT64_C(1) << 2;

    v2.words[0] |= UINT64_C(1) << 3;

    const laplace_hv_t* ptrs[3] = { &v0, &v1, &v2 };
    laplace_hv_bundle(&result, ptrs, 3u, 0u);

    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 0)) != 0u); /* bit 0: 1 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 1)) == 0u); /* bit 1: 0 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 2)) != 0u); /* bit 2: 1 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 3)) == 0u); /* bit 3: 0 */
    return 0;
}

static int test_hv_bundle_two_even_explicit(void) {
    laplace_hv_t a, b, result;
    laplace_hv_zero(&a);
    laplace_hv_zero(&b);

    a.words[0] |= UINT64_C(1) << 0;
    b.words[0] |= UINT64_C(1) << 0;

    a.words[0] |= UINT64_C(1) << 2;

    b.words[0] |= UINT64_C(1) << 3;

    const laplace_hv_t* ptrs[2] = { &a, &b };
    laplace_hv_bundle(&result, ptrs, 2u, 42u);

    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 0)) != 0u); /* both 1 */
    LAPLACE_TEST_ASSERT((result.words[0] & (UINT64_C(1) << 1)) == 0u); /* both 0 */

    laplace_hv_t result2;
    laplace_hv_bundle(&result2, ptrs, 2u, 42u);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&result, &result2));

    return 0;
}

static int test_hv_bind_distance_preservation(void) {
    laplace_hv_t a, b, k, ak, bk;
    laplace_hv_random(&a, 7000u);
    laplace_hv_random(&b, 7001u);
    laplace_hv_random(&k, 7002u);

    laplace_hv_bind(&ak, &a, &k);
    laplace_hv_bind(&bk, &b, &k);

    LAPLACE_TEST_ASSERT(laplace_hv_distance(&ak, &bk) == laplace_hv_distance(&a, &b));
    return 0;
}

static int test_hv_distance_popcount_relation(void) {
    laplace_hv_t a, zero;
    laplace_hv_random(&a, 8000u);
    laplace_hv_zero(&zero);

    LAPLACE_TEST_ASSERT(laplace_hv_distance(&a, &zero) == laplace_hv_popcount(&a));
    return 0;
}

static int test_hv_bind_inverse_multiseed(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t a, b, ab, recovered;
        laplace_hv_random(&a, seed * 2u);
        laplace_hv_random(&b, seed * 2u + 1u);
        laplace_hv_bind(&ab, &a, &b);
        laplace_hv_bind(&recovered, &ab, &b);
        LAPLACE_TEST_ASSERT(laplace_hv_equal(&recovered, &a));
    }
    return 0;
}

static int test_hv_triangle_inequality_multiseed(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t a, b, c;
        laplace_hv_random(&a, seed * 3u + 100u);
        laplace_hv_random(&b, seed * 3u + 101u);
        laplace_hv_random(&c, seed * 3u + 102u);

        const uint32_t dab = laplace_hv_distance(&a, &b);
        const uint32_t dbc = laplace_hv_distance(&b, &c);
        const uint32_t dac = laplace_hv_distance(&a, &c);
        LAPLACE_TEST_ASSERT(dac <= dab + dbc);
    }
    return 0;
}

static int test_hv_bundle_determinism_multiseed(void) {
    for (uint64_t seed = 0u; seed < 10u; ++seed) {
        laplace_hv_t vecs[5];
        const laplace_hv_t* ptrs[5];
        for (uint32_t i = 0u; i < 5u; ++i) {
            laplace_hv_random(&vecs[i], seed * 5u + (uint64_t)i + 200u);
            ptrs[i] = &vecs[i];
        }

        laplace_hv_t r1, r2;
        laplace_hv_bundle(&r1, ptrs, 5u, seed + 1000u);
        laplace_hv_bundle(&r2, ptrs, 5u, seed + 1000u);
        LAPLACE_TEST_ASSERT(laplace_hv_equal(&r1, &r2));
    }
    return 0;
}

int laplace_test_hv(void) {
    const laplace_test_case_t subtests[] = {
        {"hv_zero", test_hv_zero},
        {"hv_random_determinism", test_hv_random_determinism},
        {"hv_random_density", test_hv_random_density},
        {"hv_copy", test_hv_copy},
        {"hv_bind_self_inverse", test_hv_bind_self_inverse},
        {"hv_bind_commutativity", test_hv_bind_commutativity},
        {"hv_bind_associativity", test_hv_bind_associativity},
        {"hv_bind_with_zero", test_hv_bind_with_zero},
        {"hv_bind_with_self", test_hv_bind_with_self},
        {"hv_bind_in_place", test_hv_bind_in_place},
        {"hv_unbind_is_bind", test_hv_unbind_is_bind},
        {"hv_distance_identity", test_hv_distance_identity},
        {"hv_distance_complement", test_hv_distance_complement},
        {"hv_distance_symmetry", test_hv_distance_symmetry},
        {"hv_distance_triangle_inequality", test_hv_distance_triangle_inequality},
        {"hv_distance_zero_vectors", test_hv_distance_zero_vectors},
        {"hv_distance_random_near_half", test_hv_distance_random_near_half},
        {"hv_similarity", test_hv_similarity},
        {"hv_popcount", test_hv_popcount},
        {"hv_equal", test_hv_equal},
        {"hv_bundle_ref_vs_opt_k1", test_hv_bundle_ref_vs_opt_k1},
        {"hv_bundle_ref_vs_opt_k2", test_hv_bundle_ref_vs_opt_k2},
        {"hv_bundle_ref_vs_opt_k3", test_hv_bundle_ref_vs_opt_k3},
        {"hv_bundle_ref_vs_opt_k7", test_hv_bundle_ref_vs_opt_k7},
        {"hv_bundle_ref_vs_opt_k15", test_hv_bundle_ref_vs_opt_k15},
        {"hv_bundle_ref_vs_opt_k4_even", test_hv_bundle_ref_vs_opt_k4_even},
        {"hv_bundle_ref_vs_opt_k8_even", test_hv_bundle_ref_vs_opt_k8_even},
        {"hv_bundle_ref_vs_opt_identical", test_hv_bundle_ref_vs_opt_identical},
        {"hv_bundle_ref_vs_opt_zeros", test_hv_bundle_ref_vs_opt_zeros},
        {"hv_bundle_ref_vs_opt_ones", test_hv_bundle_ref_vs_opt_ones},
        {"hv_bundle_ref_vs_opt_mixed_edge", test_hv_bundle_ref_vs_opt_mixed_edge},
        {"hv_bundle_ref_vs_opt_multiseed", test_hv_bundle_ref_vs_opt_multiseed},
        {"hv_bundle_ref_vs_opt_k5_odd", test_hv_bundle_ref_vs_opt_k5_odd},
        {"hv_bundle_ref_vs_opt_k6_even", test_hv_bundle_ref_vs_opt_k6_even},
        {"hv_bundle_generic_vs_ref_sweep", test_hv_bundle_generic_vs_ref_sweep},
        {"hv_bundle_generic_vs_ref_even_ties", test_hv_bundle_generic_vs_ref_even_ties},
        {"hv_bundle2_identity", test_hv_bundle2_identity},
        {"hv_bundle4_explicit", test_hv_bundle4_explicit},
        {"hv_bundle_alternating", test_hv_bundle_alternating},
        {"hv_bundle2_direct_parity", test_hv_bundle2_direct_parity},
        {"hv_bundle3_direct_parity", test_hv_bundle3_direct_parity},
        {"hv_bundle_single", test_hv_bundle_single},
        {"hv_bundle_identical", test_hv_bundle_identical},
        {"hv_bundle_odd_majority", test_hv_bundle_odd_majority},
        {"hv_bundle_tie_break_determinism", test_hv_bundle_tie_break_determinism},
        {"hv_bundle_tie_break_seed_sensitivity", test_hv_bundle_tie_break_seed_sensitivity},
        {"hv_bundle_even_count_ties", test_hv_bundle_even_count_ties},
        {"hv_bundle_large_odd", test_hv_bundle_large_odd},
        {"hv_bundle_preserves_agreement", test_hv_bundle_preserves_agreement},
        {"hv_bundle_two_of_three", test_hv_bundle_two_of_three},
        {"hv_bundle_two_even_explicit", test_hv_bundle_two_even_explicit},
        {"hv_bind_distance_preservation", test_hv_bind_distance_preservation},
        {"hv_distance_popcount_relation", test_hv_distance_popcount_relation},
        {"hv_bind_inverse_multiseed", test_hv_bind_inverse_multiseed},
        {"hv_triangle_inequality_multiseed", test_hv_triangle_inequality_multiseed},
        {"hv_bundle_determinism_multiseed", test_hv_bundle_determinism_multiseed},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    for (size_t i = 0u; i < count; ++i) {
        const int rc = subtests[i].fn();
        if (rc != 0) {
            fprintf(stderr, "  subtest FAIL: %s\n", subtests[i].name);
            return 1;
        }
    }
    return 0;
}
