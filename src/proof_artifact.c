#include "laplace/proof_artifact.h"

#include <string.h>

static void validation_init(laplace_proof_validation_result_t* r) {
    memset(r, 0, sizeof(*r));
    r->valid = true;
}

static void validation_add_error(laplace_proof_validation_result_t* r,
                                  laplace_proof_validation_error_t error,
                                  uint32_t index) {
    if (r->error_count < LAPLACE_PROOF_VALIDATE_MAX_ERRORS) {
        r->errors[r->error_count]       = error;
        r->error_indices[r->error_count] = index;
        r->error_count++;
    }
    r->valid = false;
}

static bool validation_check_store(const laplace_proof_store_t* store,
                                    laplace_proof_validation_result_t* result) {
    if (!store) {
        validation_add_error(result, LAPLACE_PROOF_VALIDATE_NULL_STORE, 0u);
        return false;
    }
    if (!store->initialized) {
        validation_add_error(result, LAPLACE_PROOF_VALIDATE_STORE_NOT_INITIALIZED, 0u);
        return false;
    }
    return true;
}

void laplace_proof_validate_symbols(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->symbol_count; ++i) {
        const laplace_proof_symbol_t* sym = &store->symbols[i];
        if (sym->kind != LAPLACE_PROOF_SYMBOL_CONSTANT &&
            sym->kind != LAPLACE_PROOF_SYMBOL_VARIABLE) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_KIND, i);
        }
    }
}

void laplace_proof_validate_expressions(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->expr_count; ++i) {
        const laplace_proof_expr_t* e = &store->expressions[i];

        if (e->typecode == LAPLACE_PROOF_SYMBOL_ID_INVALID ||
            e->typecode > store->symbol_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_TYPECODE, i);
            continue;
        }
        if (store->symbols[e->typecode].kind != LAPLACE_PROOF_SYMBOL_CONSTANT) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_TYPECODE, i);
            continue;
        }

        if (e->token_count > 0u) {
            if (e->token_offset + e->token_count > store->token_pool_used) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_TOKEN_SPAN_OVERFLOW, i);
                continue;
            }

            for (uint32_t t = 0u; t < e->token_count; ++t) {
                const laplace_proof_symbol_id_t tok = store->token_pool[e->token_offset + t];
                if (tok == LAPLACE_PROOF_SYMBOL_ID_INVALID || tok > store->symbol_count) {
                    validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_TOKEN, i);
                    break;
                }
            }
        }
    }
}

void laplace_proof_validate_frames(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->frame_count; ++i) {
        const laplace_proof_frame_t* f = &store->frames[i];

        if (f->parent_id != LAPLACE_PROOF_FRAME_ID_INVALID) {
            if (f->parent_id > store->frame_count) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_PARENT_FRAME, i);
                continue;
            }
        }

        if (f->float_hyp_count > 0u) {
            if (f->float_hyp_offset + f->float_hyp_count > store->hyp_count + 1u) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW, i);
            }
        }

        if (f->essential_hyp_count > 0u) {
            if (f->essential_hyp_offset + f->essential_hyp_count > store->hyp_count + 1u) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW, i);
            }
        }

        if (f->dv_count > 0u) {
            if (f->dv_offset + f->dv_count > store->dv_pair_count) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_SPAN_OVERFLOW, i);
            }
        }

        if (f->mandatory_var_count > 0u) {
            if (f->mandatory_var_offset + f->mandatory_var_count > store->mandatory_var_count) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_OVERFLOW, i);
            } else {
                for (uint32_t v = 0u; v < f->mandatory_var_count; ++v) {
                    laplace_proof_symbol_id_t vid =
                        store->mandatory_vars[f->mandatory_var_offset + v];
                    if (!laplace_proof_symbol_is_variable(store, vid)) {
                        validation_add_error(result,
                            LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_NOT_VARIABLE, i);
                        break;
                    }
                }
            }
        }
    }
}

void laplace_proof_validate_hypotheses(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->hyp_count; ++i) {
        const laplace_proof_hyp_t* h = &store->hypotheses[i];

        if (h->kind != LAPLACE_PROOF_HYP_FLOATING &&
            h->kind != LAPLACE_PROOF_HYP_ESSENTIAL) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_HYP_KIND, i);
            continue;
        }

        if (h->expr_id == LAPLACE_PROOF_EXPR_ID_INVALID ||
            h->expr_id > store->expr_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_HYP_EXPR, i);
            continue;
        }

        if (h->frame_id == LAPLACE_PROOF_FRAME_ID_INVALID ||
            h->frame_id > store->frame_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_HYP_FRAME, i);
        }
    }
}

void laplace_proof_validate_assertions(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->assertion_count; ++i) {
        const laplace_proof_assertion_t* a = &store->assertions[i];

        if (a->kind != LAPLACE_PROOF_ASSERTION_AXIOM &&
            a->kind != LAPLACE_PROOF_ASSERTION_THEOREM) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_KIND, i);
            continue;
        }

        if (a->frame_id == LAPLACE_PROOF_FRAME_ID_INVALID ||
            a->frame_id > store->frame_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_FRAME, i);
            continue;
        }

        if (a->conclusion_id == LAPLACE_PROOF_EXPR_ID_INVALID ||
            a->conclusion_id > store->expr_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_EXPR, i);
            continue;
        }

        if (a->mand_float_count > 0u) {
            if (a->mand_float_offset + a->mand_float_count > store->hyp_count + 1u) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW, i);
            }
        }

        if (a->mand_ess_count > 0u) {
            if (a->mand_ess_offset + a->mand_ess_count > store->hyp_count + 1u) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW, i);
            }
        }

        if (a->dv_count > 0u) {
            if (a->dv_offset + a->dv_count > store->dv_pair_count) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_SPAN_OVERFLOW, i);
            }
        }
    }
}

void laplace_proof_validate_dv_pairs(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 0u; i < store->dv_pair_count; ++i) {
        const laplace_proof_dv_pair_t* p = &store->dv_pairs[i];

        if (!laplace_proof_symbol_is_variable(store, p->var_a)) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_NOT_VARIABLE, i);
            continue;
        }
        if (!laplace_proof_symbol_is_variable(store, p->var_b)) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_NOT_VARIABLE, i);
            continue;
        }

        if (p->var_a == p->var_b) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_SAME_VARIABLE, i);
            continue;
        }

        if (p->var_a >= p->var_b) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_DV_NOT_NORMALIZED, i);
        }
    }
}

void laplace_proof_validate_theorems(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    for (uint32_t i = 1u; i <= store->theorem_count; ++i) {
        const laplace_proof_theorem_t* thm = &store->theorems[i];

        if (thm->assertion_id == LAPLACE_PROOF_ASSERTION_ID_INVALID ||
            thm->assertion_id > store->assertion_count) {
            validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_THEOREM_ASSERTION, i);
            continue;
        }

        if (thm->step_count > 0u) {
            if (thm->step_offset + thm->step_count > store->proof_step_count) {
                validation_add_error(result, LAPLACE_PROOF_VALIDATE_STEP_SPAN_OVERFLOW, i);
                continue;
            }

            for (uint32_t s = 0u; s < thm->step_count; ++s) {
                const laplace_proof_step_t* step =
                    &store->proof_steps[thm->step_offset + s];

                if (step->kind == LAPLACE_PROOF_STEP_INVALID ||
                    step->kind >= LAPLACE_PROOF_STEP_KIND_COUNT_) {
                    validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_STEP_KIND, i);
                    break;
                }

                if (step->kind == LAPLACE_PROOF_STEP_HYP) {
                    if (step->ref_id == LAPLACE_PROOF_HYP_ID_INVALID ||
                        step->ref_id > store->hyp_count) {
                        validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_STEP_REF, i);
                        break;
                    }
                } else if (step->kind == LAPLACE_PROOF_STEP_ASSERTION) {
                    if (step->ref_id == LAPLACE_PROOF_ASSERTION_ID_INVALID ||
                        step->ref_id > store->assertion_count) {
                        validation_add_error(result, LAPLACE_PROOF_VALIDATE_INVALID_STEP_REF, i);
                        break;
                    }
                }
            }
        }
    }
}

