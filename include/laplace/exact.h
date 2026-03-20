#ifndef LAPLACE_EXACT_H
#define LAPLACE_EXACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laplace/arena.h"
#include "laplace/entity.h"
#include "laplace/errors.h"
#include "laplace/types.h"

typedef struct laplace_observe_context laplace_observe_context_t;

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LAPLACE_EXACT_MAX_PREDICATES = 128u,
    LAPLACE_EXACT_MAX_FACTS = 2048u,
    LAPLACE_EXACT_FACT_INTERN_SLOTS = 4096u,
    LAPLACE_EXACT_MAX_ARITY = 8u,
    LAPLACE_EXACT_MAX_RULES = 512u,
    LAPLACE_EXACT_MAX_RULE_BODY_LITERALS = 8u,
    LAPLACE_EXACT_MAX_RULE_LITERALS_TOTAL = LAPLACE_EXACT_MAX_RULES * LAPLACE_EXACT_MAX_RULE_BODY_LITERALS,
    LAPLACE_EXACT_MAX_RULE_VARIABLES = 32u,
    LAPLACE_EXACT_MAX_PROVENANCE_RECORDS = 2048u,
    LAPLACE_EXACT_MAX_PROVENANCE_PARENTS = 8u,
    LAPLACE_EXACT_MAX_PROVENANCE_PARENTS_TOTAL = LAPLACE_EXACT_MAX_FACTS * LAPLACE_EXACT_MAX_PROVENANCE_PARENTS
};

LAPLACE_STATIC_ASSERT(LAPLACE_EXACT_MAX_PREDICATES < UINT16_MAX,
                       "predicate count must fit in laplace_predicate_id_t");
LAPLACE_STATIC_ASSERT(LAPLACE_EXACT_MAX_ARITY > 0u,
                       "exact arity bound must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_EXACT_FACT_INTERN_SLOTS >= LAPLACE_EXACT_MAX_FACTS,
                       "fact interning table must hold at least max facts");
LAPLACE_STATIC_ASSERT(LAPLACE_EXACT_MAX_RULE_BODY_LITERALS > 0u,
                       "rule body literal bound must be non-zero");
LAPLACE_STATIC_ASSERT(LAPLACE_EXACT_MAX_PROVENANCE_PARENTS > 0u,
                       "provenance parent bound must be non-zero");

typedef enum laplace_exact_predicate_flags {
    LAPLACE_EXACT_PREDICATE_FLAG_NONE = 0u,
    LAPLACE_EXACT_PREDICATE_FLAG_RESERVED = 1u << 0
} laplace_exact_predicate_flags_t;

typedef enum laplace_exact_fact_flags {
    LAPLACE_EXACT_FACT_FLAG_NONE = 0u,
    LAPLACE_EXACT_FACT_FLAG_ASSERTED = 1u << 0,
    LAPLACE_EXACT_FACT_FLAG_DERIVED = 1u << 1,
    LAPLACE_EXACT_FACT_FLAG_BRANCH_LOCAL = 1u << 2,
    LAPLACE_EXACT_FACT_FLAG_RETIRED = 1u << 3,
    LAPLACE_EXACT_FACT_FLAG_COMMITTED = 1u << 4
} laplace_exact_fact_flags_t;

typedef enum laplace_exact_term_kind {
    LAPLACE_EXACT_TERM_INVALID = 0u,
    LAPLACE_EXACT_TERM_VARIABLE = 1u,
    LAPLACE_EXACT_TERM_CONSTANT = 2u
} laplace_exact_term_kind_t;

typedef enum laplace_exact_provenance_kind {
    LAPLACE_EXACT_PROVENANCE_INVALID = 0u,
    LAPLACE_EXACT_PROVENANCE_ASSERTED = 1u,
    LAPLACE_EXACT_PROVENANCE_DERIVED = 2u
} laplace_exact_provenance_kind_t;

typedef enum laplace_exact_rule_status {
    LAPLACE_EXACT_RULE_STATUS_INVALID = 0u,
    LAPLACE_EXACT_RULE_STATUS_VALID = 1u
} laplace_exact_rule_status_t;

typedef enum laplace_exact_rule_validation_error {
    LAPLACE_EXACT_RULE_VALIDATION_OK = 0u,
    LAPLACE_EXACT_RULE_VALIDATION_NULL_RULE = 1u,
    LAPLACE_EXACT_RULE_VALIDATION_BODY_REQUIRED = 2u,
    LAPLACE_EXACT_RULE_VALIDATION_BODY_TOO_LARGE = 3u,
    LAPLACE_EXACT_RULE_VALIDATION_HEAD_PREDICATE_UNDECLARED = 4u,
    LAPLACE_EXACT_RULE_VALIDATION_BODY_PREDICATE_UNDECLARED = 5u,
    LAPLACE_EXACT_RULE_VALIDATION_ARITY_MISMATCH = 6u,
    LAPLACE_EXACT_RULE_VALIDATION_TERM_KIND_INVALID = 7u,
    LAPLACE_EXACT_RULE_VALIDATION_VARIABLE_OUT_OF_RANGE = 8u,
    LAPLACE_EXACT_RULE_VALIDATION_HEAD_VARIABLE_MISSING_FROM_BODY = 9u,
    LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_REGISTERED = 10u,
    LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_ALIVE = 11u,
    LAPLACE_EXACT_RULE_VALIDATION_CONSTANT_NOT_CONSTANT = 12u,
    LAPLACE_EXACT_RULE_VALIDATION_BODY_LITERALS_NULL = 13u
} laplace_exact_rule_validation_error_t;

