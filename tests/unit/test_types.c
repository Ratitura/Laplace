#include <stddef.h>
#include <stdint.h>

#include "laplace/align.h"
#include "laplace/types.h"
#include "test_harness.h"

int laplace_test_types(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_entity_id_t) == 4u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_generation_t) == 4u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_rule_id_t) == 4u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_tick_t) == 8u);

    LAPLACE_TEST_ASSERT(laplace_is_pow2_u64(64u));
    LAPLACE_TEST_ASSERT(!laplace_is_pow2_u64(0u));
    LAPLACE_TEST_ASSERT(!laplace_is_pow2_u64(63u));

    LAPLACE_TEST_ASSERT(laplace_align_up_uintptr((uintptr_t)65u, (uintptr_t)64u) == (uintptr_t)128u);
    LAPLACE_TEST_ASSERT(laplace_is_aligned_uintptr((uintptr_t)128u, (uintptr_t)64u));
    LAPLACE_TEST_ASSERT(!laplace_is_aligned_uintptr((uintptr_t)130u, (uintptr_t)64u));

    return 0;
}
