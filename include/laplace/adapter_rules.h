#ifndef LAPLACE_ADAPTER_RULES_H
#define LAPLACE_ADAPTER_RULES_H

#include "laplace/adapter.h"
#include "laplace/exact.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- adapter term (8 bytes) ----------
 *
 * Mirror of laplace_exact_term_t using only fixed-width integer fields.
 * kind: LAPLACE_EXACT_TERM_VARIABLE (1) or LAPLACE_EXACT_TERM_CONSTANT (2).
 * value: variable ID (must fit uint16_t) or entity ID (uint32_t).
 */
typedef struct laplace_adapter_term {
    uint32_t kind;      /* 0 - 3 */
    uint32_t value;     /* 4 - 7 */
} laplace_adapter_term_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_term_t) == 8u,
                       "laplace_adapter_term_t must be exactly 8 bytes");

/* ---------- adapter literal (68 bytes) ---------- */

typedef struct laplace_adapter_literal {
    uint16_t predicate_id;      /*  0 -  1 */
    uint8_t  arity;             /*  2      */
    uint8_t  reserved;          /*  3      */
    laplace_adapter_term_t terms[LAPLACE_EXACT_MAX_ARITY]; /*  4 - 67 */
} laplace_adapter_literal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_literal_t) ==
                       4u + LAPLACE_EXACT_MAX_ARITY * sizeof(laplace_adapter_term_t),
                       "laplace_adapter_literal_t size mismatch");

/* ---------- compiled rule artifact (628 bytes) ----------
 *
 * Self-contained normalized rule produced by an external compiler.
 * Contains head literal + up to MAX_RULE_BODY_LITERALS body literals.
 * Compiler provenance fields are optional metadata (may be zero).
 */
typedef struct laplace_adapter_rule_artifact {
    uint32_t abi_version;       /*   0 -   3 */
    uint32_t body_count;        /*   4 -   7 */
    uint32_t compiler_id;       /*   8 -  11  (optional compiler provenance) */
    uint32_t compiler_version;  /*  12 -  15  (optional compiler provenance) */
    laplace_adapter_literal_t head;
    laplace_adapter_literal_t body[LAPLACE_EXACT_MAX_RULE_BODY_LITERALS];
} laplace_adapter_rule_artifact_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_rule_artifact_t) ==
                       16u + sizeof(laplace_adapter_literal_t) *
                             (1u + LAPLACE_EXACT_MAX_RULE_BODY_LITERALS),
                       "laplace_adapter_rule_artifact_t size mismatch");

/* ---------- rule import result (24 bytes) ---------- */

typedef struct laplace_adapter_rule_import_result {
    laplace_adapter_status_t status;        /*  0 -  3 */
    uint32_t rule_id;                       /*  4 -  7 */
    uint32_t validation_error;              /*  8 - 11  (laplace_exact_rule_validation_error_t) */
    uint32_t error_literal_index;           /* 12 - 15 */
    uint32_t error_term_index;              /* 16 - 19 */
    uint32_t reserved;                      /* 20 - 23 */
} laplace_adapter_rule_import_result_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_adapter_rule_import_result_t) == 24u,
                       "laplace_adapter_rule_import_result_t must be exactly 24 bytes");

/*
 * Validate the adapter-level format of a compiled rule artifact without
 * touching the kernel store.  Checks ABI version, body count bounds,
 * term kind validity, and variable-value range.
 */
laplace_adapter_status_t laplace_adapter_validate_rule_artifact(
    const laplace_adapter_rule_artifact_t* artifact);

/*
 * Import a compiled rule artifact into the exact symbolic store.
 * Performs adapter-level validation, converts to internal rule
 * descriptor, then delegates to laplace_exact_add_rule which runs
 * full kernel validation (predicate declarations, arity, constant
 * registration, head-variable safety, etc.).
 *
 * The result struct is always filled, even on failure.
 */
laplace_adapter_status_t laplace_adapter_import_rule(
    laplace_exact_store_t* store,
    const laplace_adapter_rule_artifact_t* artifact,
    laplace_adapter_rule_import_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_ADAPTER_RULES_H */
