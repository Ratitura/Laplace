
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/hv.h"
#include "test_harness.h"

extern void     laplace__hv_bind_words_scalar(uint64_t* restrict dst,
                                               const uint64_t* a,
                                               const uint64_t* b,
                                               uint32_t num_words);
extern uint32_t laplace__hv_xor_popcount_words_scalar(const uint64_t* a,
                                                       const uint64_t* b,
                                                       uint32_t num_words);
extern uint32_t laplace__hv_popcount_words_scalar(const uint64_t* words,
                                                   uint32_t num_words);

static void hv_fill_ones_parity(laplace_hv_t* dst) {
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        dst->words[i] = UINT64_MAX;
    }
}

static void hv_fill_alternating(laplace_hv_t* dst, uint64_t pattern) {
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        dst->words[i] = pattern;
    }
}

static int test_parity_bind_random(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t a, b;
        laplace_hv_random(&a, seed * 1000u + 100u);
        laplace_hv_random(&b, seed * 1000u + 200u);

        laplace_hv_t scalar_result;
        laplace__hv_bind_words_scalar(scalar_result.words,
                                       a.words, b.words, LAPLACE_HV_WORDS);

        laplace_hv_t backend_result;
        laplace_hv_bind(&backend_result, &a, &b);

        LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    }
    return 0;
}

static int test_parity_bind_zeros(void) {
    laplace_hv_t a, b;
    laplace_hv_zero(&a);
    laplace_hv_zero(&b);

    laplace_hv_t scalar_result;
    laplace__hv_bind_words_scalar(scalar_result.words,
                                   a.words, b.words, LAPLACE_HV_WORDS);

    laplace_hv_t backend_result;
    laplace_hv_bind(&backend_result, &a, &b);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&backend_result) == 0u);
    return 0;
}

static int test_parity_bind_ones(void) {
    laplace_hv_t a, b;
    hv_fill_ones_parity(&a);
    hv_fill_ones_parity(&b);

    laplace_hv_t scalar_result;
    laplace__hv_bind_words_scalar(scalar_result.words,
                                   a.words, b.words, LAPLACE_HV_WORDS);

    laplace_hv_t backend_result;
    laplace_hv_bind(&backend_result, &a, &b);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&backend_result) == 0u);
    return 0;
}

static int test_parity_bind_complement(void) {
    laplace_hv_t a, b;
    laplace_hv_zero(&a);
    hv_fill_ones_parity(&b);

    laplace_hv_t scalar_result;
    laplace__hv_bind_words_scalar(scalar_result.words,
                                   a.words, b.words, LAPLACE_HV_WORDS);

    laplace_hv_t backend_result;
    laplace_hv_bind(&backend_result, &a, &b);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&backend_result) == LAPLACE_HV_DIM);
    return 0;
}

static int test_parity_bind_alternating(void) {
    laplace_hv_t a, b;
    hv_fill_alternating(&a, UINT64_C(0xAAAAAAAAAAAAAAAA));
    hv_fill_alternating(&b, UINT64_C(0x5555555555555555));

    laplace_hv_t scalar_result;
    laplace__hv_bind_words_scalar(scalar_result.words,
                                   a.words, b.words, LAPLACE_HV_WORDS);

    laplace_hv_t backend_result;
    laplace_hv_bind(&backend_result, &a, &b);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&backend_result) == LAPLACE_HV_DIM);
    return 0;
}

static int test_parity_bind_self(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 7777u);

    laplace_hv_t scalar_result;
    laplace__hv_bind_words_scalar(scalar_result.words,
                                   a.words, a.words, LAPLACE_HV_WORDS);

    laplace_hv_t backend_result;
    laplace_hv_bind(&backend_result, &a, &a);

    LAPLACE_TEST_ASSERT(laplace_hv_equal(&scalar_result, &backend_result));
    LAPLACE_TEST_ASSERT(laplace_hv_popcount(&backend_result) == 0u);
    return 0;
}

static int test_parity_distance_random(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t a, b;
        laplace_hv_random(&a, seed * 2000u + 100u);
        laplace_hv_random(&b, seed * 2000u + 200u);

        const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
            a.words, b.words, LAPLACE_HV_WORDS);
        const uint32_t backend_dist = laplace_hv_distance(&a, &b);

        LAPLACE_TEST_ASSERT(scalar_dist == backend_dist);
    }
    return 0;
}

