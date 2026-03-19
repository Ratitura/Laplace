#ifndef LAPLACE_POOL_H
#define LAPLACE_POOL_H

#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shard-local pool: combines a dedicated arena with an entity pool
 * to represent a self-contained, shard-owned unit of entity storage.
 *
 * Design:
 *  - Each shard owns one laplace_shard_pool_t.
 *  - The shard provides the backing buffer at init time (no internal malloc).
 *  - The arena is used exclusively by this pool's entity allocations.
 *  - shard_id identifies the owning shard for cross-shard reference checks.
 *  - No thread safety: one shard = one owner = no contention.
 */
typedef struct laplace_shard_pool {
    laplace_arena_t       arena;          /* shard-local arena for all pool allocations */
    laplace_entity_pool_t entity_pool;    /* SoA entity storage backed by the arena */
    uint32_t              shard_id;       /* identifier of the owning shard */
    uint32_t              entity_capacity;/* max entities this shard can hold */
} laplace_shard_pool_t;

/*
 * Initialize a shard pool over a caller-provided backing buffer.
 *
 * Parameters:
 *   pool            - shard pool to initialize
 *   shard_id        - unique shard identifier
 *   backing_buffer  - caller-owned memory for the arena
 *   buffer_size     - size of backing_buffer in bytes
 *   entity_capacity - max number of entities for this shard
 *
 * Returns LAPLACE_OK on success.
 * Returns LAPLACE_ERR_CAPACITY_EXHAUSTED if buffer is too small for entity_capacity.
 */
laplace_error_t laplace_shard_pool_init(laplace_shard_pool_t* pool,
                                         uint32_t shard_id,
                                         void* backing_buffer,
                                         size_t buffer_size,
                                         uint32_t entity_capacity);

/*
 * Allocate an entity from this shard's pool.
 * Returns a handle; handle.id == LAPLACE_ENTITY_ID_INVALID on failure.
 */
laplace_entity_handle_t laplace_shard_pool_alloc(laplace_shard_pool_t* pool);

/*
 * Free an entity back into this shard's pool.
 */
laplace_error_t laplace_shard_pool_free(laplace_shard_pool_t* pool, laplace_entity_handle_t handle);

/*
 * Query how many entities are currently alive in this shard.
 */
uint32_t laplace_shard_pool_alive_count(const laplace_shard_pool_t* pool);

/*
 * Query how many free slots remain in this shard.
 */
uint32_t laplace_shard_pool_free_count(const laplace_shard_pool_t* pool);

/*
 * Reset the entire shard pool.
 * All entities are invalidated. Arena is reset.
 * The pool must be re-initialized after reset via laplace_shard_pool_init.
 */
void laplace_shard_pool_reset(laplace_shard_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif
