#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/arena.h"
#include "laplace/assert.h"
#include "test_harness.h"

static int test_arena_init_basic(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[1024];
    laplace_arena_t arena;

    const laplace_error_t rc = laplace_arena_init(&arena, buf, sizeof(buf));
    LAPLACE_TEST_ASSERT(rc == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_arena_used(&arena) == 0u);
    LAPLACE_TEST_ASSERT(laplace_arena_remaining(&arena) == sizeof(buf));
    LAPLACE_TEST_ASSERT(laplace_arena_peak(&arena) == 0u);
    return 0;
}

#if !LAPLACE_DEBUG
static int test_arena_init_null(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(NULL, (void*)1, 1024) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, NULL, 1024) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, (void*)1, 0) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}
#endif

static int test_arena_alloc_aligned(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[4096];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    void* p1 = laplace_arena_alloc(&arena, 7, 1);
    LAPLACE_TEST_ASSERT(p1 != NULL);

    void* p16 = laplace_arena_alloc(&arena, 32, 16);
    LAPLACE_TEST_ASSERT(p16 != NULL);
    LAPLACE_TEST_ASSERT(((uintptr_t)p16 & 15u) == 0u);

    void* p64 = laplace_arena_alloc(&arena, 64, 64);
    LAPLACE_TEST_ASSERT(p64 != NULL);
    LAPLACE_TEST_ASSERT(((uintptr_t)p64 & 63u) == 0u);

    /* All pointers within buffer bounds */
    LAPLACE_TEST_ASSERT((uint8_t*)p1 >= buf && (uint8_t*)p1 < buf + sizeof(buf));
    LAPLACE_TEST_ASSERT((uint8_t*)p16 >= buf && (uint8_t*)p16 < buf + sizeof(buf));
    LAPLACE_TEST_ASSERT((uint8_t*)p64 >= buf && (uint8_t*)p64 < buf + sizeof(buf));

    return 0;
}

static int test_arena_exhaustion(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[128];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    void* p1 = laplace_arena_alloc(&arena, 64, 1);
    LAPLACE_TEST_ASSERT(p1 != NULL);

    void* p2 = laplace_arena_alloc(&arena, 64, 1);
    LAPLACE_TEST_ASSERT(p2 != NULL);

    /* Arena should now be full */
    void* p3 = laplace_arena_alloc(&arena, 1, 1);
    LAPLACE_TEST_ASSERT(p3 == NULL);

    return 0;
}

static int test_arena_alloc_zero_size(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[256];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    void* p = laplace_arena_alloc(&arena, 0, 1);
    LAPLACE_TEST_ASSERT(p == NULL);
    LAPLACE_TEST_ASSERT(laplace_arena_used(&arena) == 0u);

    return 0;
}

static int test_arena_reset(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[512];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    laplace_arena_alloc(&arena, 256, 1);
    LAPLACE_TEST_ASSERT(laplace_arena_used(&arena) >= 256u);

    const size_t peak_before = laplace_arena_peak(&arena);
    laplace_arena_reset(&arena);
    LAPLACE_TEST_ASSERT(laplace_arena_used(&arena) == 0u);
    LAPLACE_TEST_ASSERT(laplace_arena_remaining(&arena) == sizeof(buf));
    /* Peak is preserved across reset */
    LAPLACE_TEST_ASSERT(laplace_arena_peak(&arena) == peak_before);

    /* Can allocate again after reset */
    void* p = laplace_arena_alloc(&arena, 128, 1);
    LAPLACE_TEST_ASSERT(p != NULL);

    return 0;
}

static int test_arena_peak_tracking(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[1024];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    laplace_arena_alloc(&arena, 100, 1);
    const size_t peak1 = laplace_arena_peak(&arena);
    LAPLACE_TEST_ASSERT(peak1 >= 100u);

    laplace_arena_reset(&arena);
    laplace_arena_alloc(&arena, 50, 1);
    /* Peak should still be >= 100 (high water mark) */
    LAPLACE_TEST_ASSERT(laplace_arena_peak(&arena) == peak1);

    laplace_arena_reset_peak(&arena);
    LAPLACE_TEST_ASSERT(laplace_arena_peak(&arena) == laplace_arena_used(&arena));

    return 0;
}

static int test_arena_sequential(void) {
    LAPLACE_ALIGNAS(64) uint8_t buf[4096];
    laplace_arena_t arena;
    laplace_arena_init(&arena, buf, sizeof(buf));

    void* ptrs[32];
    for (int i = 0; i < 32; ++i) {
        ptrs[i] = laplace_arena_alloc(&arena, 64, 8);
        LAPLACE_TEST_ASSERT(ptrs[i] != NULL);
        LAPLACE_TEST_ASSERT(((uintptr_t)ptrs[i] & 7u) == 0u);
    }

    /* All distinct and non-overlapping */
    for (int i = 1; i < 32; ++i) {
        LAPLACE_TEST_ASSERT((uint8_t*)ptrs[i] >= (uint8_t*)ptrs[i - 1] + 64);
    }

    return 0;
}

int laplace_test_arena(void) {
    const laplace_test_case_t subtests[] = {
        {"arena_init_basic", test_arena_init_basic},
#if !LAPLACE_DEBUG
        {"arena_init_null", test_arena_init_null},
#endif
        {"arena_alloc_aligned", test_arena_alloc_aligned},
        {"arena_exhaustion", test_arena_exhaustion},
        {"arena_alloc_zero_size", test_arena_alloc_zero_size},
        {"arena_reset", test_arena_reset},
        {"arena_peak_tracking", test_arena_peak_tracking},
        {"arena_sequential", test_arena_sequential},
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
