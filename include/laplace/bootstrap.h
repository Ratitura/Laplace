#ifndef LAPLACE_BOOTSTRAP_H
#define LAPLACE_BOOTSTRAP_H

#include "laplace/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

laplace_error_t laplace_bootstrap(void);
const char* laplace_bootstrap_banner(void);

#ifdef __cplusplus
}
#endif

#endif
