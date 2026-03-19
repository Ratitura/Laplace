#include "laplace/version.h"

#define LAPLACE_STRINGIFY_IMPL(x) #x
#define LAPLACE_STRINGIFY(x) LAPLACE_STRINGIFY_IMPL(x)

static const char laplace__version_string[] =
    LAPLACE_STRINGIFY(LAPLACE_VERSION_MAJOR) "."
    LAPLACE_STRINGIFY(LAPLACE_VERSION_MINOR) "."
    LAPLACE_STRINGIFY(LAPLACE_VERSION_PATCH);

uint32_t laplace_version_major(void) {
    return LAPLACE_VERSION_MAJOR;
}

uint32_t laplace_version_minor(void) {
    return LAPLACE_VERSION_MINOR;
}

uint32_t laplace_version_patch(void) {
    return LAPLACE_VERSION_PATCH;
}

const char* laplace_version_string(void) {
    return laplace__version_string;
}
