#include <stddef.h>
#include <stdint.h>

#include "laplace/bitset.h"
#include "test_harness.h"

static int test_bitset_init(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 100, 2);

    LAPLACE_TEST_ASSERT(bs.bit_count == 100u);
    LAPLACE_TEST_ASSERT(bs.word_count == 2u);
    LAPLACE_TEST_ASSERT(laplace_bitset_none(&bs));
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 0u);

    return 0;
}

static int test_bitset_single_bit(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 128, 2);

    laplace_bitset_set(&bs, 0u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 0u));
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&bs, 1u));

    laplace_bitset_set(&bs, 63u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 63u));

    laplace_bitset_set(&bs, 64u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 64u));

    laplace_bitset_set(&bs, 127u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 127u));

    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 4u);

    laplace_bitset_clear(&bs, 63u);
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&bs, 63u));
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 3u);

    return 0;
}

static int test_bitset_toggle(void) {
    uint64_t words[1];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 64, 1);

    laplace_bitset_toggle(&bs, 10u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 10u));

    laplace_bitset_toggle(&bs, 10u);
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&bs, 10u));

    return 0;
}

static int test_bitset_set_clear_all(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 100, 2);

    laplace_bitset_set_all(&bs);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 100u);
    LAPLACE_TEST_ASSERT(laplace_bitset_any(&bs));

    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 99u));

    laplace_bitset_clear_all(&bs);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 0u);
    LAPLACE_TEST_ASSERT(laplace_bitset_none(&bs));

    return 0;
}

static int test_bitset_set_all_exact_boundary(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 128, 2);

    laplace_bitset_set_all(&bs);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 128u);

    return 0;
}

static int test_bitset_logical_ops(void) {
    uint64_t wa[2], wb[2], wd[2];
    laplace_bitset_t a, b, dst;
    laplace_bitset_init(&a, wa, 128, 2);
    laplace_bitset_init(&b, wb, 128, 2);
    laplace_bitset_init(&dst, wd, 128, 2);

    laplace_bitset_set(&a, 0u);
    laplace_bitset_set(&a, 10u);
    laplace_bitset_set(&a, 64u);

    laplace_bitset_set(&b, 10u);
    laplace_bitset_set(&b, 64u);
    laplace_bitset_set(&b, 100u);

    laplace_bitset_and(&dst, &a, &b);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&dst) == 2u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 10u));
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 64u));
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&dst, 0u));
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&dst, 100u));

    laplace_bitset_or(&dst, &a, &b);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&dst) == 4u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 0u));
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 100u));

    laplace_bitset_xor(&dst, &a, &b);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&dst) == 2u);
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 0u));
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&dst, 100u));
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&dst, 10u));

    return 0;
}

static int test_bitset_not(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 100, 2);

    laplace_bitset_set(&bs, 0u);
    laplace_bitset_set(&bs, 99u);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 2u);

    laplace_bitset_not(&bs);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 98u);
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&bs, 0u));
    LAPLACE_TEST_ASSERT(!laplace_bitset_test(&bs, 99u));
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 1u));
    LAPLACE_TEST_ASSERT(laplace_bitset_test(&bs, 50u));

    return 0;
}

static int test_bitset_iteration(void) {
    uint64_t words[4];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 256, 4);

    laplace_bitset_set(&bs, 3u);
    laplace_bitset_set(&bs, 63u);
    laplace_bitset_set(&bs, 64u);
    laplace_bitset_set(&bs, 128u);
    laplace_bitset_set(&bs, 255u);

    uint32_t idx = laplace_bitset_find_first_set(&bs);
    LAPLACE_TEST_ASSERT(idx == 3u);

    idx = laplace_bitset_find_next_set(&bs, idx);
    LAPLACE_TEST_ASSERT(idx == 63u);

    idx = laplace_bitset_find_next_set(&bs, idx);
    LAPLACE_TEST_ASSERT(idx == 64u);

    idx = laplace_bitset_find_next_set(&bs, idx);
    LAPLACE_TEST_ASSERT(idx == 128u);

    idx = laplace_bitset_find_next_set(&bs, idx);
    LAPLACE_TEST_ASSERT(idx == 255u);

    idx = laplace_bitset_find_next_set(&bs, idx);
    LAPLACE_TEST_ASSERT(idx == UINT32_MAX);

    return 0;
}

static int test_bitset_find_empty(void) {
    uint64_t words[2];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 128, 2);

    LAPLACE_TEST_ASSERT(laplace_bitset_find_first_set(&bs) == UINT32_MAX);

    return 0;
}

static int test_bitset_popcount_full(void) {
    uint64_t words[1];
    laplace_bitset_t bs;
    laplace_bitset_init(&bs, words, 64, 1);

    laplace_bitset_set_all(&bs);
    LAPLACE_TEST_ASSERT(laplace_bitset_popcount(&bs) == 64u);

    return 0;
}

int laplace_test_bitset(void) {
    const laplace_test_case_t subtests[] = {
        {"bitset_init", test_bitset_init},
        {"bitset_single_bit", test_bitset_single_bit},
        {"bitset_toggle", test_bitset_toggle},
        {"bitset_set_clear_all", test_bitset_set_clear_all},
        {"bitset_set_all_exact_boundary", test_bitset_set_all_exact_boundary},
        {"bitset_logical_ops", test_bitset_logical_ops},
        {"bitset_not", test_bitset_not},
        {"bitset_iteration", test_bitset_iteration},
        {"bitset_find_empty", test_bitset_find_empty},
        {"bitset_popcount_full", test_bitset_popcount_full},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    for (size_t i = 0; i < count; ++i) {
        const int rc = subtests[i].fn();
        if (rc != 0) {
            fprintf(stderr, "  subtest FAIL: %s\n", subtests[i].name);
            return 1;
        }
    }
    return 0;
}
