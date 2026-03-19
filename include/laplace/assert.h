#ifndef LAPLACE_ASSERT_H
#define LAPLACE_ASSERT_H

#include <assert.h>

#include "laplace/compiler.h"

#ifndef LAPLACE_DEBUG
#define LAPLACE_DEBUG 0
#endif

#define LAPLACE_STATIC_ASSERT(condition, message) _Static_assert((condition), message)

#if LAPLACE_DEBUG
#define LAPLACE_ASSERT(condition) assert((condition))
#else
#define LAPLACE_ASSERT(condition) ((void)sizeof(condition))
#endif

#define LAPLACE_UNREACHABLE_ASSERT() \
    do {                          \
        LAPLACE_ASSERT(0);        \
        LAPLACE_UNREACHABLE();    \
    } while (0)

#endif
