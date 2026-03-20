#include "laplace/graph_artifact.h"
#include "laplace/exact.h"
#include "laplace/graph_profile.h"

#include <stddef.h>
#include <string.h>

uint32_t
laplace_graph_artifact_compute_checksum(
    const laplace_graph_artifact_header_t* const header)
{
    if (header == NULL) {
        return 0u;
    }
    /* Simple deterministic checksum: XOR of key header fields with rotation */
    uint32_t h = header->magic;
    h ^= (header->version << 3u) | (header->version >> 29u);
    h ^= ((uint32_t)header->profile_id << 7u);
    h ^= (header->flags << 11u) | (header->flags >> 21u);
    h ^= (header->predicate_count << 5u) | (header->predicate_count >> 27u);
    h ^= (header->entity_count << 13u) | (header->entity_count >> 19u);
    h ^= (header->fact_count << 17u) | (header->fact_count >> 15u);
    h ^= (header->rule_count << 23u) | (header->rule_count >> 9u);
    return h;
}

static laplace_graph_artifact_status_t
validate_header(const laplace_graph_artifact_header_t* const hdr,
                laplace_graph_artifact_validation_t* const detail)
{
    if (hdr->magic != LAPLACE_GRAPH_ARTIFACT_MAGIC) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_BAD_MAGIC;
        return detail->status;
    }
    if (hdr->version != LAPLACE_GRAPH_ARTIFACT_VERSION) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_BAD_VERSION;
        detail->detail_a = hdr->version;
        detail->detail_b = LAPLACE_GRAPH_ARTIFACT_VERSION;
        return detail->status;
    }
    if (hdr->profile_id == LAPLACE_GRAPH_PROFILE_INVALID ||
        hdr->profile_id >= LAPLACE_GRAPH_PROFILE_COUNT) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_BAD_PROFILE;
        detail->detail_a = (uint32_t)hdr->profile_id;
        return detail->status;
    }
    /* Check profile is supported */
    const laplace_graph_profile_descriptor_t* const prof =
        laplace_graph_profile_get(hdr->profile_id);
    if (prof->profile_id == LAPLACE_GRAPH_PROFILE_INVALID) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_UNSUPPORTED_PROFILE;
        detail->detail_a = (uint32_t)hdr->profile_id;
        return detail->status;
    }
    /* Section count bounds */
    if (hdr->predicate_count > LAPLACE_GRAPH_ARTIFACT_MAX_PREDICATES) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_OVERFLOW;
        detail->detail_a = hdr->predicate_count;
        return detail->status;
    }
    if (hdr->entity_count > LAPLACE_GRAPH_ARTIFACT_MAX_ENTITIES) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_OVERFLOW;
        detail->detail_a = hdr->entity_count;
        return detail->status;
    }
    if (hdr->fact_count > LAPLACE_GRAPH_ARTIFACT_MAX_FACTS) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_FACT_OVERFLOW;
        detail->detail_a = hdr->fact_count;
        return detail->status;
    }
    if (hdr->rule_count > LAPLACE_GRAPH_ARTIFACT_MAX_RULES) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_OVERFLOW;
        detail->detail_a = hdr->rule_count;
        return detail->status;
    }
    /* Rules present but flag not set */
    if (hdr->rule_count > 0u &&
        (hdr->flags & LAPLACE_GRAPH_ARTIFACT_FLAG_HAS_RULES) == 0u) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULES_WITHOUT_FLAG;
        return detail->status;
    }
    /* Rules present but profile doesn't support rules */
    if (hdr->rule_count > 0u) {
        if (prof->supported_rule_shapes == 0u) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULES_NOT_SUPPORTED;
            detail->detail_a = (uint32_t)hdr->profile_id;
            return detail->status;
        }
    }
    /* Checksum */
    const uint32_t expected = laplace_graph_artifact_compute_checksum(hdr);
    if (hdr->checksum != 0u && hdr->checksum != expected) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_CHECKSUM_MISMATCH;
        detail->detail_a = hdr->checksum;
        detail->detail_b = expected;
        return detail->status;
    }

    return LAPLACE_GRAPH_ARTIFACT_OK;
}

