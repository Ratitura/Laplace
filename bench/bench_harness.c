#include "bench_harness.h"

#include <inttypes.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t laplace_bench_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);

    const uint64_t ticks = (uint64_t)counter.QuadPart;
    const uint64_t hz = (uint64_t)frequency.QuadPart;
    const uint64_t seconds = ticks / hz;
    const uint64_t remainder = ticks % hz;
    return seconds * 1000000000ULL + (remainder * 1000000000ULL) / hz;
#else
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

double laplace_bench_run_case(const laplace_bench_case_t* const bench_case) {
    if (bench_case == NULL || bench_case->fn == NULL || bench_case->iterations == 0u) {
        return -1.0;
    }

    for (uint64_t i = 0; i < 1024u; ++i) {
        bench_case->fn(bench_case->context);
    }

    const uint64_t start_ns = laplace_bench_now_ns();
    for (uint64_t i = 0; i < bench_case->iterations; ++i) {
        bench_case->fn(bench_case->context);
    }
    const uint64_t end_ns = laplace_bench_now_ns();

    const uint64_t elapsed_ns = end_ns - start_ns;
    const double ns_per_op = (double)elapsed_ns / (double)bench_case->iterations;

    printf("[BENCH] %s iterations=%" PRIu64 " elapsed_ns=%" PRIu64 " ns_per_op=%.3f\n",
           bench_case->name,
           bench_case->iterations,
           elapsed_ns,
           ns_per_op);

    return ns_per_op;
}
