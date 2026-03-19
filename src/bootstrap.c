#include "laplace/bootstrap.h"

#include <stddef.h>

#include "laplace/align.h"
#include "laplace/config.h"
#include "laplace/types.h"
#include "laplace/version.h"

laplace_error_t laplace_bootstrap(void) {
    if (LAPLACE_CACHELINE_BYTES != (size_t)LAPLACE_CACHELINE_SIZE) {
        return LAPLACE_ERR_INTERNAL;
    }

    if ((LAPLACE_CACHELINE_BYTES & (LAPLACE_CACHELINE_BYTES - 1u)) != 0u) {
        return LAPLACE_ERR_INTERNAL;
    }

    if (sizeof(laplace_entity_id_t) != 4u || sizeof(laplace_generation_t) != 4u) {
        return LAPLACE_ERR_INTERNAL;
    }

    if (laplace_version_major() != LAPLACE_VERSION_MAJOR ||
        laplace_version_minor() != LAPLACE_VERSION_MINOR ||
        laplace_version_patch() != LAPLACE_VERSION_PATCH) {
        return LAPLACE_ERR_INTERNAL;
    }

    return LAPLACE_OK;
}

const char* laplace_bootstrap_banner(void) {
    return "Project Laplace foundation initialized";
}
