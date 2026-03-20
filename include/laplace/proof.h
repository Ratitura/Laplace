#ifndef LAPLACE_PROOF_H
#define LAPLACE_PROOF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/assert.h"
#include "laplace/errors.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t laplace_proof_symbol_id_t;
typedef uint32_t laplace_proof_expr_id_t;
typedef uint32_t laplace_proof_frame_id_t;
typedef uint32_t laplace_proof_hyp_id_t;
typedef uint32_t laplace_proof_assertion_id_t;
typedef uint32_t laplace_proof_theorem_id_t;
typedef uint32_t laplace_proof_dv_id_t;
typedef uint32_t laplace_proof_subst_id_t;
typedef uint32_t laplace_proof_state_id_t;

enum {
    LAPLACE_PROOF_SYMBOL_ID_INVALID    = 0u,
    LAPLACE_PROOF_EXPR_ID_INVALID      = 0u,
    LAPLACE_PROOF_FRAME_ID_INVALID     = 0u,
    LAPLACE_PROOF_HYP_ID_INVALID       = 0u,
    LAPLACE_PROOF_ASSERTION_ID_INVALID = 0u,
    LAPLACE_PROOF_THEOREM_ID_INVALID   = 0u,
    LAPLACE_PROOF_DV_ID_INVALID        = 0u,
    LAPLACE_PROOF_SUBST_ID_INVALID     = 0u,
    LAPLACE_PROOF_STATE_ID_INVALID     = 0u
};

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_symbol_id_t) == 4u,
                       "laplace_proof_symbol_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_expr_id_t) == 4u,
                       "laplace_proof_expr_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_frame_id_t) == 4u,
                       "laplace_proof_frame_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_hyp_id_t) == 4u,
                       "laplace_proof_hyp_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_assertion_id_t) == 4u,
                       "laplace_proof_assertion_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_theorem_id_t) == 4u,
                       "laplace_proof_theorem_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_dv_id_t) == 4u,
                       "laplace_proof_dv_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_subst_id_t) == 4u,
                       "laplace_proof_subst_id_t must be 32-bit");
LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_state_id_t) == 4u,
                       "laplace_proof_state_id_t must be 32-bit");

enum {
    LAPLACE_PROOF_MAX_SYMBOLS       = 16384u,
    LAPLACE_PROOF_MAX_EXPRESSIONS   = 65536u,
    LAPLACE_PROOF_MAX_TOKEN_POOL    = 1048576u, /* 1M tokens */
    LAPLACE_PROOF_MAX_FRAMES        = 16384u,
    LAPLACE_PROOF_MAX_HYPOTHESES    = 65536u,
    LAPLACE_PROOF_MAX_ASSERTIONS    = 65536u,
    LAPLACE_PROOF_MAX_DV_PAIRS      = 131072u,
    LAPLACE_PROOF_MAX_THEOREMS      = 65536u,
    LAPLACE_PROOF_MAX_PROOF_STEPS   = 2097152u, /* 2M steps */
    LAPLACE_PROOF_MAX_SUBST_ENTRIES = 4096u,
    LAPLACE_PROOF_MAX_STACK_DEPTH   = 4096u,

    LAPLACE_PROOF_MAX_EXPR_TOKENS   = 4096u
};

LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_SYMBOLS > 0u,
                       "proof symbol capacity must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_EXPRESSIONS > 0u,
                       "proof expression capacity must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_TOKEN_POOL > 0u,
                       "proof token pool capacity must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_FRAMES > 0u,
                       "proof frame capacity must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_HYPOTHESES > 0u,
                       "proof hypothesis capacity must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_PROOF_MAX_ASSERTIONS > 0u,
                       "proof assertion capacity must be non-zero");

typedef enum laplace_proof_symbol_kind {
    LAPLACE_PROOF_SYMBOL_INVALID  = 0u,
    LAPLACE_PROOF_SYMBOL_CONSTANT = 1u,
    LAPLACE_PROOF_SYMBOL_VARIABLE = 2u,
    LAPLACE_PROOF_SYMBOL_KIND_COUNT_ = 3u
} laplace_proof_symbol_kind_t;

