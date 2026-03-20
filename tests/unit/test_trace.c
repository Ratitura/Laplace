#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/align.h"
#include "laplace/arena.h"
#include "laplace/trace.h"
#include "test_harness.h"

#define TEST_ARENA_SIZE (512u * 1024u)

static laplace_trace_record_t make_record(laplace_trace_kind_t kind,
                                           laplace_trace_subsystem_t subsystem,
                                           laplace_entity_id_t entity_id) {
    laplace_trace_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind      = (uint16_t)kind;
    rec.subsystem = (uint8_t)subsystem;
    rec.entity_id = entity_id;
    return rec;
}

static int test_trace_buffer_init(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    const laplace_error_t rc = laplace_trace_buffer_init(&buf, &arena);
    LAPLACE_TEST_ASSERT(rc == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(buf.capacity == LAPLACE_TRACE_BUFFER_CAPACITY);
    LAPLACE_TEST_ASSERT(buf.head == 0u);
    LAPLACE_TEST_ASSERT(buf.count == 0u);
    LAPLACE_TEST_ASSERT(buf.next_sequence == 1u);
    LAPLACE_TEST_ASSERT(buf.overflow_count == 0u);
    LAPLACE_TEST_ASSERT(buf.overflow_marked == false);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_overflow_count(&buf) == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_next_sequence(&buf) == 1u);
    return 0;
}

static int test_trace_buffer_init_null(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    LAPLACE_TEST_ASSERT(laplace_trace_buffer_init(NULL, &arena) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_trace_buffer_t buf;
    LAPLACE_TEST_ASSERT(laplace_trace_buffer_init(&buf, NULL) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_trace_emit_single(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    laplace_trace_record_t rec = make_record(
        LAPLACE_TRACE_KIND_FACT_ASSERTED,
        LAPLACE_TRACE_SUBSYSTEM_EXACT,
        42u);
    rec.tick = 100u;

    const laplace_trace_seq_t seq = laplace_trace_emit(&buf, &rec);
    LAPLACE_TEST_ASSERT(seq == 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_next_sequence(&buf) == 2u);
    LAPLACE_TEST_ASSERT(laplace_trace_overflow_count(&buf) == 0u);

    const laplace_trace_record_t* got = laplace_trace_get(&buf, 0u);
    LAPLACE_TEST_ASSERT(got != NULL);
    LAPLACE_TEST_ASSERT(got->sequence == 1u);
    LAPLACE_TEST_ASSERT(got->kind == (uint16_t)LAPLACE_TRACE_KIND_FACT_ASSERTED);
    LAPLACE_TEST_ASSERT(got->subsystem == (uint8_t)LAPLACE_TRACE_SUBSYSTEM_EXACT);
    LAPLACE_TEST_ASSERT(got->entity_id == 42u);
    LAPLACE_TEST_ASSERT(got->tick == 100u);
    return 0;
}

static int test_trace_sequence_monotonicity(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    laplace_trace_seq_t prev_seq = 0u;
    for (uint32_t i = 0u; i < 128u; ++i) {
        laplace_trace_record_t rec = make_record(
            LAPLACE_TRACE_KIND_EXEC_STEP,
            LAPLACE_TRACE_SUBSYSTEM_EXEC,
            i);
        const laplace_trace_seq_t seq = laplace_trace_emit(&buf, &rec);
        LAPLACE_TEST_ASSERT(seq > prev_seq);
        prev_seq = seq;
    }

    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 128u);

    for (uint32_t i = 0u; i < 128u; ++i) {
        const laplace_trace_record_t* got = laplace_trace_get(&buf, i);
        LAPLACE_TEST_ASSERT(got != NULL);
        LAPLACE_TEST_ASSERT(got->entity_id == i);
    }
    return 0;
}

static int test_trace_overflow(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    for (uint32_t i = 0u; i < LAPLACE_TRACE_BUFFER_CAPACITY; ++i) {
        laplace_trace_record_t rec = make_record(
            LAPLACE_TRACE_KIND_FACT_DERIVED,
            LAPLACE_TRACE_SUBSYSTEM_EXACT,
            i);
        laplace_trace_emit(&buf, &rec);
    }
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == LAPLACE_TRACE_BUFFER_CAPACITY);
    LAPLACE_TEST_ASSERT(laplace_trace_overflow_count(&buf) == 0u);

    laplace_trace_record_t extra = make_record(
        LAPLACE_TRACE_KIND_BRANCH_CREATE,
        LAPLACE_TRACE_SUBSYSTEM_BRANCH,
        9999u);
    laplace_trace_emit(&buf, &extra);

    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == LAPLACE_TRACE_BUFFER_CAPACITY);
    LAPLACE_TEST_ASSERT(laplace_trace_overflow_count(&buf) > 0u);

    const laplace_trace_record_t* newest = laplace_trace_get(&buf,
        laplace_trace_count(&buf) - 1u);
    LAPLACE_TEST_ASSERT(newest != NULL);
    LAPLACE_TEST_ASSERT(newest->entity_id == 9999u);
    LAPLACE_TEST_ASSERT(newest->kind == (uint16_t)LAPLACE_TRACE_KIND_BRANCH_CREATE);

    return 0;
}