static bool
pred_local_id_exists(const laplace_graph_artifact_predicate_t* const preds,
                     const uint32_t count,
                     const uint16_t local_id)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (preds[i].local_id == local_id) {
            return true;
        }
    }
    return false;
}

static uint8_t
pred_arity_for_local_id(const laplace_graph_artifact_predicate_t* const preds,
                        const uint32_t count,
                        const uint16_t local_id)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (preds[i].local_id == local_id) {
            return preds[i].arity;
        }
    }
    return 0u;
}

static bool
entity_local_id_exists(const laplace_graph_artifact_entity_t* const ents,
                       const uint32_t count,
                       const uint32_t local_id)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (ents[i].local_id == local_id) {
            return true;
        }
    }
    return false;
}

static laplace_graph_artifact_status_t
validate_predicates(const laplace_graph_artifact_t* const art,
                    laplace_graph_artifact_validation_t* const detail)
{
    for (uint32_t i = 0u; i < art->header.predicate_count; ++i) {
        const laplace_graph_artifact_predicate_t* const p = &art->predicates[i];
        /* Arity bound */
        if (p->arity == 0u || p->arity > LAPLACE_GRAPH_ARTIFACT_MAX_ARITY) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_ARITY;
            detail->record_index = i;
            detail->detail_a = (uint32_t)p->arity;
            return detail->status;
        }
        /* Duplicate check: no earlier predicate with same local_id */
        for (uint32_t j = 0u; j < i; ++j) {
            if (art->predicates[j].local_id == p->local_id) {
                detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_DUPLICATE;
                detail->record_index = i;
                detail->detail_a = (uint32_t)p->local_id;
                return detail->status;
            }
        }
    }
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

static laplace_graph_artifact_status_t
validate_entities(const laplace_graph_artifact_t* const art,
                  laplace_graph_artifact_validation_t* const detail)
{
    for (uint32_t i = 0u; i < art->header.entity_count; ++i) {
        const laplace_graph_artifact_entity_t* const e = &art->entities[i];
        /* Duplicate check */
        for (uint32_t j = 0u; j < i; ++j) {
            if (art->entities[j].local_id == e->local_id) {
                detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_DUPLICATE;
                detail->record_index = i;
                detail->detail_a = e->local_id;
                return detail->status;
            }
        }
    }
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

static laplace_graph_artifact_status_t
validate_facts(const laplace_graph_artifact_t* const art,
               laplace_graph_artifact_validation_t* const detail)
{
    for (uint32_t i = 0u; i < art->header.fact_count; ++i) {
        const laplace_graph_artifact_fact_t* const f = &art->facts[i];
        /* Predicate reference */
        if (!pred_local_id_exists(art->predicates, art->header.predicate_count,
                                  f->predicate_local_id)) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_FACT_PREDICATE_REF;
            detail->record_index = i;
            detail->detail_a = (uint32_t)f->predicate_local_id;
            return detail->status;
        }
        /* Arity match */
        const uint8_t expected_arity = pred_arity_for_local_id(
            art->predicates, art->header.predicate_count, f->predicate_local_id);
        if (f->arg_count != expected_arity) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ARITY_MISMATCH;
            detail->record_index = i;
            detail->detail_a = (uint32_t)f->arg_count;
            detail->detail_b = (uint32_t)expected_arity;
            return detail->status;
        }
        /* Entity references */
        for (uint32_t a = 0u; a < f->arg_count; ++a) {
            if (!entity_local_id_exists(art->entities, art->header.entity_count,
                                        f->arg_entity_local_ids[a])) {
                detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ENTITY_REF;
                detail->record_index = i;
                detail->detail_a = a;
                detail->detail_b = f->arg_entity_local_ids[a];
                return detail->status;
            }
        }
    }
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

