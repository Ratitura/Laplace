#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/assert.h"
#include "laplace/state.h"
#include "test_harness.h"

static int test_state_names(void) {
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_FREE), "FREE") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_PENDING), "PENDING") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_READY), "READY") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_ACTIVE), "ACTIVE") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_MASKED), "MASKED") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_DEAD), "DEAD") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name(LAPLACE_STATE_RETIRED), "RETIRED") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_state_name((laplace_entity_state_t)99), "UNKNOWN") == 0);
    return 0;
}

static int test_state_valid_transitions(void) {
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_FREE, LAPLACE_STATE_PENDING));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_PENDING, LAPLACE_STATE_READY));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_PENDING, LAPLACE_STATE_DEAD));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_READY, LAPLACE_STATE_ACTIVE));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_READY, LAPLACE_STATE_MASKED));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_READY, LAPLACE_STATE_DEAD));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_ACTIVE, LAPLACE_STATE_MASKED));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_ACTIVE, LAPLACE_STATE_DEAD));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_MASKED, LAPLACE_STATE_ACTIVE));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_MASKED, LAPLACE_STATE_DEAD));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_DEAD, LAPLACE_STATE_RETIRED));
    LAPLACE_TEST_ASSERT(laplace_state_transition_valid(LAPLACE_STATE_RETIRED, LAPLACE_STATE_FREE));
    return 0;
}

static int test_state_invalid_transitions(void) {
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_FREE, LAPLACE_STATE_FREE));
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_ACTIVE, LAPLACE_STATE_ACTIVE));

    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_FREE, LAPLACE_STATE_READY));
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_PENDING, LAPLACE_STATE_ACTIVE));
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_DEAD, LAPLACE_STATE_FREE));

    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_READY, LAPLACE_STATE_PENDING));
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_ACTIVE, LAPLACE_STATE_READY));

    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid((laplace_entity_state_t)99, LAPLACE_STATE_FREE));
    LAPLACE_TEST_ASSERT(!laplace_state_transition_valid(LAPLACE_STATE_FREE, (laplace_entity_state_t)99));
    return 0;
}

static int test_state_exhaustive_matrix(void) {
    int valid_count = 0;
    for (uint32_t from = 0; from < LAPLACE_STATE_COUNT_; ++from) {
        for (uint32_t to = 0; to < LAPLACE_STATE_COUNT_; ++to) {
            if (laplace_state_transition_valid((laplace_entity_state_t)from, (laplace_entity_state_t)to)) {
                ++valid_count;
            }
        }
    }
    LAPLACE_TEST_ASSERT(valid_count == 12);
    return 0;
}

int laplace_test_state(void) {
    const laplace_test_case_t subtests[] = {
        {"state_names", test_state_names},
        {"state_valid_transitions", test_state_valid_transitions},
        {"state_invalid_transitions", test_state_invalid_transitions},
        {"state_exhaustive_matrix", test_state_exhaustive_matrix},
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
