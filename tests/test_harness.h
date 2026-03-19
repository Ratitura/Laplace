#ifndef LAPLACE_TEST_HARNESS_H
#define LAPLACE_TEST_HARNESS_H

#include <stdio.h>

#define LAPLACE_TEST_ASSERT(condition)                                                         \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            return 1;                                                                          \
        }                                                                                      \
    } while (0)

typedef int (*laplace_test_fn_t)(void);

typedef struct laplace_test_case {
    const char* name;
    laplace_test_fn_t fn;
} laplace_test_case_t;

#endif