static int test_trace_reset(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    for (uint32_t i = 0u; i < 10u; ++i) {
        laplace_trace_record_t rec = make_record(
            LAPLACE_TRACE_KIND_RULE_ACCEPTED,
            LAPLACE_TRACE_SUBSYSTEM_EXACT,
            i);
        laplace_trace_emit(&buf, &rec);
    }
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 10u);

    laplace_trace_buffer_reset(&buf);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_overflow_count(&buf) == 0u);
    LAPLACE_TEST_ASSERT(laplace_trace_next_sequence(&buf) == 1u);

    laplace_trace_record_t rec = make_record(
        LAPLACE_TRACE_KIND_EPOCH_ADVANCE,
        LAPLACE_TRACE_SUBSYSTEM_BRANCH,
        0u);
    const laplace_trace_seq_t seq = laplace_trace_emit(&buf, &rec);
    LAPLACE_TEST_ASSERT(seq == 1u);
    LAPLACE_TEST_ASSERT(laplace_trace_count(&buf) == 1u);
    return 0;
}

static int test_trace_get_out_of_range(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    LAPLACE_TEST_ASSERT(laplace_trace_get(&buf, 0u) == NULL);
    LAPLACE_TEST_ASSERT(laplace_trace_get(&buf, 100u) == NULL);

    for (uint32_t i = 0u; i < 5u; ++i) {
        laplace_trace_record_t rec = make_record(
            LAPLACE_TRACE_KIND_FACT_ASSERTED,
            LAPLACE_TRACE_SUBSYSTEM_EXACT,
            i);
        laplace_trace_emit(&buf, &rec);
    }

    LAPLACE_TEST_ASSERT(laplace_trace_get(&buf, 0u) != NULL);
    LAPLACE_TEST_ASSERT(laplace_trace_get(&buf, 4u) != NULL);
    LAPLACE_TEST_ASSERT(laplace_trace_get(&buf, 5u) == NULL);
    return 0;
}

static int test_trace_payload_fact(void) {
    LAPLACE_ALIGNAS(64) uint8_t backing[TEST_ARENA_SIZE];
    laplace_arena_t arena;
    laplace_arena_init(&arena, backing, sizeof(backing));

    laplace_trace_buffer_t buf;
    laplace_trace_buffer_init(&buf, &arena);

    laplace_trace_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind      = (uint16_t)LAPLACE_TRACE_KIND_FACT_DERIVED;
    rec.subsystem = (uint8_t)LAPLACE_TRACE_SUBSYSTEM_EXACT;
    rec.entity_id = 7u;
    rec.rule_id   = 3u;
    rec.branch_id = 1u;
    rec.payload.fact.fact_row  = 42u;
    rec.payload.fact.predicate = 5u;

    laplace_trace_emit(&buf, &rec);

    const laplace_trace_record_t* got = laplace_trace_get(&buf, 0u);
    LAPLACE_TEST_ASSERT(got != NULL);
    LAPLACE_TEST_ASSERT(got->payload.fact.fact_row == 42u);
    LAPLACE_TEST_ASSERT(got->payload.fact.predicate == 5u);
    LAPLACE_TEST_ASSERT(got->rule_id == 3u);
    LAPLACE_TEST_ASSERT(got->branch_id == 1u);
    return 0;
}

static int test_trace_record_size(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_trace_record_t) == 64u);
    return 0;
}

int laplace_test_trace(void) {
    typedef struct { const char* name; int (*fn)(void); } sub_t;
    const sub_t subs[] = {
        {"trace_buffer_init",          test_trace_buffer_init},
        {"trace_buffer_init_null",     test_trace_buffer_init_null},
        {"trace_emit_single",          test_trace_emit_single},
        {"trace_sequence_monotonicity", test_trace_sequence_monotonicity},
        {"trace_overflow",             test_trace_overflow},
        {"trace_reset",                test_trace_reset},
        {"trace_get_out_of_range",     test_trace_get_out_of_range},
        {"trace_payload_fact",         test_trace_payload_fact},
        {"trace_record_size",          test_trace_record_size},
    };
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); ++i) {
        const int r = subs[i].fn();
        if (r != 0) {
            fprintf(stderr, "  SUBTEST FAIL: %s\n", subs[i].name);
            return 1;
        }
    }
    return 0;
}
