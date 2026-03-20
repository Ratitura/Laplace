#include "test_harness.h"
#include "laplace/graph_profile.h"
#include "laplace/exact.h"

#include <string.h>

static int test_profile_ids(void) {
    LAPLACE_TEST_ASSERT(LAPLACE_GRAPH_PROFILE_INVALID == 0u);
    LAPLACE_TEST_ASSERT(LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES == 1u);
    LAPLACE_TEST_ASSERT(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE == 2u);
    return 0;
}

static int test_profile_default(void) {
    LAPLACE_TEST_ASSERT(laplace_graph_profile_default() == LAPLACE_GRAPH_PROFILE_HORN_CLOSURE);
    return 0;
}

static int test_profile_limits(void) {
    const laplace_graph_profile_descriptor_t* d = laplace_graph_profile_get(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE);
    LAPLACE_TEST_ASSERT(d != NULL);
    LAPLACE_TEST_ASSERT(d->max_arity == LAPLACE_EXACT_MAX_ARITY);
    LAPLACE_TEST_ASSERT(d->max_rule_body == LAPLACE_EXACT_MAX_RULE_BODY_LITERALS);
    return 0;
}

static int test_profile_exclusions(void) {
    LAPLACE_TEST_ASSERT(laplace_graph_profile_is_excluded(LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES, LAPLACE_GRAPH_EXCL_SPARQL));
    LAPLACE_TEST_ASSERT(laplace_graph_profile_is_excluded(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE, LAPLACE_GRAPH_EXCL_OWL_DL));
    LAPLACE_TEST_ASSERT(laplace_graph_profile_all_exclusions() == LAPLACE_GRAPH_EXCL_ALL);
    return 0;
}

static int test_profile_names(void) {
    LAPLACE_TEST_ASSERT(strcmp(laplace_graph_profile_name(LAPLACE_GRAPH_PROFILE_INVALID), "INVALID") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_graph_profile_name(LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES), "BASIC_TRIPLES") == 0);
    LAPLACE_TEST_ASSERT(strcmp(laplace_graph_profile_name(LAPLACE_GRAPH_PROFILE_HORN_CLOSURE), "HORN_CLOSURE") == 0);
    return 0;
}

int laplace_test_graph_profile(void) {
    int result = 0;
    result |= test_profile_ids();
    result |= test_profile_default();
    result |= test_profile_limits();
    result |= test_profile_exclusions();
    result |= test_profile_names();
    return result;
}
