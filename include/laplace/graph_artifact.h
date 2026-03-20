#ifndef LAPLACE_GRAPH_ARTIFACT_H
#define LAPLACE_GRAPH_ARTIFACT_H

#include <stdbool.h>
#include <stdint.h>

#include "laplace/assert.h"
#include "laplace/exact.h"
#include "laplace/graph_profile.h"
#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAPLACE_GRAPH_ARTIFACT_VERSION       1u
#define LAPLACE_GRAPH_ARTIFACT_MAGIC         0x4C415047u  /* "LAPG" */

enum {
    LAPLACE_GRAPH_ARTIFACT_MAX_PREDICATES   = 128u,
    LAPLACE_GRAPH_ARTIFACT_MAX_ENTITIES     = 2048u,
    LAPLACE_GRAPH_ARTIFACT_MAX_FACTS        = 2048u,
    LAPLACE_GRAPH_ARTIFACT_MAX_RULES        = 512u,
    LAPLACE_GRAPH_ARTIFACT_MAX_RULE_BODY    = 8u,
    LAPLACE_GRAPH_ARTIFACT_MAX_ARITY        = 8u,
    LAPLACE_GRAPH_ARTIFACT_MAX_NAME_LEN     = 128u
};

LAPLACE_STATIC_ASSERT(LAPLACE_GRAPH_ARTIFACT_MAX_ARITY == LAPLACE_EXACT_MAX_ARITY,
                       "artifact max arity must match exact max arity");

typedef uint32_t laplace_graph_artifact_flags_t;

enum {
    LAPLACE_GRAPH_ARTIFACT_FLAG_NONE           = 0u,
    LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES      = 1u << 0,
    LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_NAMES      = 1u << 1,
    LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_PROVENANCE = 1u << 2
};

typedef struct laplace_graph_artifact_header {
    uint32_t                          magic;              /*  0 -  3 */
    uint32_t                          version;            /*  4 -  7 */
    laplace_graph_profile_id_t        profile_id;         /*  8      */
    uint8_t                           reserved_u8[3];     /*  9 - 11 */
    laplace_graph_artifact_flags_t    flags;              /* 12 - 15 */
    uint32_t                          predicate_count;    /* 16 - 19 */
    uint32_t                          entity_count;       /* 20 - 23 */
    uint32_t                          fact_count;         /* 24 - 27 */
    uint32_t                          rule_count;         /* 28 - 31 */
    uint32_t                          checksum;           /* 32 - 35 */
    uint32_t                          reserved[7];        /* 36 - 63 */
} laplace_graph_artifact_header_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_header_t) == 64u,
                       "artifact header must be exactly 64 bytes");

typedef uint32_t laplace_graph_pred_flags_t;

enum {
    LAPLACE_GRAPH_PRED_FLAG_NONE     = 0u,
    LAPLACE_GRAPH_PRED_FLAG_BUILTIN  = 1u << 0,
    LAPLACE_GRAPH_PRED_FLAG_SCHEMA   = 1u << 1
};

typedef struct laplace_graph_artifact_predicate {
    uint16_t                     local_id;       /*  0 -  1 : compiler-local predicate ID */
    uint8_t                      arity;          /*  2      : target arity */
    uint8_t                      reserved_u8;    /*  3      */
    laplace_graph_pred_flags_t   flags;          /*  4 -  7 */
    uint32_t                     name_offset;    /*  8 - 11 : byte offset into name blob, or 0 */
    uint32_t                     name_length;    /* 12 - 15 : byte length of name, or 0 */
} laplace_graph_artifact_predicate_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_predicate_t) == 16u,
                       "predicate record must be exactly 16 bytes");

typedef uint32_t laplace_graph_entity_flags_t;

enum {
    LAPLACE_GRAPH_ENTITY_FLAG_NONE     = 0u,
    LAPLACE_GRAPH_ENTITY_FLAG_IRI      = 1u << 0,
    LAPLACE_GRAPH_ENTITY_FLAG_LITERAL  = 1u << 1,
    LAPLACE_GRAPH_ENTITY_FLAG_BLANK    = 1u << 2
};

typedef struct laplace_graph_artifact_entity {
    uint32_t                      local_id;      /*  0 -  3 : compiler-local entity ID */
    laplace_graph_entity_flags_t  flags;         /*  4 -  7 */
    uint32_t                      name_offset;   /*  8 - 11 : byte offset into name blob, or 0 */
    uint32_t                      name_length;   /* 12 - 15 : byte length of name, or 0 */
} laplace_graph_artifact_entity_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_entity_t) == 16u,
                       "entity record must be exactly 16 bytes");

typedef uint32_t laplace_graph_fact_flags_t;

enum {
    LAPLACE_GRAPH_FACT_FLAG_NONE     = 0u,
    LAPLACE_GRAPH_FACT_FLAG_ASSERTED = 1u << 0
};

typedef struct laplace_graph_artifact_fact {
    uint16_t                    predicate_local_id;                          /*  0 -  1 */
    uint8_t                     arg_count;                                   /*  2      */
    uint8_t                     reserved_u8;                                 /*  3      */
    laplace_graph_fact_flags_t  flags;                                       /*  4 -  7 */
    uint32_t                    arg_entity_local_ids[LAPLACE_GRAPH_ARTIFACT_MAX_ARITY]; /*  8 - 39 */
} laplace_graph_artifact_fact_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_fact_t) == 40u,
                       "fact record must be exactly 40 bytes");

