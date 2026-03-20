#ifndef LAPLACE_PROOF_ARTIFACT_H
#define LAPLACE_PROOF_ARTIFACT_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/errors.h"
#include "laplace/proof.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum laplace_proof_validation_error {
    LAPLACE_PROOF_VALIDATE_OK                      = 0u,
    LAPLACE_PROOF_VALIDATE_NULL_STORE              = 1u,
    LAPLACE_PROOF_VALIDATE_STORE_NOT_INITIALIZED   = 2u,
    LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_ID       = 3u,
    LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_KIND     = 4u,
    LAPLACE_PROOF_VALIDATE_INVALID_EXPR_ID         = 5u,
    LAPLACE_PROOF_VALIDATE_INVALID_TYPECODE        = 6u,
    LAPLACE_PROOF_VALIDATE_INVALID_TOKEN           = 7u,
    LAPLACE_PROOF_VALIDATE_TOKEN_SPAN_OVERFLOW     = 8u,
    LAPLACE_PROOF_VALIDATE_INVALID_FRAME_ID        = 9u,
    LAPLACE_PROOF_VALIDATE_INVALID_PARENT_FRAME    = 10u,
    LAPLACE_PROOF_VALIDATE_INVALID_HYP_ID          = 11u,
    LAPLACE_PROOF_VALIDATE_INVALID_HYP_KIND        = 12u,
    LAPLACE_PROOF_VALIDATE_INVALID_HYP_EXPR        = 13u,
    LAPLACE_PROOF_VALIDATE_INVALID_HYP_FRAME       = 14u,
    LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_ID    = 15u,
    LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_KIND  = 16u,
    LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_FRAME = 17u,
    LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_EXPR  = 18u,
    LAPLACE_PROOF_VALIDATE_INVALID_DV_PAIR         = 19u,
    LAPLACE_PROOF_VALIDATE_DV_NOT_VARIABLE         = 20u,
    LAPLACE_PROOF_VALIDATE_DV_SAME_VARIABLE        = 21u,
    LAPLACE_PROOF_VALIDATE_DV_NOT_NORMALIZED       = 22u,
    LAPLACE_PROOF_VALIDATE_INVALID_THEOREM_ID      = 23u,
    LAPLACE_PROOF_VALIDATE_INVALID_THEOREM_ASSERTION = 24u,
    LAPLACE_PROOF_VALIDATE_INVALID_STEP_KIND       = 25u,
    LAPLACE_PROOF_VALIDATE_INVALID_STEP_REF        = 26u,
    LAPLACE_PROOF_VALIDATE_STEP_SPAN_OVERFLOW      = 27u,
    LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW       = 28u,
    LAPLACE_PROOF_VALIDATE_DV_SPAN_OVERFLOW        = 29u,
    LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_OVERFLOW  = 30u,
    LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_NOT_VARIABLE = 31u,
    LAPLACE_PROOF_VALIDATE_COUNT_                   = 32u
} laplace_proof_validation_error_t;

#define LAPLACE_PROOF_VALIDATE_MAX_ERRORS 32u

typedef struct laplace_proof_validation_result {
    laplace_proof_validation_error_t errors[LAPLACE_PROOF_VALIDATE_MAX_ERRORS];
    uint32_t error_indices[LAPLACE_PROOF_VALIDATE_MAX_ERRORS]; /**< index of offending item */
    uint32_t error_count;
    bool     valid;
} laplace_proof_validation_result_t;

void laplace_proof_validate_symbols(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_expressions(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_frames(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_hypotheses(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_assertions(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_dv_pairs(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_theorems(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

void laplace_proof_validate_all(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result);

const char* laplace_proof_validation_error_string(
    laplace_proof_validation_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_PROOF_ARTIFACT_H */
