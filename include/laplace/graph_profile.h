#ifndef LAPLACE_GRAPH_PROFILE_H
#define LAPLACE_GRAPH_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAPLACE_GRAPH_PROFILE_VERSION 1u

typedef uint8_t laplace_graph_profile_id_t;

enum {
    LAPLACE_GRAPH_PROFILE_INVALID = 0u,
    LAPLACE_GRAPH_PROFILE_BASIC_TRIPLES = 1u,
    LAPLACE_GRAPH_PROFILE_HORN_CLOSURE = 2u,
    LAPLACE_GRAPH_PROFILE_COUNT = 3u
};

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_profile_id_t) == 1u,
                      "laplace_graph_profile_id_t must be 8-bit");

typedef uint32_t laplace_graph_fact_shape_t;

enum {
    LAPLACE_GRAPH_FACT_UNARY = 1u << 0,
    LAPLACE_GRAPH_FACT_BINARY = 1u << 1,
    LAPLACE_GRAPH_FACT_TERNARY = 1u << 2,
    LAPLACE_GRAPH_FACT_BOUNDED_NARY = 1u << 3
};

#define LAPLACE_GRAPH_FACT_ALL \
    (LAPLACE_GRAPH_FACT_UNARY | LAPLACE_GRAPH_FACT_BINARY | \
     LAPLACE_GRAPH_FACT_TERNARY | LAPLACE_GRAPH_FACT_BOUNDED_NARY)

typedef uint32_t laplace_graph_rule_shape_t;

enum {
    LAPLACE_GRAPH_RULE_POSITIVE_HORN = 1u << 0,
    LAPLACE_GRAPH_RULE_BOUNDED_BODY = 1u << 1,
    LAPLACE_GRAPH_RULE_DATALOG_SAFE = 1u << 2
};

#define LAPLACE_GRAPH_RULE_ALL \
    (LAPLACE_GRAPH_RULE_POSITIVE_HORN | LAPLACE_GRAPH_RULE_BOUNDED_BODY | \
     LAPLACE_GRAPH_RULE_DATALOG_SAFE)

typedef uint32_t laplace_graph_closure_cap_t;

enum {
    LAPLACE_GRAPH_CLOSURE_TRANSITIVE = 1u << 0,
    LAPLACE_GRAPH_CLOSURE_SYMMETRIC = 1u << 1,
    LAPLACE_GRAPH_CLOSURE_SUBCLASS = 1u << 2,
    LAPLACE_GRAPH_CLOSURE_SUBPROPERTY = 1u << 3,
    LAPLACE_GRAPH_CLOSURE_DOMAIN_RANGE = 1u << 4
};

#define LAPLACE_GRAPH_CLOSURE_ALL \
    (LAPLACE_GRAPH_CLOSURE_TRANSITIVE | LAPLACE_GRAPH_CLOSURE_SYMMETRIC | \
     LAPLACE_GRAPH_CLOSURE_SUBCLASS | LAPLACE_GRAPH_CLOSURE_SUBPROPERTY | \
     LAPLACE_GRAPH_CLOSURE_DOMAIN_RANGE)

typedef uint32_t laplace_graph_exclusion_t;

enum {
    LAPLACE_GRAPH_EXCL_FULL_RDF_MT = 1u << 0,
    LAPLACE_GRAPH_EXCL_FULL_RDFS = 1u << 1,
    LAPLACE_GRAPH_EXCL_OWL_RL = 1u << 2,
    LAPLACE_GRAPH_EXCL_OWL_DL = 1u << 3,
    LAPLACE_GRAPH_EXCL_SPARQL = 1u << 4,
    LAPLACE_GRAPH_EXCL_BLANK_NODE_GEN = 1u << 5,
    LAPLACE_GRAPH_EXCL_OPEN_WORLD = 1u << 6,
    LAPLACE_GRAPH_EXCL_NEGATION = 1u << 7,
    LAPLACE_GRAPH_EXCL_AGGREGATION = 1u << 8,
    LAPLACE_GRAPH_EXCL_FUNCTION_SYMBOLS = 1u << 9,
    LAPLACE_GRAPH_EXCL_DISJUNCTION = 1u << 10,
    LAPLACE_GRAPH_EXCL_NON_MONOTONIC = 1u << 11,
    LAPLACE_GRAPH_EXCL_TEXT_PARSING = 1u << 12,
    LAPLACE_GRAPH_EXCL_HEURISTIC_MATCH = 1u << 13
};

#define LAPLACE_GRAPH_EXCL_ALL \
    (LAPLACE_GRAPH_EXCL_FULL_RDF_MT | LAPLACE_GRAPH_EXCL_FULL_RDFS | \
     LAPLACE_GRAPH_EXCL_OWL_RL | LAPLACE_GRAPH_EXCL_OWL_DL | \
     LAPLACE_GRAPH_EXCL_SPARQL | LAPLACE_GRAPH_EXCL_BLANK_NODE_GEN | \
     LAPLACE_GRAPH_EXCL_OPEN_WORLD | LAPLACE_GRAPH_EXCL_NEGATION | \
     LAPLACE_GRAPH_EXCL_AGGREGATION | LAPLACE_GRAPH_EXCL_FUNCTION_SYMBOLS | \
     LAPLACE_GRAPH_EXCL_DISJUNCTION | LAPLACE_GRAPH_EXCL_NON_MONOTONIC | \
     LAPLACE_GRAPH_EXCL_TEXT_PARSING | LAPLACE_GRAPH_EXCL_HEURISTIC_MATCH)

#define LAPLACE_GRAPH_EXCL_COUNT 14u

typedef struct laplace_graph_snapshot_semantics {
    bool exact_snapshot;
    bool closed_world_local;
    bool monotonic;
    bool deterministic;
} laplace_graph_snapshot_semantics_t;

typedef struct laplace_graph_profile_descriptor {
    laplace_graph_profile_id_t profile_id;
    uint8_t reserved[3];
    const char* profile_name;
    laplace_graph_fact_shape_t supported_fact_shapes;
    laplace_graph_rule_shape_t supported_rule_shapes;
    laplace_graph_closure_cap_t supported_closures;
    laplace_graph_exclusion_t exclusions;
    laplace_graph_snapshot_semantics_t snapshot_semantics;
    uint32_t max_arity;
    uint32_t max_rule_body;
} laplace_graph_profile_descriptor_t;

const laplace_graph_profile_descriptor_t* laplace_graph_profile_get(laplace_graph_profile_id_t id);
const char* laplace_graph_profile_name(laplace_graph_profile_id_t id);
bool laplace_graph_profile_supports_fact_shape(laplace_graph_profile_id_t id, laplace_graph_fact_shape_t shape);
bool laplace_graph_profile_supports_rule_shape(laplace_graph_profile_id_t id, laplace_graph_rule_shape_t shape);
bool laplace_graph_profile_supports_closure(laplace_graph_profile_id_t id, laplace_graph_closure_cap_t closure);
bool laplace_graph_profile_is_excluded(laplace_graph_profile_id_t id, laplace_graph_exclusion_t feature);
laplace_graph_profile_id_t laplace_graph_profile_default(void);
laplace_graph_exclusion_t laplace_graph_profile_all_exclusions(void);
uint32_t laplace_graph_profile_version(void);

#ifdef __cplusplus
}
#endif

#endif
