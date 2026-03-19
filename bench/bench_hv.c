#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/hv.h"

/*
 * Forward declarations for scalar word-level kernels.
 * These are always available regardless of backend selection.
 * Used here to benchmark the scalar path directly for comparison
 * when an optimized backend is active.
 */
extern void     laplace__hv_bind_words_scalar(uint64_t* restrict dst,
                                               const uint64_t* a,
                                               const uint64_t* b,
                                               uint32_t num_words);
extern uint32_t laplace__hv_xor_popcount_words_scalar(const uint64_t* a,
                                                       const uint64_t* b,
                                                       uint32_t num_words);
extern uint32_t laplace__hv_popcount_words_scalar(const uint64_t* words,
                                                   uint32_t num_words);

/*
 * Benchmark context for HV operations.
 * Uses volatile sink to prevent dead-code elimination.
 */
typedef struct hv_bench_ctx {
    laplace_hv_t a;
    laplace_hv_t b;
    laplace_hv_t dst;
    volatile uint32_t sink_u32;
    volatile double   sink_f64;
} hv_bench_ctx_t;

static hv_bench_ctx_t g_ctx;

/* Pre-generate test vectors once. */
static void hv_bench_setup(void) {
    laplace_hv_random(&g_ctx.a, 0xBEEFCAFEu);
    laplace_hv_random(&g_ctx.b, 0xDEADF00Du);
    laplace_hv_zero(&g_ctx.dst);
    g_ctx.sink_u32 = 0u;
    g_ctx.sink_f64 = 0.0;
}

static void bench_hv_bind(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bind(&ctx->dst, &ctx->a, &ctx->b);
}

static void bench_hv_distance(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    ctx->sink_u32 = laplace_hv_distance(&ctx->a, &ctx->b);
}

static void bench_hv_popcount(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    ctx->sink_u32 = laplace_hv_popcount(&ctx->a);
}

static void bench_hv_similarity(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    ctx->sink_f64 = laplace_hv_similarity(&ctx->a, &ctx->b);
}

static void bench_hv_bind_scalar(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace__hv_bind_words_scalar(ctx->dst.words,
                                   ctx->a.words, ctx->b.words,
                                   LAPLACE_HV_WORDS);
}

static void bench_hv_distance_scalar(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    ctx->sink_u32 = laplace__hv_xor_popcount_words_scalar(
        ctx->a.words, ctx->b.words, LAPLACE_HV_WORDS);
}

static void bench_hv_popcount_scalar(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    ctx->sink_u32 = laplace__hv_popcount_words_scalar(
        ctx->a.words, LAPLACE_HV_WORDS);
}

static void bench_hv_similarity_scalar(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    const uint32_t dist = laplace__hv_xor_popcount_words_scalar(
        ctx->a.words, ctx->b.words, LAPLACE_HV_WORDS);
    ctx->sink_f64 = 1.0 - ((double)dist / (double)LAPLACE_HV_DIM);
}

static void bench_hv_random(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_random(&ctx->dst, ctx->sink_u32);
    ctx->sink_u32 += 1u;
}

/* --- bundle N=1 (trivial copy fast path) --- */

static void bench_hv_bundle1(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    const laplace_hv_t* ptrs[1] = { &ctx->a };
    laplace_hv_bundle(&ctx->dst, ptrs, 1u, 0u);
}

/* --- bundle N=2 (fast path: word-level tie-break) --- */

static laplace_hv_t g_bvecs2[2];
static const laplace_hv_t* g_bptrs2[2];

static void bench_hv_bundle2(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs2, 2u, 42u);
}

/* --- bundle N=3 (fast path: bitwise majority) --- */

static laplace_hv_t g_bvecs3[3];
static const laplace_hv_t* g_bptrs3[3];

static void bench_hv_bundle3(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs3, 3u, 42u);
}

/* --- bundle N=4 (fast path: carry-chain counter) --- */

static laplace_hv_t g_bvecs4[4];
static const laplace_hv_t* g_bptrs4[4];

static void bench_hv_bundle4_even(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs4, 4u, 42u);
}

/* --- bundle N=5 (generic odd, bit-sliced) --- */

static laplace_hv_t g_bvecs5[5];
static const laplace_hv_t* g_bptrs5[5];

static void bench_hv_bundle5_odd(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs5, 5u, 42u);
}

/* --- bundle N=7 (generic odd, bit-sliced) --- */

static laplace_hv_t g_bvecs7[7];
static const laplace_hv_t* g_bptrs7[7];

static void bench_hv_bundle7(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs7, 7u, 42u);
}

/* --- bundle N=15 (generic odd, bit-sliced, larger) --- */

static laplace_hv_t g_bvecs15[15];
static const laplace_hv_t* g_bptrs15[15];

static void bench_hv_bundle15(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle(&ctx->dst, g_bptrs15, 15u, 42u);
}

