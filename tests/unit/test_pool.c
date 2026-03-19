#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/pool.h"
#include "laplace/types.h"
#include "test_harness.h"

/* Per entity overhead in the arena: gen(4) + state(4 aligned) + stack(4) = ~12 bytes.
 * 64 bytes per entity is generous with alignment padding. */
#define TEST_SHARD_CAPACITY 8u
#define TEST_SHARD_BUFSIZE  (TEST_SHARD_CAPACITY * 64u)

static _Alignas(64) uint8_t g_shard_buf[TEST_SHARD_BUFSIZE];

static int test_shard_pool_init(void) {
    laplace_shard_pool_t sp;
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 42u, g_shard_buf, sizeof(g_shard_buf), TEST_SHARD_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(sp.shard_id == 42u);
    LAPLACE_TEST_ASSERT(sp.entity_capacity == TEST_SHARD_CAPACITY);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(&sp) == 0u);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free_count(&sp) == TEST_SHARD_CAPACITY);
    return 0;
}

#if !LAPLACE_DEBUG
static int test_shard_pool_init_invalid(void) {
    laplace_shard_pool_t sp;
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(NULL, 0u, g_shard_buf, sizeof(g_shard_buf), 4u) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 0u, NULL, sizeof(g_shard_buf), 4u) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 0u, g_shard_buf, 0u, 4u) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 0u, g_shard_buf, sizeof(g_shard_buf), 0u) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}
#endif

static int test_shard_pool_buffer_too_small(void) {
    laplace_shard_pool_t sp;
    /* 4 bytes is far too small for even 1 entity */
    uint8_t tiny[4];
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 0u, tiny, sizeof(tiny), 1u) == LAPLACE_ERR_CAPACITY_EXHAUSTED);
    return 0;
}

static int test_shard_pool_alloc_free(void) {
    laplace_shard_pool_t sp;
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 1u, g_shard_buf, sizeof(g_shard_buf), TEST_SHARD_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h = laplace_shard_pool_alloc(&sp);
    LAPLACE_TEST_ASSERT(h.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(&sp) == 1u);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free_count(&sp) == TEST_SHARD_CAPACITY - 1u);

    LAPLACE_TEST_ASSERT(laplace_shard_pool_free(&sp, h) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(&sp) == 0u);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free_count(&sp) == TEST_SHARD_CAPACITY);

    return 0;
}

static int test_shard_pool_exhaustion(void) {
    laplace_shard_pool_t sp;
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 2u, g_shard_buf, sizeof(g_shard_buf), TEST_SHARD_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t handles[TEST_SHARD_CAPACITY];
    for (uint32_t i = 0; i < TEST_SHARD_CAPACITY; ++i) {
        handles[i] = laplace_shard_pool_alloc(&sp);
        LAPLACE_TEST_ASSERT(handles[i].id != LAPLACE_ENTITY_ID_INVALID);
    }

    /* Pool exhausted */
    laplace_entity_handle_t fail = laplace_shard_pool_alloc(&sp);
    LAPLACE_TEST_ASSERT(fail.id == LAPLACE_ENTITY_ID_INVALID);

    /* Free one, alloc again */
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free(&sp, handles[0]) == LAPLACE_OK);
    laplace_entity_handle_t recycled = laplace_shard_pool_alloc(&sp);
    LAPLACE_TEST_ASSERT(recycled.id == handles[0].id);
    LAPLACE_TEST_ASSERT(recycled.generation > handles[0].generation);

    return 0;
}

static int test_shard_pool_reset(void) {
    laplace_shard_pool_t sp;
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 3u, g_shard_buf, sizeof(g_shard_buf), TEST_SHARD_CAPACITY) == LAPLACE_OK);

    /* Alloc a couple entities */
    (void)laplace_shard_pool_alloc(&sp);
    (void)laplace_shard_pool_alloc(&sp);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(&sp) == 2u);

    /* Reset invalidates everything */
    laplace_shard_pool_reset(&sp);
    LAPLACE_TEST_ASSERT(sp.entity_capacity == 0u);

    /* Must re-init after reset */
    LAPLACE_TEST_ASSERT(laplace_shard_pool_init(&sp, 3u, g_shard_buf, sizeof(g_shard_buf), TEST_SHARD_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(&sp) == 0u);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free_count(&sp) == TEST_SHARD_CAPACITY);

    return 0;
}

#if !LAPLACE_DEBUG
static int test_shard_pool_null_queries(void) {
    LAPLACE_TEST_ASSERT(laplace_shard_pool_alive_count(NULL) == 0u);
    LAPLACE_TEST_ASSERT(laplace_shard_pool_free_count(NULL) == 0u);
    return 0;
}
#endif

int laplace_test_pool(void) {
    const laplace_test_case_t subtests[] = {
        {"shard_pool_init", test_shard_pool_init},
#if !LAPLACE_DEBUG
        {"shard_pool_init_invalid", test_shard_pool_init_invalid},
#endif
        {"shard_pool_buffer_too_small", test_shard_pool_buffer_too_small},
        {"shard_pool_alloc_free", test_shard_pool_alloc_free},
        {"shard_pool_exhaustion", test_shard_pool_exhaustion},
        {"shard_pool_reset", test_shard_pool_reset},
#if !LAPLACE_DEBUG
        {"shard_pool_null_queries", test_shard_pool_null_queries},
#endif
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
