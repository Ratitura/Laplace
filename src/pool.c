#include "laplace/pool.h"

#include <string.h>

laplace_error_t laplace_shard_pool_init(laplace_shard_pool_t* const pool,
                                         const uint32_t shard_id,
                                         void* const backing_buffer,
                                         const size_t buffer_size,
                                         const uint32_t entity_capacity) {
    LAPLACE_ASSERT(pool != NULL);
    LAPLACE_ASSERT(backing_buffer != NULL);
    LAPLACE_ASSERT(buffer_size > 0u);
    LAPLACE_ASSERT(entity_capacity > 0u);

    if (pool == NULL || backing_buffer == NULL || buffer_size == 0u || entity_capacity == 0u) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(pool, 0, sizeof(*pool));

    pool->shard_id = shard_id;
    pool->entity_capacity = entity_capacity;

    laplace_error_t rc = laplace_arena_init(&pool->arena, backing_buffer, buffer_size);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    rc = laplace_entity_pool_init(&pool->entity_pool, &pool->arena, entity_capacity);
    if (rc != LAPLACE_OK) {
        return rc;
    }

    return LAPLACE_OK;
}

laplace_entity_handle_t laplace_shard_pool_alloc(laplace_shard_pool_t* const pool) {
    LAPLACE_ASSERT(pool != NULL);

    laplace_entity_handle_t handle = { LAPLACE_ENTITY_ID_INVALID, LAPLACE_GENERATION_INVALID };
    if (pool == NULL) {
        return handle;
    }

    return laplace_entity_pool_alloc(&pool->entity_pool);
}

laplace_error_t laplace_shard_pool_free(laplace_shard_pool_t* const pool,
                                         const laplace_entity_handle_t handle) {
    LAPLACE_ASSERT(pool != NULL);
    if (pool == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    return laplace_entity_pool_free(&pool->entity_pool, handle);
}

uint32_t laplace_shard_pool_alive_count(const laplace_shard_pool_t* const pool) {
    LAPLACE_ASSERT(pool != NULL);
    if (pool == NULL) {
        return 0u;
    }
    return pool->entity_pool.alive_count;
}

uint32_t laplace_shard_pool_free_count(const laplace_shard_pool_t* const pool) {
    LAPLACE_ASSERT(pool != NULL);
    if (pool == NULL) {
        return 0u;
    }
    return pool->entity_pool.free_count;
}

void laplace_shard_pool_reset(laplace_shard_pool_t* const pool) {
    LAPLACE_ASSERT(pool != NULL);
    if (pool == NULL) {
        return;
    }
    laplace_arena_reset(&pool->arena);
    memset(&pool->entity_pool, 0, sizeof(pool->entity_pool));
    pool->entity_capacity = 0u;
}