typedef struct laplace_proof_symbol {
    laplace_proof_symbol_kind_t kind;          /**< constant or variable */
    uint32_t                    compiler_index; /**< external compiler-assigned index */
    uint32_t                    flags;          /**< reserved flags */
} laplace_proof_symbol_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_symbol_t) == 12u,
                       "laplace_proof_symbol_t must be 12 bytes");

typedef struct laplace_proof_expr {
    laplace_proof_symbol_id_t typecode;     /**< typecode symbol ID (a constant) */
    uint32_t                  token_offset;  /**< start index in the token pool */
    uint32_t                  token_count;   /**< number of body tokens */
    uint32_t                  hash;          /**< structural hash for dedup/lookup */
} laplace_proof_expr_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_expr_t) == 16u,
                       "laplace_proof_expr_t must be 16 bytes");

typedef enum laplace_proof_hyp_kind {
    LAPLACE_PROOF_HYP_INVALID   = 0u,
    LAPLACE_PROOF_HYP_FLOATING  = 1u, /**< typed variable declaration ($f) */
    LAPLACE_PROOF_HYP_ESSENTIAL = 2u, /**< exact expression requirement ($e) */
    LAPLACE_PROOF_HYP_KIND_COUNT_ = 3u
} laplace_proof_hyp_kind_t;

typedef struct laplace_proof_hyp {
    laplace_proof_hyp_kind_t    kind;       /**< floating or essential */
    laplace_proof_expr_id_t     expr_id;    /**< expression for this hypothesis */
    laplace_proof_frame_id_t    frame_id;   /**< owning frame */
    uint32_t                    order;      /**< order within frame (deterministic) */
} laplace_proof_hyp_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_hyp_t) == 16u,
                       "laplace_proof_hyp_t must be 16 bytes");

typedef struct laplace_proof_dv_pair {
    laplace_proof_symbol_id_t var_a;
    laplace_proof_symbol_id_t var_b;
} laplace_proof_dv_pair_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_dv_pair_t) == 8u,
                       "laplace_proof_dv_pair_t must be 8 bytes");

typedef struct laplace_proof_frame {
    laplace_proof_frame_id_t parent_id;     /**< parent frame ID, or INVALID for root */
    uint32_t float_hyp_offset;              /**< start index in hypothesis store (floating) */
    uint32_t float_hyp_count;               /**< number of floating hypotheses */
    uint32_t essential_hyp_offset;          /**< start index in hypothesis store (essential) */
    uint32_t essential_hyp_count;           /**< number of essential hypotheses */
    uint32_t dv_offset;                     /**< start index in DV pair store */
    uint32_t dv_count;                      /**< number of DV pairs in this frame */
    uint32_t mandatory_var_offset;          /**< start index in mandatory var array */
    uint32_t mandatory_var_count;           /**< number of mandatory variables */
} laplace_proof_frame_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_frame_t) == 36u,
                       "laplace_proof_frame_t must be 36 bytes");

typedef enum laplace_proof_assertion_kind {
    LAPLACE_PROOF_ASSERTION_INVALID = 0u,
    LAPLACE_PROOF_ASSERTION_AXIOM   = 1u, /**< axiom ($a) — accepted without proof */
    LAPLACE_PROOF_ASSERTION_THEOREM = 2u, /**< theorem ($p) — requires proof */
    LAPLACE_PROOF_ASSERTION_KIND_COUNT_ = 3u
} laplace_proof_assertion_kind_t;

typedef struct laplace_proof_assertion {
    laplace_proof_assertion_kind_t kind;            /**< axiom or theorem */
    laplace_proof_frame_id_t       frame_id;         /**< associated frame */
    laplace_proof_expr_id_t        conclusion_id;    /**< conclusion expression */
    uint32_t                       mand_float_offset; /**< mandatory floating hyp span start */
    uint32_t                       mand_float_count;  /**< mandatory floating hyp count */
    uint32_t                       mand_ess_offset;   /**< mandatory essential hyp span start */
    uint32_t                       mand_ess_count;    /**< mandatory essential hyp count */
    uint32_t                       dv_offset;         /**< DV pair span start */
    uint32_t                       dv_count;          /**< DV pair count */
    laplace_proof_theorem_id_t     theorem_id;        /**< proof artifact ref (INVALID for axioms) */
    uint32_t                       compiler_index;    /**< external compiler-assigned index */
} laplace_proof_assertion_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_assertion_t) == 44u,
                       "laplace_proof_assertion_t must be 44 bytes");

