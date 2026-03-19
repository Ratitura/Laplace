#include "laplace/errors.h"

const char* laplace_error_string(const laplace_error_t error) {
    switch (error) {
        case LAPLACE_OK:
            return "LAPLACE_OK";
        case LAPLACE_ERR_INVALID_ARGUMENT:
            return "LAPLACE_ERR_INVALID_ARGUMENT";
        case LAPLACE_ERR_OUT_OF_RANGE:
            return "LAPLACE_ERR_OUT_OF_RANGE";
        case LAPLACE_ERR_OVERFLOW:
            return "LAPLACE_ERR_OVERFLOW";
        case LAPLACE_ERR_NOT_SUPPORTED:
            return "LAPLACE_ERR_NOT_SUPPORTED";
        case LAPLACE_ERR_INTERNAL:
            return "LAPLACE_ERR_INTERNAL";
        case LAPLACE_ERR_CAPACITY_EXHAUSTED:
            return "LAPLACE_ERR_CAPACITY_EXHAUSTED";
        case LAPLACE_ERR_BAD_ALIGNMENT:
            return "LAPLACE_ERR_BAD_ALIGNMENT";
        case LAPLACE_ERR_INVALID_STATE:
            return "LAPLACE_ERR_INVALID_STATE";
        case LAPLACE_ERR_GENERATION_MISMATCH:
            return "LAPLACE_ERR_GENERATION_MISMATCH";
        default:
            return "LAPLACE_ERR_UNKNOWN";
    }
}