static laplace_graph_artifact_status_t
validate_rule_literal(const laplace_graph_artifact_t* const art,
                      const laplace_graph_artifact_literal_t* const lit,
                      const uint32_t rule_index,
                      laplace_graph_artifact_validation_t* const detail)
{
    /* Predicate reference */
    if (!pred_local_id_exists(art->predicates, art->header.predicate_count,
                              lit->predicate_local_id)) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_PREDICATE_REF;
        detail->record_index = rule_index;
        detail->detail_a = (uint32_t)lit->predicate_local_id;
        return detail->status;
    }
    /* Arity match */
    const uint8_t expected_arity = pred_arity_for_local_id(
        art->predicates, art->header.predicate_count, lit->predicate_local_id);
    if (lit->arity != expected_arity) {
        detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ARITY_MISMATCH;
        detail->record_index = rule_index;
        detail->detail_a = (uint32_t)lit->arity;
        detail->detail_b = (uint32_t)expected_arity;
        return detail->status;
    }
    /* Term validation */
    for (uint8_t t = 0u; t < lit->arity; ++t) {
        const laplace_graph_artifact_term_t* const term = &lit->terms[t];
        if (term->kind == 0u) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_TERM_KIND;
            detail->record_index = rule_index;
            detail->detail_a = (uint32_t)t;
            return detail->status;
        }
        if (term->kind == 2u) {
            /* Constant: must reference a declared entity */
            if (!entity_local_id_exists(art->entities, art->header.entity_count,
                                        term->value)) {
                detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ENTITY_REF;
                detail->record_index = rule_index;
                detail->detail_a = (uint32_t)t;
                detail->detail_b = term->value;
                return detail->status;
            }
        }
        if (term->kind > 2u) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_TERM_KIND;
            detail->record_index = rule_index;
            detail->detail_a = (uint32_t)t;
            return detail->status;
        }
    }
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

static laplace_graph_artifact_status_t
validate_rules(const laplace_graph_artifact_t* const art,
               laplace_graph_artifact_validation_t* const detail)
{
    for (uint32_t i = 0u; i < art->header.rule_count; ++i) {
        const laplace_graph_artifact_rule_t* const r = &art->rules[i];

        /* Body count bounds */
        if (r->body_count == 0u ||
            r->body_count > LAPLACE_GRAPH_ARTIFACT_MAX_RULE_BODY) {
            detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_BODY_OVERFLOW;
            detail->record_index = i;
            detail->detail_a = r->body_count;
            return detail->status;
        }

        /* Validate head literal */
        laplace_graph_artifact_status_t s =
            validate_rule_literal(art, &r->head, i, detail);
        if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
            return s;
        }

        /* Validate body literals */
        for (uint32_t b = 0u; b < r->body_count; ++b) {
            s = validate_rule_literal(art, &r->body[b], i, detail);
            if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
                return s;
            }
        }

        /* Datalog safety: all head variables must appear in at least one body literal */
        for (uint8_t ht = 0u; ht < r->head.arity; ++ht) {
            if (r->head.terms[ht].kind != 1u) { /* not a variable */
                continue;
            }
            const uint32_t head_var = r->head.terms[ht].value;
            bool found = false;
            for (uint32_t b = 0u; b < r->body_count && !found; ++b) {
                for (uint8_t bt = 0u; bt < r->body[b].arity && !found; ++bt) {
                    if (r->body[b].terms[bt].kind == 1u &&
                        r->body[b].terms[bt].value == head_var) {
                        found = true;
                    }
                }
            }
            if (!found) {
                detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_RULE_VARIABLE_SAFETY;
                detail->record_index = i;
                detail->detail_a = (uint32_t)ht;
                detail->detail_b = head_var;
                return detail->status;
            }
        }
    }
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