static int test_parity_distance_identical(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 3333u);

    const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
        a.words, a.words, LAPLACE_HV_WORDS);
    const uint32_t backend_dist = laplace_hv_distance(&a, &a);

    LAPLACE_TEST_ASSERT(scalar_dist == 0u);
    LAPLACE_TEST_ASSERT(backend_dist == 0u);
    return 0;
}

static int test_parity_distance_complement(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 4444u);
    laplace_hv_copy(&b, &a);
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        b.words[i] = ~b.words[i];
    }

    const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
        a.words, b.words, LAPLACE_HV_WORDS);
    const uint32_t backend_dist = laplace_hv_distance(&a, &b);

    LAPLACE_TEST_ASSERT(scalar_dist == LAPLACE_HV_DIM);
    LAPLACE_TEST_ASSERT(backend_dist == LAPLACE_HV_DIM);
    return 0;
}

static int test_parity_distance_zeros(void) {
    laplace_hv_t a, b;
    laplace_hv_zero(&a);
    laplace_hv_zero(&b);

    const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
        a.words, b.words, LAPLACE_HV_WORDS);
    const uint32_t backend_dist = laplace_hv_distance(&a, &b);

    LAPLACE_TEST_ASSERT(scalar_dist == 0u);
    LAPLACE_TEST_ASSERT(backend_dist == 0u);
    return 0;
}

static int test_parity_distance_single_bit(void) {
    laplace_hv_t a, b;
    laplace_hv_zero(&a);
    laplace_hv_zero(&b);
    b.words[0] = 1u;

    const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
        a.words, b.words, LAPLACE_HV_WORDS);
    const uint32_t backend_dist = laplace_hv_distance(&a, &b);

    LAPLACE_TEST_ASSERT(scalar_dist == 1u);
    LAPLACE_TEST_ASSERT(backend_dist == 1u);
    return 0;
}

static int test_parity_popcount_random(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t hv;
        laplace_hv_random(&hv, seed * 3000u + 500u);

        const uint32_t scalar_pc = laplace__hv_popcount_words_scalar(
            hv.words, LAPLACE_HV_WORDS);
        const uint32_t backend_pc = laplace_hv_popcount(&hv);

        LAPLACE_TEST_ASSERT(scalar_pc == backend_pc);
    }
    return 0;
}

static int test_parity_popcount_zeros(void) {
    laplace_hv_t hv;
    laplace_hv_zero(&hv);

    const uint32_t scalar_pc = laplace__hv_popcount_words_scalar(
        hv.words, LAPLACE_HV_WORDS);
    const uint32_t backend_pc = laplace_hv_popcount(&hv);

    LAPLACE_TEST_ASSERT(scalar_pc == 0u);
    LAPLACE_TEST_ASSERT(backend_pc == 0u);
    return 0;
}

static int test_parity_popcount_ones(void) {
    laplace_hv_t hv;
    hv_fill_ones_parity(&hv);

    const uint32_t scalar_pc = laplace__hv_popcount_words_scalar(
        hv.words, LAPLACE_HV_WORDS);
    const uint32_t backend_pc = laplace_hv_popcount(&hv);

    LAPLACE_TEST_ASSERT(scalar_pc == LAPLACE_HV_DIM);
    LAPLACE_TEST_ASSERT(backend_pc == LAPLACE_HV_DIM);
    return 0;
}

static int test_parity_popcount_single_bit(void) {
    laplace_hv_t hv;
    laplace_hv_zero(&hv);
    hv.words[LAPLACE_HV_WORDS / 2u] = UINT64_C(1) << 31;

    const uint32_t scalar_pc = laplace__hv_popcount_words_scalar(
        hv.words, LAPLACE_HV_WORDS);
    const uint32_t backend_pc = laplace_hv_popcount(&hv);

    LAPLACE_TEST_ASSERT(scalar_pc == 1u);
    LAPLACE_TEST_ASSERT(backend_pc == 1u);
    return 0;
}

static int test_parity_popcount_alternating(void) {
    laplace_hv_t hv;
    hv_fill_alternating(&hv, UINT64_C(0xAAAAAAAAAAAAAAAA));

    const uint32_t scalar_pc = laplace__hv_popcount_words_scalar(
        hv.words, LAPLACE_HV_WORDS);
    const uint32_t backend_pc = laplace_hv_popcount(&hv);

    LAPLACE_TEST_ASSERT(scalar_pc == LAPLACE_HV_DIM / 2u);
    LAPLACE_TEST_ASSERT(backend_pc == LAPLACE_HV_DIM / 2u);
    return 0;
}

