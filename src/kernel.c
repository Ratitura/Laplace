#include "laplace/kernel.h"

static const char* const laplace__kernel_names[LAPLACE_KERNEL_COUNT] = {
    "invalid",
    "relational",
    "proof",
    "bitvector"
};

const char* laplace_kernel_name(laplace_kernel_id_t id) {
    if (id >= LAPLACE_KERNEL_COUNT) {
        return "unknown";
    }
    return laplace__kernel_names[id];
}