typedef struct laplace_exact_predicate_desc {
    uint8_t  arity;
    uint32_t flags;
    uint32_t fact_capacity;
} laplace_exact_predicate_desc_t;

typedef struct laplace_exact_term {
    laplace_exact_term_kind_t kind;
    union {
        laplace_exact_var_id_t variable;
        laplace_entity_id_t    constant;
    } value;
} laplace_exact_term_t;

typedef struct laplace_exact_literal {
    laplace_predicate_id_t predicate;
    uint8_t                arity;
    laplace_exact_term_t   terms[LAPLACE_EXACT_MAX_ARITY];
} laplace_exact_literal_t;

typedef struct laplace_exact_rule_desc {
    laplace_exact_literal_t head;
    const laplace_exact_literal_t* body_literals;
    uint32_t body_count;
} laplace_exact_rule_desc_t;

typedef struct laplace_exact_rule_validation_result {
    laplace_exact_rule_validation_error_t error;
    uint32_t literal_index;
    uint32_t term_index;
} laplace_exact_rule_validation_result_t;

typedef struct laplace_exact_provenance_desc {
    laplace_exact_provenance_kind_t kind;
    laplace_rule_id_t               source_rule_id;
    const laplace_entity_id_t*      parent_facts;
    uint32_t                        parent_count;
    uint32_t                        reserved_epoch;
    uint32_t                        reserved_branch;
} laplace_exact_provenance_desc_t;

typedef struct laplace_exact_predicate_view {
    const laplace_exact_fact_row_t* rows;
    uint32_t                        count;
} laplace_exact_predicate_view_t;

typedef struct laplace_exact_fact {
    laplace_entity_id_t     entity;
    laplace_predicate_id_t  predicate;
    uint8_t                 arity;
    uint8_t                 reserved[3];
    laplace_provenance_id_t provenance;
    laplace_branch_id_t     branch_id;
    laplace_branch_generation_t branch_generation;
    laplace_epoch_id_t      create_epoch;
    laplace_epoch_id_t      retire_epoch;
    uint32_t                flags;
    laplace_entity_id_t     args[LAPLACE_EXACT_MAX_ARITY];
} laplace_exact_fact_t;

typedef struct laplace_exact_rule {
    laplace_rule_id_t              id;
    laplace_exact_literal_t        head;
    uint32_t                       body_offset;
    uint32_t                       body_count;
    uint32_t                       flags;
    laplace_exact_rule_status_t    status;
    uint32_t                       reserved;
} laplace_exact_rule_t;

typedef struct laplace_exact_provenance_record {
    laplace_exact_provenance_kind_t kind;
    laplace_rule_id_t               source_rule_id;
    uint32_t                        parent_offset;
    uint32_t                        parent_count;
    laplace_tick_t                  tick;
    uint32_t                        reserved_epoch;
    uint32_t                        reserved_branch;
} laplace_exact_provenance_record_t;

typedef struct laplace_exact_store {
    laplace_entity_pool_t* entity_pool;

    laplace_observe_context_t* observe;

    uint32_t predicate_count;
    uint32_t predicate_scan_rows_used;
    uint32_t fact_count;
    uint32_t rule_count;
    uint32_t rule_literal_count;
    uint32_t provenance_count;
    uint32_t provenance_parent_count;
    laplace_tick_t next_tick;

    uint8_t*                predicate_declared;
    uint8_t*                predicate_arities;
    uint32_t*               predicate_flags;
    uint32_t*               predicate_fact_capacities;
    uint32_t*               predicate_row_offsets;
    uint32_t*               predicate_row_counts;
    laplace_exact_fact_row_t* predicate_rows;

    laplace_exact_fact_t* facts;
    uint32_t*             fact_hashes;
    laplace_exact_fact_row_t* fact_slots;

    laplace_exact_rule_t*    rules;
    laplace_exact_literal_t* rule_literals;

    laplace_exact_provenance_record_t* provenance_records;
    laplace_entity_id_t*              provenance_parents;
} laplace_exact_store_t;

laplace_error_t laplace_exact_store_init(laplace_exact_store_t* store,
                                          laplace_arena_t* arena,
                                          laplace_entity_pool_t* entity_pool);

laplace_error_t laplace_exact_register_predicate(laplace_exact_store_t* store,
                                                  laplace_predicate_id_t predicate_id,
                                                  const laplace_exact_predicate_desc_t* desc);