static int test_parity_similarity_random(void) {
    for (uint64_t seed = 0u; seed < 20u; ++seed) {
        laplace_hv_t a, b;
        laplace_hv_random(&a, seed * 4000u + 100u);
        laplace_hv_random(&b, seed * 4000u + 200u);

        const uint32_t scalar_dist = laplace__hv_xor_popcount_words_scalar(
            a.words, b.words, LAPLACE_HV_WORDS);
        const double scalar_sim = 1.0 - ((double)scalar_dist / (double)LAPLACE_HV_DIM);

        const double backend_sim = laplace_hv_similarity(&a, &b);

        LAPLACE_TEST_ASSERT(scalar_sim == backend_sim);
    }
    return 0;
}

static int test_parity_similarity_identical(void) {
    laplace_hv_t a;
    laplace_hv_random(&a, 5555u);

    const double sim = laplace_hv_similarity(&a, &a);
    LAPLACE_TEST_ASSERT(sim == 1.0);
    return 0;
}

static int test_parity_similarity_complement(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 6666u);
    laplace_hv_copy(&b, &a);
    for (uint32_t i = 0u; i < LAPLACE_HV_WORDS; ++i) {
        b.words[i] = ~b.words[i];
    }

    const double sim = laplace_hv_similarity(&a, &b);
    LAPLACE_TEST_ASSERT(sim == 0.0);
    return 0;
}

static int test_parity_determinism_bind(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 8000u);
    laplace_hv_random(&b, 8001u);

    laplace_hv_t r1, r2;
    laplace_hv_bind(&r1, &a, &b);
    laplace_hv_bind(&r2, &a, &b);
    LAPLACE_TEST_ASSERT(laplace_hv_equal(&r1, &r2));
    return 0;
}

static int test_parity_determinism_distance(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 8100u);
    laplace_hv_random(&b, 8101u);

    const uint32_t d1 = laplace_hv_distance(&a, &b);
    const uint32_t d2 = laplace_hv_distance(&a, &b);
    LAPLACE_TEST_ASSERT(d1 == d2);
    return 0;
}

static int test_parity_determinism_popcount(void) {
    laplace_hv_t hv;
    laplace_hv_random(&hv, 8200u);

    const uint32_t p1 = laplace_hv_popcount(&hv);
    const uint32_t p2 = laplace_hv_popcount(&hv);
    LAPLACE_TEST_ASSERT(p1 == p2);
    return 0;
}

static int test_parity_determinism_similarity(void) {
    laplace_hv_t a, b;
    laplace_hv_random(&a, 8300u);
    laplace_hv_random(&b, 8301u);

    const double s1 = laplace_hv_similarity(&a, &b);
    const double s2 = laplace_hv_similarity(&a, &b);
    LAPLACE_TEST_ASSERT(s1 == s2);
    return 0;
}

int laplace_test_hv_parity(void) {
    const laplace_test_case_t subtests[] = {
        {"parity_bind_random",       test_parity_bind_random},
        {"parity_bind_zeros",        test_parity_bind_zeros},
        {"parity_bind_ones",         test_parity_bind_ones},
        {"parity_bind_complement",   test_parity_bind_complement},
        {"parity_bind_alternating",  test_parity_bind_alternating},
        {"parity_bind_self",         test_parity_bind_self},
        {"parity_distance_random",     test_parity_distance_random},
        {"parity_distance_identical",  test_parity_distance_identical},
        {"parity_distance_complement", test_parity_distance_complement},
        {"parity_distance_zeros",      test_parity_distance_zeros},
        {"parity_distance_single_bit", test_parity_distance_single_bit},
        {"parity_popcount_random",      test_parity_popcount_random},
        {"parity_popcount_zeros",       test_parity_popcount_zeros},
        {"parity_popcount_ones",        test_parity_popcount_ones},
        {"parity_popcount_single_bit",  test_parity_popcount_single_bit},
        {"parity_popcount_alternating", test_parity_popcount_alternating},
        {"parity_similarity_random",     test_parity_similarity_random},
        {"parity_similarity_identical",  test_parity_similarity_identical},
        {"parity_similarity_complement", test_parity_similarity_complement},
        {"parity_determinism_bind",       test_parity_determinism_bind},
        {"parity_determinism_distance",   test_parity_determinism_distance},
        {"parity_determinism_popcount",   test_parity_determinism_popcount},
        {"parity_determinism_similarity", test_parity_determinism_similarity},
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
