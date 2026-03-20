#include "laplace/graph_profile.h"
#include "laplace/exact.h"

static const laplace_graph_profile_descriptor_t s_profiles[LAPLACE_GRAPH_PROFILE_COUNT] = {
    {
        .profile_id = LAPLACE_GRAPH_PROFILE_INVALID,
        .reserved = {0u, 0u, 0u},
        .profile_name = "INVALID",
        .supported_fact_shapes = 0u,
        .supported_rule_shapes = 0u,
        .supported_closures = 0u,
        .exclusions = LAPLACE_GRAPH_EXCL_ALL,
        .snapshot_semantics = {false, false, false, false},
        .max_arity = 0u,
        .max_rule_body = 0u,
    },
    {
        .profile_id = LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES,
        .reserved = {0u, 0u, 0u},
        .profile_name = "BASIC_TRIPLES",
        .supported_fact_shapes = LAPLACE_GRAPH_FACT_ALL,
        .supported_rule_shapes = 0u,
        .supported_closures = 0u,
        .exclusions = LAPLACE_GRAPH_EXCL_ALL,
        .snapshot_semantics = {true, true, true, true},
        .max_arity = LAPLACE_EXACT_MAX_ARITY,
        .max_rule_body = 0u,
    },
    {
        .profile_id = LAPLACE_GRAPH_PROFILE_HORN_CLOSURE,
        .reserved = {0u, 0u, 0u},
        .profile_name = "HORN_CLOSURE",
        .supported_fact_shapes = LAPLACE_GRAPH_FACT_ALL,
        .supported_rule_shapes = LAPLACE_GRAPH_RULE_ALL,
        .supported_closures = LAPLACE_GRAPH_CLOSURE_ALL,
        .exclusions = LAPLACE_GRAPH_EXCL_ALL,
        .snapshot_semantics = {true, true, true, true},
        .max_arity = LAPLACE_EXACT_MAX_ARITY,
        .max_rule_body = LAPLACE_EXACT_MAX_RULE_BODY_LITERALS,
    },
};

const laplace_graph_profile_descriptor_t* laplace_graph_profile_get(const laplace_graph_profile_id_t id) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return &s_profiles[LAPLACE_GRAPH_PROFILE_INVALID];
    }
    return &s_profiles[id];
}

const char* laplace_graph_profile_name(const laplace_graph_profile_id_t id) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return "UNKNOWN";
    }
    return s_profiles[id].profile_name;
}

bool laplace_graph_profile_supports_fact_shape(const laplace_graph_profile_id_t id,
                                               const laplace_graph_fact_shape_t shape) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return false;
    }
    return (s_profiles[id].supported_fact_shapes & shape) == shape;
}

bool laplace_graph_profile_supports_rule_shape(const laplace_graph_profile_id_t id,
                                               const laplace_graph_rule_shape_t shape) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return false;
    }
    return (s_profiles[id].supported_rule_shapes & shape) == shape;
}

bool laplace_graph_profile_supports_closure(const laplace_graph_profile_id_t id,
                                            const laplace_graph_closure_cap_t closure) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return false;
    }
    return (s_profiles[id].supported_closures & closure) == closure;
}

bool laplace_graph_profile_is_excluded(const laplace_graph_profile_id_t id,
                                       const laplace_graph_exclusion_t feature) {
    if (id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        return true;
    }
    return (s_profiles[id].exclusions & feature) == feature;
}

laplace_graph_profile_id_t laplace_graph_profile_default(void) {
    return LAPLACE_GRAPH_PROFILE_HORN_CLOSURE;
}

laplace_graph_exclusion_t laplace_graph_profile_all_exclusions(void) {
    return LAPLACE_GRAPH_EXCL_ALL;
}

uint32_t laplace_graph_profile_version(void) {
    return LAPLACE_GRAPH_PROFILE_VERSION;
}
