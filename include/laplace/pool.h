#ifndef LAPLACE_POOL_H
#define LAPLACE_POOL_H

#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_shard_pool {
    laplace_arena_t       arena;          /* shard-local arena for all pool allocations */
    laplace_entity_pool_t entity_pool;    /* SoA entity storage backed by the arena */
    uint32_t              shard_id;       /* identifier of the owning shard */
    uint32_t              entity_capacity;/* max entities this shard can hold */
} laplace_shard_pool_t;

laplace_error_t laplace_shard_pool_init(laplace_shard_pool_t* pool,
                                         uint32_t shard_id,
                                         void* backing_buffer,
                                         size_t buffer_size,
                                         uint32_t entity_capacity);

laplace_entity_handle_t laplace_shard_pool_alloc(laplace_shard_pool_t* pool);

laplace_error_t laplace_shard_pool_free(laplace_shard_pool_t* pool, laplace_entity_handle_t handle);

uint32_t laplace_shard_pool_alive_count(const laplace_shard_pool_t* pool);

uint32_t laplace_shard_pool_free_count(const laplace_shard_pool_t* pool);

void laplace_shard_pool_reset(laplace_shard_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif
