#ifndef LAPLACE_VERSION_H
#define LAPLACE_VERSION_H

#include <stdint.h>

#define LAPLACE_VERSION_MAJOR 0u
#define LAPLACE_VERSION_MINOR 1u
#define LAPLACE_VERSION_PATCH 0u

#ifdef __cplusplus
extern "C" {
#endif

uint32_t laplace_version_major(void);
uint32_t laplace_version_minor(void);
uint32_t laplace_version_patch(void);
const char* laplace_version_string(void);

#ifdef __cplusplus
}
#endif

#endif