void laplace_proof_validate_all(
    const laplace_proof_store_t* store,
    laplace_proof_validation_result_t* result) {

    validation_init(result);
    if (!validation_check_store(store, result)) return;

    laplace_proof_validation_result_t sub;

    laplace_proof_validate_symbols(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_expressions(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_frames(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_hypotheses(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_assertions(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_dv_pairs(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    laplace_proof_validate_theorems(store, &sub);
    if (!sub.valid) {
        *result = sub;
        return;
    }

    result->valid = true;
}

const char* laplace_proof_validation_error_string(
    laplace_proof_validation_error_t error) {

    switch (error) {
    case LAPLACE_PROOF_VALIDATE_OK:
        return "ok";
    case LAPLACE_PROOF_VALIDATE_NULL_STORE:
        return "null store";
    case LAPLACE_PROOF_VALIDATE_STORE_NOT_INITIALIZED:
        return "store not initialized";
    case LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_ID:
        return "invalid symbol ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_KIND:
        return "invalid symbol kind";
    case LAPLACE_PROOF_VALIDATE_INVALID_EXPR_ID:
        return "invalid expression ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_TYPECODE:
        return "invalid typecode";
    case LAPLACE_PROOF_VALIDATE_INVALID_TOKEN:
        return "invalid token";
    case LAPLACE_PROOF_VALIDATE_TOKEN_SPAN_OVERFLOW:
        return "token span overflow";
    case LAPLACE_PROOF_VALIDATE_INVALID_FRAME_ID:
        return "invalid frame ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_PARENT_FRAME:
        return "invalid parent frame";
    case LAPLACE_PROOF_VALIDATE_INVALID_HYP_ID:
        return "invalid hypothesis ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_HYP_KIND:
        return "invalid hypothesis kind";
    case LAPLACE_PROOF_VALIDATE_INVALID_HYP_EXPR:
        return "invalid hypothesis expression";
    case LAPLACE_PROOF_VALIDATE_INVALID_HYP_FRAME:
        return "invalid hypothesis frame";
    case LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_ID:
        return "invalid assertion ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_KIND:
        return "invalid assertion kind";
    case LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_FRAME:
        return "invalid assertion frame";
    case LAPLACE_PROOF_VALIDATE_INVALID_ASSERTION_EXPR:
        return "invalid assertion expression";
    case LAPLACE_PROOF_VALIDATE_INVALID_DV_PAIR:
        return "invalid DV pair";
    case LAPLACE_PROOF_VALIDATE_DV_NOT_VARIABLE:
        return "DV pair member is not a variable";
    case LAPLACE_PROOF_VALIDATE_DV_SAME_VARIABLE:
        return "DV pair contains same variable twice";
    case LAPLACE_PROOF_VALIDATE_DV_NOT_NORMALIZED:
        return "DV pair not normalized (var_a must be < var_b)";
    case LAPLACE_PROOF_VALIDATE_INVALID_THEOREM_ID:
        return "invalid theorem ID";
    case LAPLACE_PROOF_VALIDATE_INVALID_THEOREM_ASSERTION:
        return "invalid theorem assertion reference";
    case LAPLACE_PROOF_VALIDATE_INVALID_STEP_KIND:
        return "invalid proof step kind";
    case LAPLACE_PROOF_VALIDATE_INVALID_STEP_REF:
        return "invalid proof step reference";
    case LAPLACE_PROOF_VALIDATE_STEP_SPAN_OVERFLOW:
        return "proof step span overflow";
    case LAPLACE_PROOF_VALIDATE_HYP_SPAN_OVERFLOW:
        return "hypothesis span overflow";
    case LAPLACE_PROOF_VALIDATE_DV_SPAN_OVERFLOW:
        return "DV pair span overflow";
    case LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_OVERFLOW:
        return "mandatory variable span overflow";
    case LAPLACE_PROOF_VALIDATE_MANDATORY_VAR_NOT_VARIABLE:
        return "mandatory variable is not a variable symbol";
    default:
        return "unknown validation error";
    }
}