static void bench_hv_bundle2_direct(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle2_direct(&ctx->dst, g_bptrs2, 2u, 42u);
}

static void bench_hv_bundle3_direct(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle3_direct(&ctx->dst, g_bptrs3, 3u, 42u);
}

static void bench_hv_bundle2_generic(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_generic(&ctx->dst, g_bptrs2, 2u, 42u);
}

static void bench_hv_bundle3_generic(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_generic(&ctx->dst, g_bptrs3, 3u, 42u);
}

static void bench_hv_bundle4_generic(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_generic(&ctx->dst, g_bptrs4, 4u, 42u);
}

static void bench_hv_bundle7_generic(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_generic(&ctx->dst, g_bptrs7, 7u, 42u);
}

static void bench_hv_bundle2_ref(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_reference(&ctx->dst, g_bptrs2, 2u, 42u);
}

static void bench_hv_bundle3_ref(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_reference(&ctx->dst, g_bptrs3, 3u, 42u);
}

static void bench_hv_bundle5_ref(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_reference(&ctx->dst, g_bptrs5, 5u, 42u);
}

static void bench_hv_bundle7_ref(void* const context) {
    hv_bench_ctx_t* const ctx = (hv_bench_ctx_t*)context;
    laplace_hv_bundle_reference(&ctx->dst, g_bptrs7, 7u, 42u);
}

static void hv_bench_setup_bundle_vectors(void) {
    for (uint32_t i = 0u; i < 2u; ++i) {
        laplace_hv_random(&g_bvecs2[i], 2000u + (uint64_t)i);
        g_bptrs2[i] = &g_bvecs2[i];
    }
    for (uint32_t i = 0u; i < 3u; ++i) {
        laplace_hv_random(&g_bvecs3[i], 3000u + (uint64_t)i);
        g_bptrs3[i] = &g_bvecs3[i];
    }
    for (uint32_t i = 0u; i < 4u; ++i) {
        laplace_hv_random(&g_bvecs4[i], 4000u + (uint64_t)i);
        g_bptrs4[i] = &g_bvecs4[i];
    }
    for (uint32_t i = 0u; i < 5u; ++i) {
        laplace_hv_random(&g_bvecs5[i], 5000u + (uint64_t)i);
        g_bptrs5[i] = &g_bvecs5[i];
    }
    for (uint32_t i = 0u; i < 7u; ++i) {
        laplace_hv_random(&g_bvecs7[i], 7000u + (uint64_t)i);
        g_bptrs7[i] = &g_bvecs7[i];
    }
    for (uint32_t i = 0u; i < 15u; ++i) {
        laplace_hv_random(&g_bvecs15[i], 15000u + (uint64_t)i);
        g_bptrs15[i] = &g_bvecs15[i];
    }
}

