#ifndef LAPLACE_DERIVE_H
#define LAPLACE_DERIVE_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/kernel.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAPLACE_DERIVE_API_VERSION 1u

#define LAPLACE_DERIVE_MAX_FACT_ARGS 8u
#define LAPLACE_DERIVE_MAX_RULE_BODY 8u
#define LAPLACE_DERIVE_MAX_ARITY     8u

#define LAPLACE_DERIVE_ACTION_PAYLOAD_SIZE 640u
#define LAPLACE_DERIVE_RESULT_PAYLOAD_SIZE 128u

typedef uint8_t laplace_derive_action_kind_t;

enum {
    LAPLACE_DERIVE_ACTION_INVALID               = 0u,

    LAPLACE_DERIVE_ACTION_QUERY_CAPABILITIES    = 1u,
    LAPLACE_DERIVE_ACTION_QUERY_STATS           = 2u,

    LAPLACE_DERIVE_ACTION_REL_ASSERT_FACT       = 10u,
    LAPLACE_DERIVE_ACTION_REL_LOOKUP_FACT       = 11u,
    LAPLACE_DERIVE_ACTION_REL_ADD_RULE          = 12u,
    LAPLACE_DERIVE_ACTION_REL_BUILD_TRIGGER_IDX = 13u,
    LAPLACE_DERIVE_ACTION_REL_EXEC_STEP         = 14u,
    LAPLACE_DERIVE_ACTION_REL_EXEC_RUN          = 15u,

    LAPLACE_DERIVE_ACTION_PROOF_IMPORT          = 30u,
    LAPLACE_DERIVE_ACTION_PROOF_TRY_ASSERTION   = 31u,
    LAPLACE_DERIVE_ACTION_PROOF_STEP            = 32u,
    LAPLACE_DERIVE_ACTION_PROOF_VERIFY          = 33u,
    LAPLACE_DERIVE_ACTION_PROOF_BUILD_INDEX     = 34u,
    LAPLACE_DERIVE_ACTION_PROOF_QUERY_CANDIDATES = 35u,
    LAPLACE_DERIVE_ACTION_PROOF_EXPAND_STATE    = 36u,

    LAPLACE_DERIVE_ACTION_BV_IMPORT             = 50u,
    LAPLACE_DERIVE_ACTION_BV_ASSERT_CONSTRAINT  = 51u,
    LAPLACE_DERIVE_ACTION_BV_VALIDATE_WITNESS   = 52u,
    LAPLACE_DERIVE_ACTION_BV_VERIFY_CERTIFICATE = 53u
};

typedef uint8_t laplace_derive_result_kind_t;

enum {
    LAPLACE_DERIVE_RESULT_INVALID              = 0u,
    LAPLACE_DERIVE_RESULT_ACK                  = 1u,
    LAPLACE_DERIVE_RESULT_REJECT               = 2u,
    LAPLACE_DERIVE_RESULT_CAPABILITY           = 3u,
    LAPLACE_DERIVE_RESULT_STATS                = 4u,
    LAPLACE_DERIVE_RESULT_REL_FACT             = 5u,
    LAPLACE_DERIVE_RESULT_REL_LOOKUP           = 6u,
    LAPLACE_DERIVE_RESULT_REL_RULE             = 7u,
    LAPLACE_DERIVE_RESULT_REL_EXEC             = 8u,
    LAPLACE_DERIVE_RESULT_UNSUPPORTED_KERNEL   = 9u,
    LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION   = 10u,
    LAPLACE_DERIVE_RESULT_PROOF_VERIFY         = 11u,
    LAPLACE_DERIVE_RESULT_PROOF_SEARCH          = 12u,
    LAPLACE_DERIVE_RESULT_PROOF_CANDIDATES      = 13u
};

typedef uint8_t laplace_derive_status_t;

