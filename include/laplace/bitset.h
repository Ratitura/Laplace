#ifndef LAPLACE_BITSET_H
#define LAPLACE_BITSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/assert.h"
#include "laplace/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAPLACE_BITSET_BITS_PER_WORD    64u
#define LAPLACE_BITSET_WORD_SHIFT       6u
#define LAPLACE_BITSET_WORD_MASK        63u

#define LAPLACE_BITSET_WORDS_FOR_BITS(n) \
    (((size_t)(n) + (LAPLACE_BITSET_BITS_PER_WORD - 1u)) >> LAPLACE_BITSET_WORD_SHIFT)

typedef struct laplace_bitset {
    uint64_t* words;       /* caller-owned word array */
    uint32_t  bit_count;   /* logical bit capacity */
    uint32_t  word_count;  /* number of uint64_t words */
} laplace_bitset_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_bitset_t) <= 24u, "laplace_bitset_t should be compact");

void laplace_bitset_init(laplace_bitset_t* bs, uint64_t* words, uint32_t bit_count, uint32_t word_count);

void laplace_bitset_clear_all(laplace_bitset_t* bs);

void laplace_bitset_set_all(laplace_bitset_t* bs);

static inline bool laplace_bitset_test(const laplace_bitset_t* bs, uint32_t index) {
    LAPLACE_ASSERT(bs != NULL);
    LAPLACE_ASSERT(index < bs->bit_count);
    const uint32_t word_idx = index >> LAPLACE_BITSET_WORD_SHIFT;
    const uint32_t bit_idx = index & LAPLACE_BITSET_WORD_MASK;
    return (bs->words[word_idx] & (UINT64_C(1) << bit_idx)) != 0u;
}

static inline void laplace_bitset_set(laplace_bitset_t* bs, uint32_t index) {
    LAPLACE_ASSERT(bs != NULL);
    LAPLACE_ASSERT(index < bs->bit_count);
    const uint32_t word_idx = index >> LAPLACE_BITSET_WORD_SHIFT;
    const uint32_t bit_idx = index & LAPLACE_BITSET_WORD_MASK;
    bs->words[word_idx] |= (UINT64_C(1) << bit_idx);
}

static inline void laplace_bitset_clear(laplace_bitset_t* bs, uint32_t index) {
    LAPLACE_ASSERT(bs != NULL);
    LAPLACE_ASSERT(index < bs->bit_count);
    const uint32_t word_idx = index >> LAPLACE_BITSET_WORD_SHIFT;
    const uint32_t bit_idx = index & LAPLACE_BITSET_WORD_MASK;
    bs->words[word_idx] &= ~(UINT64_C(1) << bit_idx);
}

static inline void laplace_bitset_toggle(laplace_bitset_t* bs, uint32_t index) {
    LAPLACE_ASSERT(bs != NULL);
    LAPLACE_ASSERT(index < bs->bit_count);
    const uint32_t word_idx = index >> LAPLACE_BITSET_WORD_SHIFT;
    const uint32_t bit_idx = index & LAPLACE_BITSET_WORD_MASK;
    bs->words[word_idx] ^= (UINT64_C(1) << bit_idx);
}

static inline uint32_t laplace_popcount_u64(const uint64_t value) {
#if defined(__clang__) || defined(__GNUC__)
    return (uint32_t)__builtin_popcountll(value);
#elif defined(_MSC_VER)
    return (uint32_t)__popcnt64(value);
#else
    uint64_t v = value;
    uint32_t count = 0u;
    while (v) {
        v &= (v - 1u);
        ++count;
    }
    return count;
#endif
}

uint32_t laplace_bitset_popcount(const laplace_bitset_t* bs);

void laplace_bitset_and(laplace_bitset_t* dst, const laplace_bitset_t* a, const laplace_bitset_t* b);

void laplace_bitset_or(laplace_bitset_t* dst, const laplace_bitset_t* a, const laplace_bitset_t* b);

void laplace_bitset_xor(laplace_bitset_t* dst, const laplace_bitset_t* a, const laplace_bitset_t* b);

void laplace_bitset_not(laplace_bitset_t* bs);

bool laplace_bitset_any(const laplace_bitset_t* bs);

bool laplace_bitset_none(const laplace_bitset_t* bs);

uint32_t laplace_bitset_find_first_set(const laplace_bitset_t* bs);

uint32_t laplace_bitset_find_next_set(const laplace_bitset_t* bs, uint32_t start_after);

#ifdef __cplusplus
}
#endif

#endif