laplace_graph_artifact_status_t
laplace_graph_artifact_validate(
    const laplace_graph_artifact_t* const       artifact,
    laplace_graph_artifact_validation_t* const   out_detail)
{
    laplace_graph_artifact_validation_t local_detail;
    memset(&local_detail, 0, sizeof(local_detail));

    if (artifact == NULL || out_detail == NULL) {
        if (out_detail != NULL) {
            out_detail->status = LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
        }
        return LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
    }

    /* Header */
    laplace_graph_artifact_status_t s = validate_header(&artifact->header, &local_detail);
    if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
        *out_detail = local_detail;
        return s;
    }

    /* Section pointers must be non-null if counts > 0 */
    if (artifact->header.predicate_count > 0u && artifact->predicates == NULL) {
        local_detail.status = LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
        *out_detail = local_detail;
        return local_detail.status;
    }
    if (artifact->header.entity_count > 0u && artifact->entities == NULL) {
        local_detail.status = LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
        *out_detail = local_detail;
        return local_detail.status;
    }
    if (artifact->header.fact_count > 0u && artifact->facts == NULL) {
        local_detail.status = LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
        *out_detail = local_detail;
        return local_detail.status;
    }
    if (artifact->header.rule_count > 0u && artifact->rules == NULL) {
        local_detail.status = LAPLACE_GRAPH_ARTIFACT_ERR_NULL;
        *out_detail = local_detail;
        return local_detail.status;
    }

    /* Predicates */
    s = validate_predicates(artifact, &local_detail);
    if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
        *out_detail = local_detail;
        return s;
    }

    /* Entities */
    s = validate_entities(artifact, &local_detail);
    if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
        *out_detail = local_detail;
        return s;
    }

    /* Facts */
    s = validate_facts(artifact, &local_detail);
    if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
        *out_detail = local_detail;
        return s;
    }

    /* Rules */
    if (artifact->header.rule_count > 0u) {
        s = validate_rules(artifact, &local_detail);
        if (s != LAPLACE_GRAPH_ARTIFACT_OK) {
            *out_detail = local_detail;
            return s;
        }
    }

    out_detail->status = LAPLACE_GRAPH_ARTIFACT_OK;
    return LAPLACE_GRAPH_ARTIFACT_OK;
}

const char*
laplace_graph_artifact_status_string(const laplace_graph_artifact_status_t status)
{
    switch (status) {
        case LAPLACE_GRAPH_ARTIFACT_OK:                      return "OK";
        case LAPLACE_GRAPH_ARTIFACT_ERR_NULL:                return "NULL_ARGUMENT";
        case LAPLACE_GRAPH_ARTIFACT_ERR_BAD_MAGIC:           return "BAD_MAGIC";
        case LAPLACE_GRAPH_ARTIFACT_ERR_BAD_VERSION:         return "BAD_VERSION";
        case LAPLACE_GRAPH_ARTIFACT_ERR_BAD_PROFILE:         return "BAD_PROFILE";
        case LAPLACE_GRAPH_ARTIFACT_ERR_UNSUPPORTED_PROFILE: return "UNSUPPORTED_PROFILE";
        case LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_OVERFLOW:  return "PREDICATE_OVERFLOW";
        case LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_OVERFLOW:     return "ENTITY_OVERFLOW";
        case LAPLACE_GRAPH_ARTIFACT_ERR_FACT_OVERFLOW:       return "FACT_OVERFLOW";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_OVERFLOW:       return "RULE_OVERFLOW";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULES_WITHOUT_FLAG:  return "RULES_WITHOUT_FLAG";
        case LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_ARITY:     return "PREDICATE_ARITY";
        case LAPLACE_GRAPH_ARTIFACT_ERR_PREDICATE_DUPLICATE: return "PREDICATE_DUPLICATE";
        case LAPLACE_GRAPH_ARTIFACT_ERR_ENTITY_DUPLICATE:    return "ENTITY_DUPLICATE";
        case LAPLACE_GRAPH_ARTIFACT_ERR_FACT_PREDICATE_REF:  return "FACT_PREDICATE_REF";
        case LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ENTITY_REF:     return "FACT_ENTITY_REF";
        case LAPLACE_GRAPH_ARTIFACT_ERR_FACT_ARITY_MISMATCH: return "FACT_ARITY_MISMATCH";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_PREDICATE_REF:  return "RULE_PREDICATE_REF";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ENTITY_REF:     return "RULE_ENTITY_REF";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_ARITY_MISMATCH: return "RULE_ARITY_MISMATCH";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_BODY_OVERFLOW:  return "RULE_BODY_OVERFLOW";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_TERM_KIND:      return "RULE_TERM_KIND";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULE_VARIABLE_SAFETY:return "RULE_VARIABLE_SAFETY";
        case LAPLACE_GRAPH_ARTIFACT_ERR_RULES_NOT_SUPPORTED: return "RULES_NOT_SUPPORTED";
        case LAPLACE_GRAPH_ARTIFACT_ERR_CHECKSUM_MISMATCH:   return "CHECKSUM_MISMATCH";
        default:                                              return "UNKNOWN";
    }
}