enum {
    LAPLACE_DERIVE_STATUS_OK                   = 0u,
    LAPLACE_DERIVE_STATUS_INVALID_VERSION      = 1u,
    LAPLACE_DERIVE_STATUS_INVALID_KERNEL       = 2u,
    LAPLACE_DERIVE_STATUS_UNSUPPORTED_KERNEL   = 3u,
    LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION   = 4u,
    LAPLACE_DERIVE_STATUS_INVALID_ARGUMENT     = 5u,
    LAPLACE_DERIVE_STATUS_INVALID_LAYOUT       = 6u,
    LAPLACE_DERIVE_STATUS_VALIDATION_FAILED    = 7u,
    LAPLACE_DERIVE_STATUS_NOT_FOUND            = 8u,
    LAPLACE_DERIVE_STATUS_CONFLICT             = 9u,
    LAPLACE_DERIVE_STATUS_RESOURCE_LIMIT       = 10u,
    LAPLACE_DERIVE_STATUS_BUDGET_EXHAUSTED     = 11u,
    LAPLACE_DERIVE_STATUS_INTERNAL_INVARIANT   = 12u,
    LAPLACE_DERIVE_STATUS_COUNT_               = 13u
};

typedef struct laplace_derive_entity_ref {
    uint32_t id;
    uint32_t generation;
} laplace_derive_entity_ref_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_derive_entity_ref_t) == 8u,
                       "laplace_derive_entity_ref_t must be 8 bytes");

typedef struct laplace_derive_term {
    uint32_t kind;   /**< 0=INVALID, 1=VARIABLE, 2=CONSTANT */
    uint32_t value;  /**< variable ID or entity ID           */
} laplace_derive_term_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_derive_term_t) == 8u,
                       "laplace_derive_term_t must be 8 bytes");

typedef struct laplace_derive_literal {
    uint16_t             predicate_id;
    uint8_t              arity;
    uint8_t              reserved;
    laplace_derive_term_t terms[LAPLACE_DERIVE_MAX_ARITY];
} laplace_derive_literal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_derive_literal_t) == 68u,
                       "laplace_derive_literal_t must be 68 bytes");

typedef struct laplace_derive_rel_assert_fact_payload {
    uint16_t                    predicate_id;
    uint8_t                     arg_count;
    uint8_t                     reserved;
    uint32_t                    flags;
    laplace_derive_entity_ref_t args[LAPLACE_DERIVE_MAX_FACT_ARGS];
} laplace_derive_rel_assert_fact_payload_t;

typedef struct laplace_derive_rel_lookup_fact_payload {
    uint16_t                    predicate_id;
    uint8_t                     arg_count;
    uint8_t                     reserved;
    laplace_derive_entity_ref_t args[LAPLACE_DERIVE_MAX_FACT_ARGS];
} laplace_derive_rel_lookup_fact_payload_t;

typedef struct laplace_derive_rel_add_rule_payload {
    uint32_t                  body_count;
    laplace_derive_literal_t  head;
    laplace_derive_literal_t  body[LAPLACE_DERIVE_MAX_RULE_BODY];
} laplace_derive_rel_add_rule_payload_t;

typedef struct laplace_derive_rel_exec_run_payload {
    uint32_t max_steps;
    uint32_t max_derivations;
    uint8_t  mode;       /**< 0=DENSE, 1=SPARSE */
    uint8_t  semi_naive;
    uint8_t  reserved[2];
} laplace_derive_rel_exec_run_payload_t;

typedef struct laplace_derive_rel_fact_result {
    uint32_t fact_row;
    uint32_t entity_id;
    uint32_t generation;
    uint32_t provenance_id;
    uint8_t  inserted;
    uint8_t  reserved[3];
} laplace_derive_rel_fact_result_t;

typedef struct laplace_derive_rel_lookup_result {
    uint32_t fact_row;
    uint8_t  found;
    uint8_t  reserved[3];
} laplace_derive_rel_lookup_result_t;

typedef struct laplace_derive_rel_rule_result {
    uint32_t rule_id;
    uint32_t validation_error;
    uint32_t literal_index;
    uint32_t term_index;
} laplace_derive_rel_rule_result_t;

typedef struct laplace_derive_rel_exec_result {
    uint32_t run_status;
    uint32_t reserved;
    uint64_t steps_executed;
    uint64_t facts_derived;
    uint64_t facts_deduplicated;
    uint32_t fixpoint_rounds;
    uint32_t reserved2;
} laplace_derive_rel_exec_result_t;