bool laplace_exact_predicate_is_declared(const laplace_exact_store_t* store,
                                          laplace_predicate_id_t predicate_id);

uint8_t laplace_exact_predicate_arity(const laplace_exact_store_t* store,
                                       laplace_predicate_id_t predicate_id);

laplace_error_t laplace_exact_register_constant(laplace_exact_store_t* store,
                                                 laplace_entity_handle_t constant,
                                                 laplace_exact_type_id_t type_id,
                                                 uint32_t flags);

bool laplace_exact_is_constant_entity(const laplace_exact_store_t* store,
                                       laplace_entity_id_t entity_id);

laplace_error_t laplace_exact_insert_provenance(laplace_exact_store_t* store,
                                                 const laplace_exact_provenance_desc_t* desc,
                                                 laplace_provenance_id_t* out_provenance_id);

const laplace_exact_provenance_record_t* laplace_exact_get_provenance(const laplace_exact_store_t* store,
                                                                       laplace_provenance_id_t provenance_id);

const laplace_entity_id_t* laplace_exact_get_provenance_parents(const laplace_exact_store_t* store,
                                                                 laplace_provenance_id_t provenance_id,
                                                                 uint32_t* out_parent_count);

laplace_error_t laplace_exact_assert_fact(laplace_exact_store_t* store,
                                           laplace_predicate_id_t predicate_id,
                                           const laplace_entity_handle_t* args,
                                           uint32_t arg_count,
                                           laplace_provenance_id_t provenance_id,
                                           uint32_t flags,
                                           laplace_exact_fact_row_t* out_fact_row,
                                           laplace_entity_handle_t* out_fact_entity,
                                           bool* out_inserted);

laplace_exact_fact_row_t laplace_exact_find_fact(const laplace_exact_store_t* store,
                                                  laplace_predicate_id_t predicate_id,
                                                  const laplace_entity_id_t* args,
                                                  uint32_t arg_count);

laplace_exact_fact_row_t laplace_exact_find_fact_in_branch(const laplace_exact_store_t* store,
                                                            laplace_branch_handle_t branch,
                                                            laplace_predicate_id_t predicate_id,
                                                            const laplace_entity_id_t* args,
                                                            uint32_t arg_count);

laplace_error_t laplace_exact_assert_fact_in_branch(laplace_exact_store_t* store,
                                                     laplace_branch_handle_t branch,
                                                     laplace_epoch_id_t create_epoch,
                                                     laplace_predicate_id_t predicate_id,
                                                     const laplace_entity_handle_t* args,
                                                     uint32_t arg_count,
                                                     laplace_provenance_id_t provenance_id,
                                                     uint32_t flags,
                                                     laplace_exact_fact_row_t* out_fact_row,
                                                     laplace_entity_handle_t* out_fact_entity,
                                                     bool* out_inserted);

const laplace_exact_fact_t* laplace_exact_get_fact(const laplace_exact_store_t* store,
                                                    laplace_exact_fact_row_t fact_row);

bool laplace_exact_fact_is_active(const laplace_exact_fact_t* fact);

bool laplace_exact_fact_is_committed(const laplace_exact_fact_t* fact);

bool laplace_exact_fact_visible_to_branch(const laplace_exact_fact_t* fact,
                                           laplace_branch_handle_t branch);

laplace_error_t laplace_exact_promote_fact(laplace_exact_store_t* store,
                                            laplace_exact_fact_row_t fact_row);

laplace_error_t laplace_exact_retire_fact(laplace_exact_store_t* store,
                                           laplace_exact_fact_row_t fact_row,
                                           laplace_epoch_id_t retire_epoch);

laplace_exact_predicate_view_t laplace_exact_predicate_rows(const laplace_exact_store_t* store,
                                                             laplace_predicate_id_t predicate_id);

laplace_exact_rule_validation_result_t laplace_exact_validate_rule(const laplace_exact_store_t* store,
                                                                    const laplace_exact_rule_desc_t* rule_desc);

laplace_error_t laplace_exact_add_rule(laplace_exact_store_t* store,
                                        const laplace_exact_rule_desc_t* rule_desc,
                                        laplace_rule_id_t* out_rule_id,
                                        laplace_exact_rule_validation_result_t* out_validation);

const laplace_exact_rule_t* laplace_exact_get_rule(const laplace_exact_store_t* store,
                                                    laplace_rule_id_t rule_id);

const laplace_exact_literal_t* laplace_exact_rule_body_literals(const laplace_exact_store_t* store,
                                                                 const laplace_exact_rule_t* rule,
                                                                 uint32_t* out_count);

const char* laplace_exact_rule_validation_error_string(laplace_exact_rule_validation_error_t error);

void laplace_exact_bind_observe(laplace_exact_store_t* store,
                                laplace_observe_context_t* observe);

#ifdef __cplusplus
}
#endif

#endif