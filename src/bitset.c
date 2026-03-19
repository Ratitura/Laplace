#include "laplace/bitset.h"

void laplace_bitset_init(laplace_bitset_t* const bs, uint64_t* const words,
                         const uint32_t bit_count, const uint32_t word_count) {
    LAPLACE_ASSERT(bs != NULL);
    LAPLACE_ASSERT(words != NULL);
    LAPLACE_ASSERT(bit_count > 0u);
    LAPLACE_ASSERT(word_count >= (uint32_t)LAPLACE_BITSET_WORDS_FOR_BITS(bit_count));

    bs->words = words;
    bs->bit_count = bit_count;
    bs->word_count = word_count;

    memset(words, 0, (size_t)word_count * sizeof(uint64_t));
}

void laplace_bitset_clear_all(laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);
    memset(bs->words, 0, (size_t)bs->word_count * sizeof(uint64_t));
}

void laplace_bitset_set_all(laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);

    if (bs->word_count == 0u) {
        return;
    }

    const uint32_t full_words = bs->bit_count >> LAPLACE_BITSET_WORD_SHIFT;
    for (uint32_t i = 0u; i < full_words; ++i) {
        bs->words[i] = UINT64_MAX;
    }

    /* Handle the tail word: only set valid bit positions */
    const uint32_t tail_bits = bs->bit_count & LAPLACE_BITSET_WORD_MASK;
    if (tail_bits > 0u) {
        bs->words[full_words] = (UINT64_C(1) << tail_bits) - 1u;
    }
}

uint32_t laplace_bitset_popcount(const laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);

    uint32_t count = 0u;
    for (uint32_t i = 0u; i < bs->word_count; ++i) {
        count += laplace_popcount_u64(bs->words[i]);
    }
    return count;
}

void laplace_bitset_and(laplace_bitset_t* const dst, const laplace_bitset_t* const a,
                        const laplace_bitset_t* const b) {
    LAPLACE_ASSERT(dst != NULL && a != NULL && b != NULL);
    LAPLACE_ASSERT(dst->word_count == a->word_count && a->word_count == b->word_count);

    for (uint32_t i = 0u; i < dst->word_count; ++i) {
        dst->words[i] = a->words[i] & b->words[i];
    }
}

void laplace_bitset_or(laplace_bitset_t* const dst, const laplace_bitset_t* const a,
                       const laplace_bitset_t* const b) {
    LAPLACE_ASSERT(dst != NULL && a != NULL && b != NULL);
    LAPLACE_ASSERT(dst->word_count == a->word_count && a->word_count == b->word_count);

    for (uint32_t i = 0u; i < dst->word_count; ++i) {
        dst->words[i] = a->words[i] | b->words[i];
    }
}

void laplace_bitset_xor(laplace_bitset_t* const dst, const laplace_bitset_t* const a,
                        const laplace_bitset_t* const b) {
    LAPLACE_ASSERT(dst != NULL && a != NULL && b != NULL);
    LAPLACE_ASSERT(dst->word_count == a->word_count && a->word_count == b->word_count);

    for (uint32_t i = 0u; i < dst->word_count; ++i) {
        dst->words[i] = a->words[i] ^ b->words[i];
    }
}

void laplace_bitset_not(laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);

    for (uint32_t i = 0u; i < bs->word_count; ++i) {
        bs->words[i] = ~bs->words[i];
    }

    /* Mask out invalid bits in the tail word */
    const uint32_t tail_bits = bs->bit_count & LAPLACE_BITSET_WORD_MASK;
    if (tail_bits > 0u && bs->word_count > 0u) {
        bs->words[bs->word_count - 1u] &= (UINT64_C(1) << tail_bits) - 1u;
    }
}

bool laplace_bitset_any(const laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);
    for (uint32_t i = 0u; i < bs->word_count; ++i) {
        if (bs->words[i] != 0u) {
            return true;
        }
    }
    return false;
}

bool laplace_bitset_none(const laplace_bitset_t* const bs) {
    return !laplace_bitset_any(bs);
}

static inline uint32_t laplace__ctz_u64(const uint64_t value) {
    LAPLACE_ASSERT(value != 0u);
#if defined(__clang__) || defined(__GNUC__)
    return (uint32_t)__builtin_ctzll(value);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, value);
    return (uint32_t)idx;
#else
    uint32_t n = 0u;
    uint64_t v = value;
    while ((v & 1u) == 0u) {
        v >>= 1;
        ++n;
    }
    return n;
#endif
}

uint32_t laplace_bitset_find_first_set(const laplace_bitset_t* const bs) {
    LAPLACE_ASSERT(bs != NULL);
    for (uint32_t i = 0u; i < bs->word_count; ++i) {
        if (bs->words[i] != 0u) {
            const uint32_t bit_in_word = laplace__ctz_u64(bs->words[i]);
            const uint32_t index = i * LAPLACE_BITSET_BITS_PER_WORD + bit_in_word;
            if (index < bs->bit_count) {
                return index;
            }
            return UINT32_MAX;
        }
    }
    return UINT32_MAX;
}

uint32_t laplace_bitset_find_next_set(const laplace_bitset_t* const bs, const uint32_t start_after) {
    LAPLACE_ASSERT(bs != NULL);

    const uint32_t start = start_after + 1u;
    if (start >= bs->bit_count) {
        return UINT32_MAX;
    }

    uint32_t word_idx = start >> LAPLACE_BITSET_WORD_SHIFT;
    const uint32_t bit_offset = start & LAPLACE_BITSET_WORD_MASK;

    uint64_t masked = bs->words[word_idx] >> bit_offset;
    if (masked != 0u) {
        const uint32_t index = word_idx * LAPLACE_BITSET_BITS_PER_WORD + bit_offset + laplace__ctz_u64(masked);
        if (index < bs->bit_count) {
            return index;
        }
        return UINT32_MAX;
    }

    ++word_idx;
    for (; word_idx < bs->word_count; ++word_idx) {
        if (bs->words[word_idx] != 0u) {
            const uint32_t index = word_idx * LAPLACE_BITSET_BITS_PER_WORD + laplace__ctz_u64(bs->words[word_idx]);
            if (index < bs->bit_count) {
                return index;
            }
            return UINT32_MAX;
        }
    }

    return UINT32_MAX;
}