typedef enum laplace_proof_step_kind {
    LAPLACE_PROOF_STEP_INVALID     = 0u,
    LAPLACE_PROOF_STEP_HYP        = 1u, /**< push hypothesis onto stack */
    LAPLACE_PROOF_STEP_ASSERTION   = 2u, /**< apply assertion */
    LAPLACE_PROOF_STEP_KIND_COUNT_ = 3u
} laplace_proof_step_kind_t;

typedef struct laplace_proof_step {
    laplace_proof_step_kind_t kind;
    uint32_t                  ref_id; /**< hypothesis ID or assertion ID */
} laplace_proof_step_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_step_t) == 8u,
                       "laplace_proof_step_t must be 8 bytes");

typedef struct laplace_proof_theorem {
    laplace_proof_assertion_id_t assertion_id; /**< owning assertion */
    uint32_t                     step_offset;  /**< start index in proof step pool */
    uint32_t                     step_count;   /**< number of proof steps */
    uint32_t                     flags;        /**< compiler metadata flags */
} laplace_proof_theorem_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_theorem_t) == 16u,
                       "laplace_proof_theorem_t must be 16 bytes");

typedef struct laplace_proof_subst_entry {
    laplace_proof_symbol_id_t variable;  /**< variable symbol ID */
    laplace_proof_expr_id_t   expr_id;   /**< substituted expression */
} laplace_proof_subst_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_subst_entry_t) == 8u,
                       "laplace_proof_subst_entry_t must be 8 bytes");

typedef struct laplace_proof_subst_map {
    laplace_proof_subst_entry_t entries[LAPLACE_PROOF_MAX_SUBST_ENTRIES];
    uint32_t                    count;
} laplace_proof_subst_map_t;

typedef struct laplace_proof_stack_entry {
    laplace_proof_expr_id_t expr_id;
} laplace_proof_stack_entry_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_proof_stack_entry_t) == 4u,
                       "laplace_proof_stack_entry_t must be 4 bytes");

typedef struct laplace_proof_state {
    laplace_proof_stack_entry_t stack[LAPLACE_PROOF_MAX_STACK_DEPTH];
    uint32_t                    stack_depth;
    laplace_proof_assertion_id_t current_assertion;
    uint32_t                    step_index;
    uint32_t                    flags;
} laplace_proof_state_t;

typedef struct laplace_proof_verifier_context {
    laplace_proof_state_t   state;
    laplace_proof_subst_map_t subst;
    uint32_t                error_code;
    uint32_t                error_step;
    uint32_t                flags;
    uint32_t                reserved;
} laplace_proof_verifier_context_t;

typedef struct laplace_proof_store {
    laplace_proof_symbol_t*    symbols;
    uint32_t                   symbol_count;

    laplace_proof_symbol_id_t* token_pool;
    uint32_t                   token_pool_used;

    laplace_proof_expr_t*      expressions;
    uint32_t                   expr_count;

    laplace_proof_hyp_t*       hypotheses;
    uint32_t                   hyp_count;

    laplace_proof_frame_t*     frames;
    uint32_t                   frame_count;

    laplace_proof_dv_pair_t*   dv_pairs;
    uint32_t                   dv_pair_count;

    laplace_proof_symbol_id_t* mandatory_vars;
    uint32_t                   mandatory_var_count;
    uint32_t                   mandatory_var_capacity;

    laplace_proof_assertion_t* assertions;
    uint32_t                   assertion_count;

    laplace_proof_theorem_t*   theorems;
    uint32_t                   theorem_count;

    laplace_proof_step_t*      proof_steps;
    uint32_t                   proof_step_count;

    bool                       initialized;
} laplace_proof_store_t;

laplace_error_t laplace_proof_store_init(laplace_proof_store_t* store,
                                          laplace_arena_t* arena);

void laplace_proof_store_reset(laplace_proof_store_t* store);

laplace_error_t laplace_proof_import_symbol(
    laplace_proof_store_t* store,
    laplace_proof_symbol_kind_t kind,
    uint32_t compiler_index,
    uint32_t flags,
    laplace_proof_symbol_id_t* out_id);