typedef struct laplace_derive_proof_verify_payload {
    uint32_t theorem_id;   /**< theorem ID to verify (1-based) */
    uint32_t reserved[3];
} laplace_derive_proof_verify_payload_t;

typedef struct laplace_derive_proof_verify_result {
    uint32_t verify_status;       /**< laplace_proof_verify_status_t */
    uint32_t failure_step;        /**< step index on failure (0-based) */
    uint32_t failure_assertion;   /**< assertion ID on failure */
    uint32_t failure_hyp;         /**< hypothesis ID on failure */
    uint32_t detail_a;            /**< DV var_a or detail */
    uint32_t detail_b;            /**< DV var_b or detail */
    uint32_t steps_processed;     /**< total steps processed */
    uint32_t reserved;
} laplace_derive_proof_verify_result_t;

typedef struct laplace_derive_proof_search_try_payload {
    uint32_t goal_expr_id;
    uint32_t assertion_id;
    uint32_t reserved[2];
} laplace_derive_proof_search_try_payload_t;

typedef struct laplace_derive_proof_search_candidates_payload {
    uint32_t goal_expr_id;
    uint32_t reserved[3];
} laplace_derive_proof_search_candidates_payload_t;

typedef struct laplace_derive_proof_search_expand_payload {
    uint32_t obligation_index;
    uint32_t assertion_id;
    uint32_t reserved[2];
} laplace_derive_proof_search_expand_payload_t;

typedef struct laplace_derive_proof_search_result {
    uint32_t search_status;
    uint32_t subgoal_count;
    uint32_t subgoal_expr_ids[16];
    uint32_t assertion_id;
    uint32_t detail_a;
    uint32_t detail_b;
    uint32_t reserved;
} laplace_derive_proof_search_result_t;

typedef struct laplace_derive_proof_candidates_result {
    uint32_t candidate_count;
    uint32_t total_matches;
    uint8_t  truncated;
    uint8_t  reserved[3];
    uint32_t reserved2;
} laplace_derive_proof_candidates_result_t;

typedef struct laplace_derive_capability_result {
    uint32_t abi_version;
    uint32_t kernel_version_major;
    uint32_t kernel_version_minor;
    uint32_t kernel_version_patch;
    uint32_t hv_dimension;
    uint32_t hv_words;
    uint32_t max_predicates;
    uint32_t max_facts;
    uint32_t max_rules;
    uint32_t max_arity;
    uint32_t max_rule_body_literals;
    uint32_t max_branches;
    uint32_t supported_kernels;   /**< bitmask: bit 1=rel, 2=proof, 3=bv */
    uint32_t reserved[3];
} laplace_derive_capability_result_t;

typedef struct laplace_derive_stats_result {
    uint32_t predicate_count;
    uint32_t fact_count;
    uint32_t rule_count;
    uint32_t entity_alive_count;
    uint32_t entity_capacity;
    uint32_t reserved;
    uint64_t exec_steps;
    uint64_t exec_facts_derived;
    uint64_t exec_facts_deduplicated;
    uint32_t exec_fixpoint_rounds;
    uint32_t reserved2;
} laplace_derive_stats_result_t;

typedef struct laplace_derive_action {
    uint32_t                        api_version;        /*  0- 3  */
    laplace_kernel_id_t             kernel;             /*  4     */
    laplace_derive_action_kind_t    action;             /*  5     */
    uint16_t                        flags;              /*  6- 7  */
    uint64_t                        correlation_id;     /*  8-15  */
    laplace_branch_id_t             branch_id;          /* 16-17  */
    laplace_branch_generation_t     branch_generation;  /* 18-19  */
    laplace_epoch_id_t              epoch_id;           /* 20-23  */
    uint32_t                        budget;             /* 24-27  */
    uint32_t                        reserved;           /* 28-31  */
    union {                                             /* 32-671 */
        laplace_derive_rel_assert_fact_payload_t   rel_assert_fact;
        laplace_derive_rel_lookup_fact_payload_t   rel_lookup_fact;
        laplace_derive_rel_add_rule_payload_t      rel_add_rule;
        laplace_derive_rel_exec_run_payload_t      rel_exec_run;
        laplace_derive_proof_verify_payload_t      proof_verify;
        laplace_derive_proof_search_try_payload_t  proof_search_try;
        laplace_derive_proof_search_candidates_payload_t proof_search_candidates;
        laplace_derive_proof_search_expand_payload_t proof_search_expand;
        uint8_t                                    raw[LAPLACE_DERIVE_ACTION_PAYLOAD_SIZE];
    } payload;
} laplace_derive_action_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_derive_action_t) == 672u,
                       "laplace_derive_action_t must be 672 bytes");