typedef struct laplace_graph_artifact_term {
    uint32_t kind;   /**< 0=INVALID, 1=VARIABLE, 2=CONSTANT (entity local ID) */
    uint32_t value;  /**< variable ID or entity local ID */
} laplace_graph_artifact_term_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_term_t) == 8u,
                       "rule term must be exactly 8 bytes");

typedef struct laplace_graph_artifact_literal {
    uint16_t                          predicate_local_id;                         /*  0 -  1 */
    uint8_t                           arity;                                      /*  2      */
    uint8_t                           reserved_u8;                                /*  3      */
    uint32_t                          reserved_u32;                               /*  4 -  7 */
    laplace_graph_artifact_term_t     terms[LAPLACE_GRAPH_ARTIFACT_MAX_ARITY];    /*  8 - 71 */
} laplace_graph_artifact_literal_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_literal_t) == 72u,
                       "rule literal must be exactly 72 bytes");

typedef uint32_t laplace_graph_rule_flags_t;

enum {
    LAPLACE_GRAPH_RULE_FLAG_NONE = 0u
};

typedef struct laplace_graph_artifact_rule {
    uint32_t                          body_count;    /*  0 -  3 */
    laplace_graph_rule_flags_t        flags;         /*  4 -  7 */
    laplace_graph_artifact_literal_t  head;          /*  8 - 79 */
    laplace_graph_artifact_literal_t  body[LAPLACE_GRAPH_ARTIFACT_MAX_RULE_BODY]; /* 80 - 655 */
    uint32_t                          reserved[16];  /* 656 - 719 */
} laplace_graph_artifact_rule_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_rule_t) == 720u,
                       "rule record must be exactly 720 bytes");

typedef uint32_t laplace_graph_artifact_status_t;

enum {
    LAPLACE_GRAPH_ARTIFACT_OK                         = 0u,
    LAPLACE_GRAPH_ARTIFACT_ERR_NULL                   = 1u,
    LAPLACE_GRAPH_ARTIFACT_ERR_BAD_MAGIC              = 2u,
    LAPLACE_GRAPH_ARTIFACT_ERR_BAD_VERSION            = 3u,
    LAPLACE_GRAPH_ARTIFACT_ERR_BAD_PROFILE            = 4u,
    LAPLACE_GRAPH_ARTIFACT_ERR_UNSUPPORTED_PROFILE    = 5u,
    LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_OVERFLOW     = 6u,
    LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_OVERFLOW        = 7u,
    LAPLACE_GRAPH_ARTIFACT_ERR_FACT_OVERFLOW           = 8u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_OVERFLOW           = 9u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULES_WITHOUT_FLAG     = 10u,
    LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_ARITY        = 11u,
    LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_DUPLICATE    = 12u,
    LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_DUPLICATE       = 13u,
    LAPLACE_GRAPH_ARTIFACT_ERR_FACT_PREDICATE_REF     = 14u,
    LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ENTITY_REF        = 15u,
    LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ARITY_MISMATCH    = 16u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_PREDICATE_REF     = 17u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ENTITY_REF        = 18u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ARITY_MISMATCH    = 19u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_BODY_OVERFLOW     = 20u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_TERM_KIND         = 21u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULE_VARIABLE_SAFETY   = 22u,
    LAPLACE_GRAPH_ARTIFACT_ERR_RULES_NOT_SUPPORTED    = 23u,
    LAPLACE_GRAPH_ARTIFACT_ERR_CHECKSUM_MISMATCH      = 24u,
    LAPLACE_GRAPH_ARTIFACT_STATUS_COUNT_              = 25u
};

typedef struct laplace_graph_artifact_validation {
    laplace_graph_artifact_status_t status;
    uint32_t                        record_index;   /**< index of the offending record */
    uint32_t                        detail_a;       /**< e.g. term index, local_id */
    uint32_t                        detail_b;       /**< e.g. expected arity */
} laplace_graph_artifact_validation_t;

LAPLACE_STATIC_ASSERT(sizeof(laplace_graph_artifact_validation_t) == 16u,
                       "validation detail must be exactly 16 bytes");

typedef struct laplace_graph_artifact {
    laplace_graph_artifact_header_t       header;
    const laplace_graph_artifact_predicate_t* predicates;  /**< [header.predicate_count] */
    const laplace_graph_artifact_entity_t*    entities;    /**< [header.entity_count]    */
    const laplace_graph_artifact_fact_t*      facts;       /**< [header.fact_count]      */
    const laplace_graph_artifact_rule_t*      rules;       /**< [header.rule_count] or NULL */
    const uint8_t*                            name_blob;   /**< optional name data */
    uint32_t                                  name_blob_size;
} laplace_graph_artifact_t;

laplace_graph_artifact_status_t
laplace_graph_artifact_validate(
    const laplace_graph_artifact_t*       artifact,
    laplace_graph_artifact_validation_t*  out_detail);

uint32_t
laplace_graph_artifact_compute_checksum(
    const laplace_graph_artifact_header_t* header);

const char*
laplace_graph_artifact_status_string(laplace_graph_artifact_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LAPLACE_GRAPH_ARTIFACT_H */