const laplace_proof_symbol_t* laplace_proof_get_symbol(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id);

bool laplace_proof_symbol_is_valid(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id);

bool laplace_proof_symbol_is_variable(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id);

bool laplace_proof_symbol_is_constant(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id);

laplace_error_t laplace_proof_import_expr(
    laplace_proof_store_t* store,
    laplace_proof_symbol_id_t typecode,
    const laplace_proof_symbol_id_t* tokens,
    uint32_t token_count,
    laplace_proof_expr_id_t* out_id);

const laplace_proof_expr_t* laplace_proof_get_expr(
    const laplace_proof_store_t* store,
    laplace_proof_expr_id_t id);

laplace_error_t laplace_proof_get_expr_tokens(
    const laplace_proof_store_t* store,
    laplace_proof_expr_id_t id,
    const laplace_proof_symbol_id_t** out_tokens,
    uint32_t* out_count);

typedef struct laplace_proof_frame_desc {
    laplace_proof_frame_id_t  parent_id;     /**< INVALID for root */
    const laplace_proof_symbol_id_t* mandatory_vars;
    uint32_t                  mandatory_var_count;
} laplace_proof_frame_desc_t;

laplace_error_t laplace_proof_import_frame(
    laplace_proof_store_t* store,
    const laplace_proof_frame_desc_t* desc,
    laplace_proof_frame_id_t* out_id);

const laplace_proof_frame_t* laplace_proof_get_frame(
    const laplace_proof_store_t* store,
    laplace_proof_frame_id_t id);

laplace_error_t laplace_proof_import_float_hyp(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_hyp_id_t* out_id);

laplace_error_t laplace_proof_import_essential_hyp(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_hyp_id_t* out_id);

const laplace_proof_hyp_t* laplace_proof_get_hyp(
    const laplace_proof_store_t* store,
    laplace_proof_hyp_id_t id);

laplace_error_t laplace_proof_import_dv_pair(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_symbol_id_t var_a,
    laplace_proof_symbol_id_t var_b,
    laplace_proof_dv_id_t* out_id);

laplace_error_t laplace_proof_get_frame_dv_pairs(
    const laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    const laplace_proof_dv_pair_t** out_pairs,
    uint32_t* out_count);

typedef struct laplace_proof_assertion_desc {
    laplace_proof_assertion_kind_t kind;
    laplace_proof_frame_id_t       frame_id;
    laplace_proof_expr_id_t        conclusion_id;
    uint32_t                       mand_float_offset;
    uint32_t                       mand_float_count;
    uint32_t                       mand_ess_offset;
    uint32_t                       mand_ess_count;
    uint32_t                       dv_offset;
    uint32_t                       dv_count;
    uint32_t                       compiler_index;
} laplace_proof_assertion_desc_t;

laplace_error_t laplace_proof_import_assertion(
    laplace_proof_store_t* store,
    const laplace_proof_assertion_desc_t* desc,
    laplace_proof_assertion_id_t* out_id);

const laplace_proof_assertion_t* laplace_proof_get_assertion(
    const laplace_proof_store_t* store,
    laplace_proof_assertion_id_t id);

typedef struct laplace_proof_theorem_desc {
    laplace_proof_assertion_id_t assertion_id;
    const laplace_proof_step_t*  steps;
    uint32_t                     step_count;
    uint32_t                     flags;
} laplace_proof_theorem_desc_t;

laplace_error_t laplace_proof_import_theorem(
    laplace_proof_store_t* store,
    const laplace_proof_theorem_desc_t* desc,
    laplace_proof_theorem_id_t* out_id);

const laplace_proof_theorem_t* laplace_proof_get_theorem(
    const laplace_proof_store_t* store,
    laplace_proof_theorem_id_t id);

laplace_error_t laplace_proof_get_theorem_steps(
    const laplace_proof_store_t* store,
    laplace_proof_theorem_id_t id,
    const laplace_proof_step_t** out_steps,
    uint32_t* out_count);

uint32_t laplace_proof_symbol_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_expr_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_frame_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_hyp_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_assertion_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_dv_pair_count(const laplace_proof_store_t* store);
uint32_t laplace_proof_theorem_count(const laplace_proof_store_t* store);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_PROOF_H */
