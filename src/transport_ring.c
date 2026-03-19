#include "laplace/transport.h"

#include <string.h>

static inline void transport_fence_release(void) {
#if defined(__clang__) || defined(__GNUC__)
    __atomic_thread_fence(__ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    MemoryBarrier();
#else
    #error "Unsupported compiler for memory fences"
#endif
}

static inline void transport_fence_acquire(void) {
#if defined(__clang__) || defined(__GNUC__)
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    MemoryBarrier();
    _ReadWriteBarrier();
#else
    #error "Unsupported compiler for memory fences"
#endif
}

laplace_error_t laplace_transport_ingress_enqueue(laplace_transport_mapping_t* mapping,
                                                   const laplace_transport_command_record_t* record) {
    if (mapping == NULL || mapping->view == NULL || record == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(mapping);

    if (laplace_transport_ring_is_full(ring, LAPLACE_TRANSPORT_INGRESS_CAPACITY)) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t slot_index = ring->head & (LAPLACE_TRANSPORT_INGRESS_CAPACITY - 1u);
    laplace_transport_command_record_t* slots = laplace_transport_get_ingress_slots(mapping);
    memcpy(&slots[slot_index], record, sizeof(laplace_transport_command_record_t));

    transport_fence_release();

    ring->head += 1u;
    return LAPLACE_OK;
}

laplace_error_t laplace_transport_ingress_dequeue(laplace_transport_mapping_t* mapping,
                                                   laplace_transport_command_record_t* out_record) {
    if (mapping == NULL || mapping->view == NULL || out_record == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_transport_ring_header_t* ring = laplace_transport_get_ingress_ring(mapping);

    transport_fence_acquire();

    if (laplace_transport_ring_is_empty(ring)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const uint32_t slot_index = ring->tail & (LAPLACE_TRANSPORT_INGRESS_CAPACITY - 1u);
    const laplace_transport_command_record_t* slots = laplace_transport_get_ingress_slots(mapping);
    memcpy(out_record, &slots[slot_index], sizeof(laplace_transport_command_record_t));

    transport_fence_release();

    ring->tail += 1u;
    return LAPLACE_OK;
}

laplace_error_t laplace_transport_egress_enqueue(laplace_transport_mapping_t* mapping,
                                                  const laplace_transport_event_record_t* record) {
    if (mapping == NULL || mapping->view == NULL || record == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_transport_ring_header_t* ring = laplace_transport_get_egress_ring(mapping);

    if (laplace_transport_ring_is_full(ring, LAPLACE_TRANSPORT_EGRESS_CAPACITY)) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t slot_index = ring->head & (LAPLACE_TRANSPORT_EGRESS_CAPACITY - 1u);
    laplace_transport_event_record_t* slots = laplace_transport_get_egress_slots(mapping);
    memcpy(&slots[slot_index], record, sizeof(laplace_transport_event_record_t));

    transport_fence_release();

    ring->head += 1u;
    return LAPLACE_OK;
}

laplace_error_t laplace_transport_egress_dequeue(laplace_transport_mapping_t* mapping,
                                                  laplace_transport_event_record_t* out_record) {
    if (mapping == NULL || mapping->view == NULL || out_record == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_transport_ring_header_t* ring = laplace_transport_get_egress_ring(mapping);

    transport_fence_acquire();

    if (laplace_transport_ring_is_empty(ring)) {
        return LAPLACE_ERR_INVALID_STATE;
    }

    const uint32_t slot_index = ring->tail & (LAPLACE_TRANSPORT_EGRESS_CAPACITY - 1u);
    const laplace_transport_event_record_t* slots = laplace_transport_get_egress_slots(mapping);
    memcpy(out_record, &slots[slot_index], sizeof(laplace_transport_event_record_t));

    transport_fence_release();

    ring->tail += 1u;
    return LAPLACE_OK;
}
