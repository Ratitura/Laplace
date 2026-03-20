#include <stdint.h>
#include <stdio.h>

#include "bench_harness.h"
#include "laplace/bootstrap.h"
#include "laplace/hv.h"
#include "laplace/version.h"

void laplace_bench_hv(void);
void laplace_bench_exact(void);
void laplace_bench_exec(void);
void laplace_bench_branch(void);
void laplace_bench_transport(void);
void laplace_bench_observe(void);
void laplace_bench_adapter(void);
void laplace_bench_derive(void);
void laplace_bench_proof(void);
void laplace_bench_proof_verify(void);
void laplace_bench_proof_search(void);
void laplace_bench_graph_profile(void);
void laplace_bench_graph_import(void);

typedef struct laplace_bench_state {
    volatile uint64_t sink_u64;
    volatile const char* sink_ptr;
} laplace_bench_state_t;

static void laplace_bench_version_query(void* const context) {
    laplace_bench_state_t* const state = (laplace_bench_state_t*)context;
    state->sink_u64 = (uint64_t)laplace_version_major() +
                      (uint64_t)laplace_version_minor() +
                      (uint64_t)laplace_version_patch();
    state->sink_ptr = laplace_version_string();
}

static void laplace_bench_bootstrap_call(void* const context) {
    laplace_bench_state_t* const state = (laplace_bench_state_t*)context;
    state->sink_u64 = (uint64_t)laplace_bootstrap();
    state->sink_ptr = laplace_bootstrap_banner();
}

int main(void) {
    laplace_bench_state_t state = {0};

    const laplace_bench_case_t benches[] = {
        {"version_query", laplace_bench_version_query, &state, 10000000u},
        {"bootstrap_call", laplace_bench_bootstrap_call, &state, 10000000u},
    };

    const size_t bench_count = sizeof(benches) / sizeof(benches[0]);
    int failed = 0;

    printf("Project Laplace Phase 00 benchmark scaffold\n");

    for (size_t i = 0; i < bench_count; ++i) {
        const double ns_per_op = laplace_bench_run_case(&benches[i]);
        if (ns_per_op < 0.0) {
            ++failed;
        }
    }

    if (failed != 0) {
        fprintf(stderr, "Benchmark execution failed for %d case(s).\n", failed);
        return 1;
    }

    printf("\nProject Laplace HV benchmarks\n");
    printf("  Backend:   %s\n", laplace_hv_backend_name());
    printf("  HV dim:    %u bits (%u words)\n",
           (unsigned)LAPLACE_HV_DIM, (unsigned)LAPLACE_HV_WORDS);
    printf("  Build:     %s\n",
#if LAPLACE_DEBUG
           "debug"
#else
           "release"
#endif
    );
    laplace_bench_hv();

    printf("\nProject Laplace exact symbolic benchmarks\n");
    laplace_bench_exact();

    printf("\nProject Laplace execution system benchmarks\n");
    laplace_bench_exec();

    printf("\nProject Laplace branch lifecycle benchmarks\n");
    laplace_bench_branch();

    printf("\nProject Laplace transport benchmarks\n");
    laplace_bench_transport();

    printf("\nProject Laplace observability benchmarks\n");
    laplace_bench_observe();

    printf("\nProject Laplace adapter benchmarks\n");
    laplace_bench_adapter();

    printf("\nProject Laplace derive dispatch benchmarks\n");
    laplace_bench_derive();

    printf("\nProject Laplace proof substrate benchmarks\n");
    laplace_bench_proof();

    printf("\nProject Laplace proof verifier benchmarks\n");
    laplace_bench_proof_verify();

    printf("\nProject Laplace proof search benchmarks\n");
    laplace_bench_proof_search();

    printf("\nProject Laplace graph profile benchmarks\n");
    laplace_bench_graph_profile();

    printf("\nProject Laplace graph import benchmarks\n");
    laplace_bench_graph_import();

    return 0;
}
