#ifndef LAPLACE_BENCH_HARNESS_H
#define LAPLACE_BENCH_HARNESS_H

#include <stdint.h>

typedef void (*laplace_bench_fn_t)(void* context);

typedef struct laplace_bench_case {
    const char* name;
    laplace_bench_fn_t fn;
    void* context;
    uint64_t iterations;
} laplace_bench_case_t;

uint64_t laplace_bench_now_ns(void);
double laplace_bench_run_case(const laplace_bench_case_t* bench_case);

#endif
