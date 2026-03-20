#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/assert.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/state.h"
#include "laplace/types.h"
#include "test_harness.h"

#define TEST_ENTITY_CAPACITY 16u
#define TEST_BACKING_SIZE    (TEST_ENTITY_CAPACITY * 64u)

static _Alignas(64) uint8_t g_backing[TEST_BACKING_SIZE];

static int test_entity_pool_init(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(pool.capacity == TEST_ENTITY_CAPACITY);
    LAPLACE_TEST_ASSERT(pool.free_count == TEST_ENTITY_CAPACITY);
    LAPLACE_TEST_ASSERT(pool.alive_count == 0u);

    for (uint32_t i = 0; i < TEST_ENTITY_CAPACITY; ++i) {
        LAPLACE_TEST_ASSERT(pool.generations[i] == 1u);
        LAPLACE_TEST_ASSERT(pool.states[i] == LAPLACE_STATE_FREE);
    }
    return 0;
}

#if !LAPLACE_DEBUG
static int test_entity_pool_init_invalid(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(NULL, &arena, 4u) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, NULL, 4u) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, 0u) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}
#endif

static int test_entity_alloc_free(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(h.id != LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(h.generation == 1u);
    LAPLACE_TEST_ASSERT(pool.alive_count == 1u);
    LAPLACE_TEST_ASSERT(pool.free_count == TEST_ENTITY_CAPACITY - 1u);

    LAPLACE_TEST_ASSERT(h.id == 1u);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_PENDING);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_is_alive(&pool, h));

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(pool.alive_count == 0u);
    LAPLACE_TEST_ASSERT(pool.free_count == TEST_ENTITY_CAPACITY);

    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&pool, h));

    return 0;
}

static int test_entity_generation_bump(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h1 = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(h1.generation == 1u);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h1) == LAPLACE_OK);

    laplace_entity_handle_t h2 = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(h2.id == h1.id);
    LAPLACE_TEST_ASSERT(h2.generation == 2u);

    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&pool, h1));
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h1) == LAPLACE_STATE_FREE);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h2) == LAPLACE_OK);
    return 0;
}

static int test_entity_stale_handle(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h = laplace_entity_pool_alloc(&pool);
    laplace_entity_handle_t stale = h;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, stale) == LAPLACE_ERR_GENERATION_MISMATCH);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, stale, LAPLACE_STATE_READY) == LAPLACE_ERR_GENERATION_MISMATCH);

    return 0;
}

static int test_entity_exhaustion(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t handles[TEST_ENTITY_CAPACITY];
    for (uint32_t i = 0; i < TEST_ENTITY_CAPACITY; ++i) {
        handles[i] = laplace_entity_pool_alloc(&pool);
        LAPLACE_TEST_ASSERT(handles[i].id != LAPLACE_ENTITY_ID_INVALID);
    }
    LAPLACE_TEST_ASSERT(pool.free_count == 0u);
    LAPLACE_TEST_ASSERT(pool.alive_count == TEST_ENTITY_CAPACITY);

    laplace_entity_handle_t fail = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(fail.id == LAPLACE_ENTITY_ID_INVALID);
    LAPLACE_TEST_ASSERT(fail.generation == LAPLACE_GENERATION_INVALID);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, handles[0]) == LAPLACE_OK);
    laplace_entity_handle_t recycled = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(recycled.id == handles[0].id);
    LAPLACE_TEST_ASSERT(recycled.generation == handles[0].generation + 1u);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, recycled) == LAPLACE_OK);
    for (uint32_t i = 1; i < TEST_ENTITY_CAPACITY; ++i) {
        LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, handles[i]) == LAPLACE_OK);
    }

    return 0;
}

static int test_entity_state_lifecycle(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_PENDING);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, h, LAPLACE_STATE_READY) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_READY);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, h, LAPLACE_STATE_ACTIVE) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_ACTIVE);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, h, LAPLACE_STATE_MASKED) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_MASKED);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, h, LAPLACE_STATE_ACTIVE) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, h) == LAPLACE_STATE_ACTIVE);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_set_state(&pool, h, LAPLACE_STATE_PENDING) == LAPLACE_ERR_INVALID_STATE);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h) == LAPLACE_OK);
    return 0;
}

static int test_entity_sequential_alloc(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    for (uint32_t i = 0; i < TEST_ENTITY_CAPACITY; ++i) {
        laplace_entity_handle_t h = laplace_entity_pool_alloc(&pool);
        LAPLACE_TEST_ASSERT(h.id == i + 1u);
    }

    return 0;
}

static int test_entity_pool_generation_query(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    for (uint32_t i = 0; i < TEST_ENTITY_CAPACITY; ++i) {
        LAPLACE_TEST_ASSERT(laplace_entity_pool_generation(&pool, i) == 1u);
    }

    LAPLACE_TEST_ASSERT(laplace_entity_pool_generation(&pool, TEST_ENTITY_CAPACITY) == LAPLACE_GENERATION_INVALID);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_generation(&pool, UINT32_MAX) == LAPLACE_GENERATION_INVALID);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_generation(NULL, 0u) == LAPLACE_GENERATION_INVALID);

    return 0;
}

static int test_entity_double_free(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t h = laplace_entity_pool_alloc(&pool);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, h) == LAPLACE_ERR_GENERATION_MISMATCH);

    return 0;
}

static int test_entity_invalid_handle(void) {
    laplace_arena_t arena;
    LAPLACE_TEST_ASSERT(laplace_arena_init(&arena, g_backing, sizeof(g_backing)) == LAPLACE_OK);

    laplace_entity_pool_t pool;
    LAPLACE_TEST_ASSERT(laplace_entity_pool_init(&pool, &arena, TEST_ENTITY_CAPACITY) == LAPLACE_OK);

    laplace_entity_handle_t bad_id = { LAPLACE_ENTITY_ID_INVALID, 1u };
    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&pool, bad_id));
    LAPLACE_TEST_ASSERT(laplace_entity_pool_get_state(&pool, bad_id) == LAPLACE_STATE_FREE);
    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, bad_id) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_entity_handle_t oob = { TEST_ENTITY_CAPACITY + 10u, 1u };
    LAPLACE_TEST_ASSERT(!laplace_entity_pool_is_alive(&pool, oob));
    LAPLACE_TEST_ASSERT(laplace_entity_pool_free(&pool, oob) == LAPLACE_ERR_OUT_OF_RANGE);

    return 0;
}

int laplace_test_entity(void) {
    const laplace_test_case_t subtests[] = {
        {"entity_pool_init", test_entity_pool_init},
#if !LAPLACE_DEBUG
        {"entity_pool_init_invalid", test_entity_pool_init_invalid},
#endif
        {"entity_alloc_free", test_entity_alloc_free},
        {"entity_generation_bump", test_entity_generation_bump},
        {"entity_stale_handle", test_entity_stale_handle},
        {"entity_exhaustion", test_entity_exhaustion},
        {"entity_state_lifecycle", test_entity_state_lifecycle},
        {"entity_sequential_alloc", test_entity_sequential_alloc},
        {"entity_pool_generation_query", test_entity_pool_generation_query},
        {"entity_double_free", test_entity_double_free},
        {"entity_invalid_handle", test_entity_invalid_handle},
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
