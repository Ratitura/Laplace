#ifndef LAPLACE_PROOF_VERIFY_H
#define LAPLACE_PROOF_VERIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/proof.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum laplace_proof_verify_status {
    LAPLACE_PROOF_VERIFY_OK                        = 0u,

    LAPLACE_PROOF_VERIFY_INVALID_ARTIFACT          = 1u,

    LAPLACE_PROOF_VERIFY_INVALID_STEP_REF          = 2u,

    LAPLACE_PROOF_VERIFY_STACK_UNDERFLOW            = 3u,

    LAPLACE_PROOF_VERIFY_STACK_OVERFLOW             = 4u,

    LAPLACE_PROOF_VERIFY_SUBSTITUTION_CONFLICT      = 5u,

    LAPLACE_PROOF_VERIFY_TYPECODE_MISMATCH          = 6u,

    LAPLACE_PROOF_VERIFY_ESSENTIAL_HYP_MISMATCH     = 7u,

    LAPLACE_PROOF_VERIFY_DV_VIOLATION               = 8u,

    LAPLACE_PROOF_VERIFY_FINAL_MISMATCH             = 9u,

    LAPLACE_PROOF_VERIFY_RESOURCE_LIMIT             = 10u,

    LAPLACE_PROOF_VERIFY_INTERNAL_INVARIANT         = 11u,

    LAPLACE_PROOF_VERIFY_STATUS_COUNT_              = 12u
} laplace_proof_verify_status_t;

typedef struct laplace_proof_verify_result {
    laplace_proof_verify_status_t status;

    uint32_t                      failure_step;

    laplace_proof_assertion_id_t  failure_assertion;

    laplace_proof_hyp_id_t        failure_hyp;

    uint32_t                      detail_a;
    uint32_t                      detail_b;

    uint32_t                      steps_processed;
} laplace_proof_verify_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_verify_result_t) == 28u,
                       "verify result must be 28 bytes");

enum {
    LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS = 16384u,

    LAPLACE_PROOF_VERIFY_MAX_SUBST_VARS  = 256u
};

typedef struct laplace_proof_verify_subst {
    laplace_proof_symbol_id_t variable;      /**< variable being substituted */
    laplace_proof_symbol_id_t typecode;      /**< required typecode from $f */
    uint32_t                  token_offset;  /**< offset into scratch token buf */
    uint32_t                  token_count;   /**< number of tokens */
} laplace_proof_verify_subst_t;

typedef struct laplace_proof_verify_context {
    const laplace_proof_store_t* store;

    laplace_proof_expr_id_t      stack[LAPLACE_PROOF_MAX_STACK_DEPTH];
    uint32_t                     stack_depth;

    laplace_proof_verify_subst_t subst[LAPLACE_PROOF_VERIFY_MAX_SUBST_VARS];
    uint32_t                     subst_count;

    laplace_proof_symbol_id_t    scratch_tokens[LAPLACE_PROOF_VERIFY_MAX_INST_TOKENS];
    uint32_t                     scratch_token_used;

    uint64_t                     total_steps;
    uint64_t                     total_assertions_applied;
    uint64_t                     total_hyps_pushed;

    bool                         initialized;
} laplace_proof_verify_context_t;

laplace_error_t laplace_proof_verify_init(
    laplace_proof_verify_context_t* ctx,
    const laplace_proof_store_t* store);

void laplace_proof_verify_reset(laplace_proof_verify_context_t* ctx);

laplace_error_t laplace_proof_verify_theorem(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_theorem_id_t theorem_id,
    laplace_proof_verify_result_t* result);

laplace_error_t laplace_proof_verify_apply_assertion(
    laplace_proof_verify_context_t* ctx,
    laplace_proof_assertion_id_t assertion_id,
    laplace_proof_verify_result_t* result,
    uint32_t step_index);

const char* laplace_proof_verify_status_string(
    laplace_proof_verify_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_PROOF_VERIFY_H */
