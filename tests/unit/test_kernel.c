#include "laplace/kernel.h"
#include "../test_harness.h"

static int test_kernel_enum_values(void) {
    LAPLACE_TEST_ASSERT(LAPLACE_KERNEL_INVALID    == 0u);
    LAPLACE_TEST_ASSERT(LAPLACE_KERNEL_RELATIONAL == 1u);
    LAPLACE_TEST_ASSERT(LAPLACE_KERNEL_PROOF      == 2u);
    LAPLACE_TEST_ASSERT(LAPLACE_KERNEL_BITVECTOR  == 3u);
    LAPLACE_TEST_ASSERT(LAPLACE_KERNEL_COUNT      == 4u);
    return 0;
}

static int test_kernel_id_size(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_kernel_id_t) == 1u);
    return 0;
}

static int test_kernel_name_valid(void) {
    const char* n0 = laplace_kernel_name(LAPLACE_KERNEL_INVALID);
    const char* n1 = laplace_kernel_name(LAPLACE_KERNEL_RELATIONAL);
    const char* n2 = laplace_kernel_name(LAPLACE_KERNEL_PROOF);
    const char* n3 = laplace_kernel_name(LAPLACE_KERNEL_BITVECTOR);

    LAPLACE_TEST_ASSERT(n0 != NULL);
    LAPLACE_TEST_ASSERT(n1 != NULL);
    LAPLACE_TEST_ASSERT(n2 != NULL);
    LAPLACE_TEST_ASSERT(n3 != NULL);

    LAPLACE_TEST_ASSERT(n0[0] == 'i'); /* "invalid" */
    LAPLACE_TEST_ASSERT(n1[0] == 'r'); /* "relational" */
    LAPLACE_TEST_ASSERT(n2[0] == 'p'); /* "proof" */
    LAPLACE_TEST_ASSERT(n3[0] == 'b'); /* "bitvector" */

    return 0;
}

static int test_kernel_name_out_of_range(void) {
    const char* n = laplace_kernel_name(LAPLACE_KERNEL_COUNT);
    LAPLACE_TEST_ASSERT(n != NULL);
    LAPLACE_TEST_ASSERT(n[0] == 'u'); /* "unknown" */

    const char* n2 = laplace_kernel_name(255u);
    LAPLACE_TEST_ASSERT(n2 != NULL);
    LAPLACE_TEST_ASSERT(n2[0] == 'u'); /* "unknown" */

    return 0;
}

int laplace_test_kernel(void) {
    int failures = 0;
    failures += test_kernel_enum_values();
    failures += test_kernel_id_size();
    failures += test_kernel_name_valid();
    failures += test_kernel_name_out_of_range();
    return failures;
}
