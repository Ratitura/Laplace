#ifndef LAPLACE_PROOF_SEARCH_H
#define LAPLACE_PROOF_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/proof.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t laplace_proof_goal_id_t;
typedef uint32_t laplace_proof_search_state_id_t;

enum {
    LAPLACE_PROOF_GOAL_ID_INVALID         = 0u,
    LAPLACE_PROOF_SEARCH_STATE_ID_INVALID = 0u
};

enum {
    LAPLACE_PROOF_SEARCH_MAX_OBLIGATIONS  = 64u,
    LAPLACE_PROOF_SEARCH_MAX_CANDIDATES   = 256u,
    LAPLACE_PROOF_SEARCH_MAX_SUBGOALS     = 16u,
    LAPLACE_PROOF_SEARCH_MAX_STATES       = 4096u,
    LAPLACE_PROOF_SEARCH_MAX_INDEX_ENTRIES = 65536u,
    LAPLACE_PROOF_SEARCH_MAX_DEPTH        = 256u,
    LAPLACE_PROOF_SEARCH_MAX_SUBST_VARS   = 256u,
    LAPLACE_PROOF_SEARCH_MAX_INST_TOKENS  = 16384u
};

typedef enum laplace_proof_search_status {
    LAPLACE_PROOF_SEARCH_SUCCESS                    = 0u,
    LAPLACE_PROOF_SEARCH_NO_CANDIDATE_MATCH         = 1u,
    LAPLACE_PROOF_SEARCH_UNIFICATION_CONFLICT       = 2u,
    LAPLACE_PROOF_SEARCH_TYPECODE_MISMATCH          = 3u,
    LAPLACE_PROOF_SEARCH_ESSENTIAL_HYP_MISMATCH     = 4u,
    LAPLACE_PROOF_SEARCH_DV_VIOLATION               = 5u,
    LAPLACE_PROOF_SEARCH_INVALID_GOAL               = 6u,
    LAPLACE_PROOF_SEARCH_INVALID_ASSERTION          = 7u,
    LAPLACE_PROOF_SEARCH_RESOURCE_LIMIT             = 8u,
    LAPLACE_PROOF_SEARCH_UNSUPPORTED_ACTION         = 9u,
    LAPLACE_PROOF_SEARCH_INTERNAL_INVARIANT         = 10u,
    LAPLACE_PROOF_SEARCH_STATUS_COUNT_              = 11u
} laplace_proof_search_status_t;

typedef enum laplace_proof_search_state_status {
    LAPLACE_PROOF_SEARCH_STATE_OPEN       = 0u,
    LAPLACE_PROOF_SEARCH_STATE_PROVED     = 1u,
    LAPLACE_PROOF_SEARCH_STATE_FAILED     = 2u,
    LAPLACE_PROOF_SEARCH_STATE_STATUS_COUNT_ = 3u
} laplace_proof_search_state_status_t;

typedef struct laplace_proof_search_goal {
    laplace_proof_expr_id_t expr_id;
} laplace_proof_search_goal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_search_goal_t) == 4u,
                       "search goal must be 4 bytes");

typedef struct laplace_proof_search_obligation {
    laplace_proof_search_goal_t goal;
    uint32_t                    depth;
    laplace_proof_assertion_id_t source_assertion;
    uint32_t                    source_hyp_index;
} laplace_proof_search_obligation_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_search_obligation_t) == 16u,
                       "search obligation must be 16 bytes");

typedef struct laplace_proof_search_state {
    laplace_proof_search_goal_t        root_goal;
    laplace_proof_search_obligation_t  obligations[LAPLACE_PROOF_SEARCH_MAX_OBLIGATIONS];
    uint32_t                           obligation_count;
    uint32_t                           parent_state_id;
    uint32_t                           depth;
    uint32_t                           step_count;
    laplace_proof_assertion_id_t       last_assertion;
    laplace_proof_search_state_status_t status;
    uint16_t                           branch_id;
    uint16_t                           branch_gen;
    uint32_t                           epoch_id;
} laplace_proof_search_state_t;

