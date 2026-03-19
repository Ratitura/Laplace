#include <string.h>

#include "laplace/bootstrap.h"
#include "laplace/errors.h"
#include "laplace/version.h"
#include "test_harness.h"

int laplace_test_version(void) {
    LAPLACE_TEST_ASSERT(laplace_version_major() == LAPLACE_VERSION_MAJOR);
    LAPLACE_TEST_ASSERT(laplace_version_minor() == LAPLACE_VERSION_MINOR);
    LAPLACE_TEST_ASSERT(laplace_version_patch() == LAPLACE_VERSION_PATCH);

    const char* version = laplace_version_string();
    LAPLACE_TEST_ASSERT(version != NULL);
    LAPLACE_TEST_ASSERT(strlen(version) > 0u);

    const laplace_error_t rc = laplace_bootstrap();
    LAPLACE_TEST_ASSERT(rc == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(strcmp(laplace_bootstrap_banner(), "Project Laplace foundation initialized") == 0);

    return 0;
}
