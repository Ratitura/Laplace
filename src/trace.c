#include "laplace/trace.h"

#include <string.h>

#include "laplace/arena.h"

laplace_error_t laplace_trace_buffer_init(laplace_trace_buffer_t* const buf,
                                           laplace_arena_t* const arena) {
    if (buf == NULL || arena == NULL) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(buf, 0, sizeof(*buf));

    const size_t alloc_size = (size_t)LAPLACE_TRACE_BUFFER_CAPACITY * sizeof(laplace_trace_record_t);
    void* const mem = laplace_arena_alloc(arena, alloc_size, _Alignof(laplace_trace_record_t));
    if (mem == NULL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    memset(mem, 0, alloc_size);
    buf->records       = (laplace_trace_record_t*)mem;
    buf->capacity      = LAPLACE_TRACE_BUFFER_CAPACITY;
    buf->head          = 0u;
    buf->count         = 0u;
    buf->next_sequence = 1u;
    buf->overflow_count  = 0u;
    buf->overflow_marked = false;

    return LAPLACE_OK;
}

void laplace_trace_buffer_reset(laplace_trace_buffer_t* const buf) {
    if (buf == NULL) {
        return;
    }

    buf->head            = 0u;
    buf->count           = 0u;
    buf->next_sequence   = 1u;
    buf->overflow_count  = 0u;
    buf->overflow_marked = false;

}

static uint32_t trace_write_slot(laplace_trace_buffer_t* const buf,
                                  const laplace_trace_record_t* const record,
                                  const laplace_trace_seq_t seq) {
    const uint32_t slot = buf->head & (buf->capacity - 1u);

    buf->records[slot] = *record;
    buf->records[slot].sequence = seq;

    buf->head = (buf->head + 1u) & (buf->capacity - 1u);
    if (buf->count < buf->capacity) {
        buf->count += 1u;
    }

    return slot;
}

laplace_trace_seq_t laplace_trace_emit(laplace_trace_buffer_t* const buf,
                                        const laplace_trace_record_t* const record) {
    LAPLACE_ASSERT(buf != NULL && buf->records != NULL);
    LAPLACE_ASSERT(record != NULL);

    const bool will_overflow = (buf->count >= buf->capacity);

    if (will_overflow && !buf->overflow_marked) {
        buf->overflow_marked = true;

        laplace_trace_record_t marker;
        memset(&marker, 0, sizeof(marker));
        marker.kind      = (uint16_t)LAPLACE_TRACE_KIND_OVERFLOW_MARKER;
        marker.subsystem = (uint8_t)LAPLACE_TRACE_SUBSYSTEM_OBSERVE;
        marker.payload.overflow.overflow_count = buf->overflow_count + 1u;

        const laplace_trace_seq_t marker_seq = buf->next_sequence++;
        trace_write_slot(buf, &marker, marker_seq);
        buf->overflow_count += 1u;
    }

    if (will_overflow) {
        buf->overflow_count += 1u;
    }

    const laplace_trace_seq_t seq = buf->next_sequence++;
    trace_write_slot(buf, record, seq);

    return seq;
}

uint32_t laplace_trace_count(const laplace_trace_buffer_t* const buf) {
    LAPLACE_ASSERT(buf != NULL);
    return buf->count;
}

const laplace_trace_record_t* laplace_trace_get(const laplace_trace_buffer_t* const buf,
                                                 const uint32_t index) {
    LAPLACE_ASSERT(buf != NULL);
    if (index >= buf->count) {
        return NULL;
    }

    uint32_t start;
    if (buf->count < buf->capacity) {
        start = 0u;
    } else {
        start = buf->head;
    }

    const uint32_t slot = (start + index) & (buf->capacity - 1u);
    return &buf->records[slot];
}

uint64_t laplace_trace_overflow_count(const laplace_trace_buffer_t* const buf) {
    LAPLACE_ASSERT(buf != NULL);
    return buf->overflow_count;
}

laplace_trace_seq_t laplace_trace_next_sequence(const laplace_trace_buffer_t* const buf) {
    LAPLACE_ASSERT(buf != NULL);
    return buf->next_sequence;
}