typedef struct laplace_proof_search_subst_entry {
    laplace_proof_symbol_id_t variable;
    laplace_proof_symbol_id_t typecode;
    uint32_t                  token_offset;
    uint32_t                  token_count;
} laplace_proof_search_subst_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_search_subst_entry_t) == 16u,
                       "search subst entry must be 16 bytes");

typedef struct laplace_proof_search_try_result {
    laplace_proof_search_status_t status;
    uint32_t                      subgoal_count;
    laplace_proof_search_goal_t   subgoals[LAPLACE_PROOF_SEARCH_MAX_SUBGOALS];
    laplace_proof_assertion_id_t  assertion_id;
    uint32_t                      detail_a;
    uint32_t                      detail_b;
} laplace_proof_search_try_result_t;

typedef struct laplace_proof_search_index_key {
    laplace_proof_symbol_id_t  typecode;
    uint32_t                   body_token_count;
    laplace_proof_symbol_id_t  first_body_token;
    uint32_t                   mand_var_count;
} laplace_proof_search_index_key_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_search_index_key_t) == 16u,
                       "index key must be 16 bytes");

typedef struct laplace_proof_search_index_entry {
    laplace_proof_search_index_key_t key;
    laplace_proof_assertion_id_t     assertion_id;
} laplace_proof_search_index_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_search_index_entry_t) == 20u,
                       "index entry must be 20 bytes");

typedef struct laplace_proof_search_candidate_buf {
    laplace_proof_assertion_id_t ids[LAPLACE_PROOF_SEARCH_MAX_CANDIDATES];
    uint32_t                     count;
    uint32_t                     total_matches;
    bool                         truncated;
} laplace_proof_search_candidate_buf_t;

typedef struct laplace_proof_search_scratch {
    laplace_proof_search_subst_entry_t subst[LAPLACE_PROOF_SEARCH_MAX_SUBST_VARS];
    uint32_t                           subst_count;
    laplace_proof_symbol_id_t          tokens[LAPLACE_PROOF_SEARCH_MAX_INST_TOKENS];
    uint32_t                           token_used;
} laplace_proof_search_scratch_t;

typedef struct laplace_proof_search_index {
    laplace_proof_search_index_entry_t entries[LAPLACE_PROOF_SEARCH_MAX_INDEX_ENTRIES];
    uint32_t                           count;
    bool                               built;
} laplace_proof_search_index_t;

typedef struct laplace_proof_search_context {
    const laplace_proof_store_t*        store;
    laplace_proof_search_index_t*       index;
    laplace_proof_search_scratch_t*     scratch;
    bool                                initialized;
} laplace_proof_search_context_t;

laplace_error_t laplace_proof_search_init(
    laplace_proof_search_context_t* ctx,
    const laplace_proof_store_t* store,
    laplace_proof_search_index_t* index,
    laplace_proof_search_scratch_t* scratch);

void laplace_proof_search_reset(laplace_proof_search_context_t* ctx);

laplace_error_t laplace_proof_search_build_index(
    laplace_proof_search_context_t* ctx);

laplace_error_t laplace_proof_search_query_candidates(
    laplace_proof_search_context_t* ctx,
    laplace_proof_expr_id_t goal_expr_id,
    laplace_proof_search_candidate_buf_t* buf);

laplace_error_t laplace_proof_search_query_candidates_typecode(
    laplace_proof_search_context_t* ctx,
    laplace_proof_symbol_id_t typecode,
    laplace_proof_search_candidate_buf_t* buf);

laplace_error_t laplace_proof_search_try_assertion(
    laplace_proof_search_context_t* ctx,
    laplace_proof_expr_id_t goal_expr_id,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_search_try_result_t* result);

laplace_error_t laplace_proof_search_state_init(
    laplace_proof_search_state_t* state,
    laplace_proof_expr_id_t root_goal_expr_id);

laplace_error_t laplace_proof_search_state_expand(
    laplace_proof_search_context_t* ctx,
    laplace_proof_search_state_t* state,
    uint32_t obligation_index,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_search_try_result_t* result);

uint32_t laplace_proof_search_index_count(
    const laplace_proof_search_context_t* ctx);

const char* laplace_proof_search_status_string(
    laplace_proof_search_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_PROOF_SEARCH_H */