typedef struct laplace_derive_result {
    uint32_t                        api_version;        /*  0- 3  */
    laplace_kernel_id_t             kernel;             /*  4     */
    laplace_derive_action_kind_t    action;             /*  5     */
    laplace_derive_result_kind_t    result_kind;        /*  6     */
    laplace_derive_status_t         status;             /*  7     */
    uint64_t                        correlation_id;     /*  8-15  */
    laplace_branch_id_t             branch_id;          /* 16-17  */
    laplace_branch_generation_t     branch_generation;  /* 18-19  */
    laplace_epoch_id_t              epoch_id;           /* 20-23  */
    uint64_t                        sequence;           /* 24-31  */
    union {                                             /* 32-159 */
        laplace_derive_rel_fact_result_t      rel_fact;
        laplace_derive_rel_lookup_result_t    rel_lookup;
        laplace_derive_rel_rule_result_t      rel_rule;
        laplace_derive_rel_exec_result_t      rel_exec;
        laplace_derive_capability_result_t    capability;
        laplace_derive_stats_result_t         stats;
        laplace_derive_proof_verify_result_t  proof_verify;
        laplace_derive_proof_search_result_t  proof_search;
        laplace_derive_proof_candidates_result_t proof_candidates;
        uint8_t                               raw[LAPLACE_DERIVE_RESULT_PAYLOAD_SIZE];
    } payload;
} laplace_derive_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_derive_result_t) == 160u,
                       "laplace_derive_result_t must be 160 bytes");

enum { LAPLACE_TRACE_SUBSYSTEM_DERIVE = 6u };

enum { LAPLACE_TRACE_KIND_DERIVE_DISPATCH = 18u };

enum { LAPLACE_TRACE_KIND_PROOF_VERIFY = 19u };

enum { LAPLACE_TRACE_KIND_PROOF_SEARCH = 20u };

#define LAPLACE_OBSERVE_MASK_DERIVE (1u << 4)

typedef struct laplace_trace_derive_payload {
    uint8_t  kernel;
    uint8_t  action;
    uint8_t  result_kind;
    uint8_t  derive_status;
    uint32_t detail;
    uint64_t reserved;
} laplace_trace_derive_payload_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_trace_derive_payload_t) == 16u,
                       "derive trace payload must fit in 16-byte slot");

struct laplace_exact_store;
struct laplace_exec_context;
struct laplace_branch_system;
struct laplace_observe_context;
struct laplace_proof_store;
struct laplace_proof_verify_context;
struct laplace_proof_search_context;

typedef struct laplace_derive_context {
    struct laplace_exact_store*          store;
    struct laplace_exec_context*         exec;
    struct laplace_branch_system*        branch;
    struct laplace_observe_context*      observe;
    struct laplace_proof_store*          proof_store;
    struct laplace_proof_verify_context* proof_verifier;
    struct laplace_proof_search_context* proof_search;
} laplace_derive_context_t;

void laplace_derive_context_init(
    laplace_derive_context_t*            ctx,
    struct laplace_exact_store*           store,
    struct laplace_exec_context*          exec,
    struct laplace_branch_system*         branch,
    struct laplace_observe_context*       observe,
    struct laplace_proof_store*           proof_store,
    struct laplace_proof_verify_context*  proof_verifier,
    struct laplace_proof_search_context*  proof_search);

void laplace_derive_dispatch(
    laplace_derive_context_t*      ctx,
    const laplace_derive_action_t* action,
    laplace_derive_result_t*       result);

const char* laplace_derive_status_string(laplace_derive_status_t status);

const char* laplace_derive_action_string(laplace_derive_action_kind_t action);

const char* laplace_derive_result_kind_string(laplace_derive_result_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_DERIVE_H */
