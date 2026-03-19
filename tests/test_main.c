#include <stdio.h>

#include "laplace/hv.h"
#include "test_harness.h"

int laplace_test_version(void);
int laplace_test_config(void);
int laplace_test_types(void);
int laplace_test_arena(void);
int laplace_test_bitset(void);
int laplace_test_state(void);
int laplace_test_entity(void);
int laplace_test_pool(void);
int laplace_test_hv(void);
int laplace_test_hv_parity(void);
int laplace_test_exact(void);
int laplace_test_exec(void);
int laplace_test_branch(void);
int laplace_test_transport(void);
int laplace_test_trace(void);
int laplace_test_replay(void);
int laplace_test_observe(void);
int laplace_test_adapter(void);

int main(void) {
    const laplace_test_case_t tests[] = {
        {"version", laplace_test_version},
        {"config", laplace_test_config},
        {"types", laplace_test_types},
        {"arena", laplace_test_arena},
        {"bitset", laplace_test_bitset},
        {"state", laplace_test_state},
        {"entity", laplace_test_entity},
        {"pool", laplace_test_pool},
        {"hv", laplace_test_hv},
        {"hv_parity", laplace_test_hv_parity},
        {"exact", laplace_test_exact},
        {"exec", laplace_test_exec},
        {"branch", laplace_test_branch},
        {"transport", laplace_test_transport},
        {"trace", laplace_test_trace},
        {"replay", laplace_test_replay},
        {"observe", laplace_test_observe},
        {"adapter", laplace_test_adapter},
    };

    int failures = 0;
    const size_t test_count = sizeof(tests) / sizeof(tests[0]);

    printf("Project Laplace test suite\n");
    printf("  Backend: %s\n", laplace_hv_backend_name());
    printf("  HV dim:  %u bits (%u words)\n",
           (unsigned)LAPLACE_HV_DIM, (unsigned)LAPLACE_HV_WORDS);
    printf("\n");

    for (size_t i = 0; i < test_count; ++i) {
        const int result = tests[i].fn();
        if (result == 0) {
            printf("[PASS] %s\n", tests[i].name);
        } else {
            printf("[FAIL] %s\n", tests[i].name);
            ++failures;
        }
    }

    if (failures == 0) {
        printf("All %zu tests passed.\n", test_count);
        return 0;
    }

    printf("%d/%zu tests failed.\n", failures, test_count);
    return 1;
}
