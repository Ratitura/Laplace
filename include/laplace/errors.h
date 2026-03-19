#ifndef LAPLACE_ERRORS_H
#define LAPLACE_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum laplace_error {
    LAPLACE_OK = 0,
    LAPLACE_ERR_INVALID_ARGUMENT = 1,
    LAPLACE_ERR_OUT_OF_RANGE = 2,
    LAPLACE_ERR_OVERFLOW = 3,
    LAPLACE_ERR_NOT_SUPPORTED = 4,
    LAPLACE_ERR_INTERNAL = 5,
    LAPLACE_ERR_CAPACITY_EXHAUSTED = 6,
    LAPLACE_ERR_BAD_ALIGNMENT = 7,
    LAPLACE_ERR_INVALID_STATE = 8,
    LAPLACE_ERR_GENERATION_MISMATCH = 9
} laplace_error_t;

const char* laplace_error_string(laplace_error_t error);

#ifdef __cplusplus
}
#endif

#endif
