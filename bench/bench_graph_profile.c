#include <stdint.h>

#include "bench_harness.h"
#include "laplace/graph_profile.h"

typedef struct bench_graph_profile_ctx {
    volatile uint32_t sink_u32;
    volatile const void* sink_ptr;
} bench_graph_profile_ctx_t;

static bench_graph_profile_ctx_t g_ctx;

static void bench_graph_profile_query(void* const context) {
    bench_graph_profile_ctx_t* const ctx = (bench_graph_profile_ctx_t*)context;
    const laplace_graph_profile_descriptor_t* d = laplace_graph_profile_get(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE);
    ctx->sink_u32 = d->supported_fact_shapes ^ d->supported_rule_shapes ^ d->supported_closures;
    ctx->sink_ptr = d->profile_name;
}

void laplace_bench_graph_profile(void) {
    const laplace_bench_case_t benches[] = {
        {"graph_profile_query", bench_graph_profile_query, &g_ctx, 10000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
