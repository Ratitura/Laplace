#ifndef LAPLACE_HV_H
#define LAPLACE_HV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LAPLACE_HV_DIM
#define LAPLACE_HV_DIM 16384u
#endif

#define LAPLACE_HV_BITS_PER_WORD  64u
#define LAPLACE_HV_WORDS          ((uint32_t)((LAPLACE_HV_DIM) / LAPLACE_HV_BITS_PER_WORD))
#define LAPLACE_HV_BYTES          ((uint32_t)(LAPLACE_HV_WORDS * sizeof(uint64_t)))
#define LAPLACE_HV_CACHELINES     ((uint32_t)(LAPLACE_HV_BYTES / 64u))

LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM > 0u,
                       "LAPLACE_HV_DIM must be positive");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM % LAPLACE_HV_BITS_PER_WORD == 0u,
                       "LAPLACE_HV_DIM must be a multiple of 64");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_WORDS == 256u || LAPLACE_HV_DIM != 16384u,
                       "default 16384-bit dimension must yield exactly 256 words");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_BYTES == LAPLACE_HV_WORDS * 8u,
                       "LAPLACE_HV_BYTES must equal LAPLACE_HV_WORDS * sizeof(uint64_t)");
LAPLACE_STATIC_ASSERT(LAPLACE_HV_DIM % 64u == 0u && LAPLACE_HV_BYTES % 64u == 0u,
                       "HV byte size must be a multiple of cache-line size (64)");

typedef struct laplace_hv {
    uint64_t words[LAPLACE_HV_WORDS];
} laplace_hv_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_hv_t) == (size_t)LAPLACE_HV_WORDS * sizeof(uint64_t),
                       "laplace_hv_t must be tightly packed");

void laplace_hv_zero(laplace_hv_t* dst);

void laplace_hv_random(laplace_hv_t* dst, uint64_t seed);

void laplace_hv_copy(laplace_hv_t* dst, const laplace_hv_t* src);

void laplace_hv_bind(laplace_hv_t* dst,
                      const laplace_hv_t* a,
                      const laplace_hv_t* b);

static inline void laplace_hv_unbind(laplace_hv_t* dst,
                                      const laplace_hv_t* a,
                                      const laplace_hv_t* b) {
    laplace_hv_bind(dst, a, b);
}

uint32_t laplace_hv_distance(const laplace_hv_t* a,
                              const laplace_hv_t* b);

void laplace_hv_bundle(laplace_hv_t* dst,
                        const laplace_hv_t* const* vectors,
                        uint32_t count,
                        uint64_t tie_seed);

void laplace_hv_bundle_reference(laplace_hv_t* dst,
                                  const laplace_hv_t* const* vectors,
                                  uint32_t count,
                                  uint64_t tie_seed);

void laplace_hv_bundle_generic(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

void laplace_hv_bundle2_direct(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

void laplace_hv_bundle3_direct(laplace_hv_t* dst,
                                const laplace_hv_t* const* vectors,
                                uint32_t count,
                                uint64_t tie_seed);

uint32_t laplace_hv_popcount(const laplace_hv_t* hv);

bool laplace_hv_equal(const laplace_hv_t* a, const laplace_hv_t* b);

double laplace_hv_similarity(const laplace_hv_t* a, const laplace_hv_t* b);

const char* laplace_hv_backend_name(void);

const char* laplace_hv_backend_name_bind(void);
const char* laplace_hv_backend_name_distance(void);
const char* laplace_hv_backend_name_popcount(void);

#ifdef __cplusplus
}
#endif

#endif