void laplace_bench_hv(void) {
    /* Setup */
    hv_bench_setup();
    hv_bench_setup_bundle_vectors();

    printf("  Active HV backend: %s\n\n", laplace_hv_backend_name());

    /* --- Core ops via active backend --- */
    printf("  Core ops (active backend: %s):\n", laplace_hv_backend_name());
    const laplace_bench_case_t core_benches[] = {
        {"hv_bind",       bench_hv_bind,       &g_ctx, 1000000u},
        {"hv_distance",   bench_hv_distance,   &g_ctx, 1000000u},
        {"hv_popcount",   bench_hv_popcount,   &g_ctx, 1000000u},
        {"hv_similarity", bench_hv_similarity, &g_ctx, 1000000u},
        {"hv_random",     bench_hv_random,     &g_ctx, 500000u},
    };

    const size_t core_count = sizeof(core_benches) / sizeof(core_benches[0]);
    for (size_t i = 0u; i < core_count; ++i) {
        laplace_bench_run_case(&core_benches[i]);
    }

    /* --- Core ops via scalar-direct (reference baseline, always available) --- */
    printf("\n  Core ops (scalar-direct reference):\n");
    const laplace_bench_case_t scalar_benches[] = {
        {"hv_bind_scalar",       bench_hv_bind_scalar,       &g_ctx, 1000000u},
        {"hv_distance_scalar",   bench_hv_distance_scalar,   &g_ctx, 1000000u},
        {"hv_popcount_scalar",   bench_hv_popcount_scalar,   &g_ctx, 1000000u},
        {"hv_similarity_scalar", bench_hv_similarity_scalar, &g_ctx, 1000000u},
    };

    const size_t scalar_count = sizeof(scalar_benches) / sizeof(scalar_benches[0]);
    for (size_t i = 0u; i < scalar_count; ++i) {
        laplace_bench_run_case(&scalar_benches[i]);
    }

    /* --- Bundle: optimized production paths (with fast-path dispatch) --- */
    /* Path mapping:
     *   hv_bundle2_even -> dispatch -> laplace__hv_bundle2_words
     *   hv_bundle3      -> dispatch -> laplace__hv_bundle3_words
     *   hv_bundle4_even -> dispatch -> laplace__hv_bundle4_words
     *   hv_bundle5_odd  -> dispatch -> laplace__hv_bundle_generic (bit-sliced)
     *   hv_bundle7      -> dispatch -> laplace__hv_bundle_generic (bit-sliced)
     *   hv_bundle15     -> dispatch -> laplace__hv_bundle_generic (bit-sliced)
     */
    printf("\n  Bundle (optimized, dispatched):\n");
    const laplace_bench_case_t bundle_opt_benches[] = {
        {"hv_bundle1",       bench_hv_bundle1,      &g_ctx, 1000000u},
        {"hv_bundle2_even",  bench_hv_bundle2,      &g_ctx, 1000000u},
        {"hv_bundle3",       bench_hv_bundle3,      &g_ctx, 1000000u},
        {"hv_bundle4_even",  bench_hv_bundle4_even, &g_ctx, 1000000u},
        {"hv_bundle5_odd",   bench_hv_bundle5_odd,  &g_ctx, 200000u},
        {"hv_bundle7",       bench_hv_bundle7,      &g_ctx, 100000u},
        {"hv_bundle15",      bench_hv_bundle15,     &g_ctx, 50000u},
    };

    const size_t bundle_opt_count = sizeof(bundle_opt_benches) / sizeof(bundle_opt_benches[0]);
    for (size_t i = 0u; i < bundle_opt_count; ++i) {
        laplace_bench_run_case(&bundle_opt_benches[i]);
    }

    /* --- Bundle: direct fast-path (bypasses dispatch if-chain) --- */
    /* Path mapping:
     *   hv_bundle2_direct -> laplace__hv_bundle2_words (no dispatch)
     *   hv_bundle3_direct -> laplace__hv_bundle3_words (no dispatch)
     * Comparing these against the dispatched variants above reveals
     * the cost (if any) of the dispatch if-chain.
     */
    printf("\n  Bundle (direct fast-path, no dispatch):\n");
    const laplace_bench_case_t bundle_direct_benches[] = {
        {"hv_bundle2_direct",  bench_hv_bundle2_direct, &g_ctx, 1000000u},
        {"hv_bundle3_direct",  bench_hv_bundle3_direct, &g_ctx, 1000000u},
    };

    const size_t bundle_direct_count = sizeof(bundle_direct_benches) / sizeof(bundle_direct_benches[0]);
    for (size_t i = 0u; i < bundle_direct_count; ++i) {
        laplace_bench_run_case(&bundle_direct_benches[i]);
    }

    /* --- Bundle: forced-generic path (bit-sliced, no fast-path dispatch) --- */
    /* These cases intentionally bypass dispatch fast paths to validate that
     * the generic bit-sliced kernel is measured directly for the same k. */
    printf("\n  Bundle (forced-generic, bit-sliced):\n");
    const laplace_bench_case_t bundle_gen_benches[] = {
        {"hv_bundle2_generic",  bench_hv_bundle2_generic, &g_ctx, 200000u},
        {"hv_bundle3_generic",  bench_hv_bundle3_generic, &g_ctx, 200000u},
        {"hv_bundle4_generic",  bench_hv_bundle4_generic, &g_ctx, 200000u},
        {"hv_bundle7_generic",  bench_hv_bundle7_generic, &g_ctx, 100000u},
    };

    const size_t bundle_gen_count = sizeof(bundle_gen_benches) / sizeof(bundle_gen_benches[0]);
    for (size_t i = 0u; i < bundle_gen_count; ++i) {
        laplace_bench_run_case(&bundle_gen_benches[i]);
    }

    /* --- Bundle: reference path (bit-by-bit oracle, for comparison) --- */
    /* These cases measure the trusted reference implementation and should
     * remain materially slower than forced-generic/optimized paths. */
    printf("\n  Bundle (reference, bit-by-bit):\n");
    const laplace_bench_case_t bundle_ref_benches[] = {
        {"hv_bundle2_ref",   bench_hv_bundle2_ref,  &g_ctx, 200000u},
        {"hv_bundle3_ref",   bench_hv_bundle3_ref,  &g_ctx, 200000u},
        {"hv_bundle5_ref",   bench_hv_bundle5_ref,  &g_ctx, 200000u},
        {"hv_bundle7_ref",   bench_hv_bundle7_ref,  &g_ctx, 100000u},
    };

    const size_t bundle_ref_count = sizeof(bundle_ref_benches) / sizeof(bundle_ref_benches[0]);
    for (size_t i = 0u; i < bundle_ref_count; ++i) {
        laplace_bench_run_case(&bundle_ref_benches[i]);
    }
}
