#include "laplace/proof.h"

#include <string.h>

static uint32_t proof_expr_hash(laplace_proof_symbol_id_t typecode,
                                 const laplace_proof_symbol_id_t* tokens,
                                 uint32_t count) {
    uint32_t h = 0x811c9dc5u;
    h ^= typecode;
    h *= 0x01000193u;
    for (uint32_t i = 0u; i < count; ++i) {
        h ^= tokens[i];
        h *= 0x01000193u;
    }
    return h;
}

laplace_error_t laplace_proof_store_init(laplace_proof_store_t* store,
                                          laplace_arena_t* arena) {
    if (!store || !arena) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    memset(store, 0, sizeof(*store));

    store->symbols = (laplace_proof_symbol_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_SYMBOLS + 1u) * sizeof(laplace_proof_symbol_t), 8u);
    if (!store->symbols) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->symbols, 0, (LAPLACE_PROOF_MAX_SYMBOLS + 1u) * sizeof(laplace_proof_symbol_t));

    store->token_pool = (laplace_proof_symbol_id_t*)laplace_arena_alloc(
        arena, LAPLACE_PROOF_MAX_TOKEN_POOL * sizeof(laplace_proof_symbol_id_t), 8u);
    if (!store->token_pool) return LAPLACE_ERR_CAPACITY_EXHAUSTED;

    store->expressions = (laplace_proof_expr_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_EXPRESSIONS + 1u) * sizeof(laplace_proof_expr_t), 8u);
    if (!store->expressions) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->expressions, 0, (LAPLACE_PROOF_MAX_EXPRESSIONS + 1u) * sizeof(laplace_proof_expr_t));

    store->hypotheses = (laplace_proof_hyp_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_HYPOTHESES + 1u) * sizeof(laplace_proof_hyp_t), 8u);
    if (!store->hypotheses) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->hypotheses, 0, (LAPLACE_PROOF_MAX_HYPOTHESES + 1u) * sizeof(laplace_proof_hyp_t));

    store->frames = (laplace_proof_frame_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_FRAMES + 1u) * sizeof(laplace_proof_frame_t), 8u);
    if (!store->frames) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->frames, 0, (LAPLACE_PROOF_MAX_FRAMES + 1u) * sizeof(laplace_proof_frame_t));

    store->dv_pairs = (laplace_proof_dv_pair_t*)laplace_arena_alloc(
        arena, LAPLACE_PROOF_MAX_DV_PAIRS * sizeof(laplace_proof_dv_pair_t), 8u);
    if (!store->dv_pairs) return LAPLACE_ERR_CAPACITY_EXHAUSTED;

    const uint32_t mand_var_capacity = LAPLACE_PROOF_MAX_SYMBOLS;
    store->mandatory_vars = (laplace_proof_symbol_id_t*)laplace_arena_alloc(
        arena, mand_var_capacity * sizeof(laplace_proof_symbol_id_t), 8u);
    if (!store->mandatory_vars) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    store->mandatory_var_capacity = mand_var_capacity;

    store->assertions = (laplace_proof_assertion_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_ASSERTIONS + 1u) * sizeof(laplace_proof_assertion_t), 8u);
    if (!store->assertions) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->assertions, 0, (LAPLACE_PROOF_MAX_ASSERTIONS + 1u) * sizeof(laplace_proof_assertion_t));

    store->theorems = (laplace_proof_theorem_t*)laplace_arena_alloc(
        arena, (LAPLACE_PROOF_MAX_THEOREMS + 1u) * sizeof(laplace_proof_theorem_t), 8u);
    if (!store->theorems) return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    memset(store->theorems, 0, (LAPLACE_PROOF_MAX_THEOREMS + 1u) * sizeof(laplace_proof_theorem_t));

    store->proof_steps = (laplace_proof_step_t*)laplace_arena_alloc(
        arena, LAPLACE_PROOF_MAX_PROOF_STEPS * sizeof(laplace_proof_step_t), 8u);
    if (!store->proof_steps) return LAPLACE_ERR_CAPACITY_EXHAUSTED;

    store->initialized = true;
    return LAPLACE_OK;
}

void laplace_proof_store_reset(laplace_proof_store_t* store) {
    if (!store) return;

    store->symbol_count      = 0u;
    store->token_pool_used   = 0u;
    store->expr_count        = 0u;
    store->hyp_count         = 0u;
    store->frame_count       = 0u;
    store->dv_pair_count     = 0u;
    store->mandatory_var_count = 0u;
    store->assertion_count   = 0u;
    store->theorem_count     = 0u;
    store->proof_step_count  = 0u;

    if (store->symbols)     memset(&store->symbols[0], 0, sizeof(laplace_proof_symbol_t));
    if (store->expressions) memset(&store->expressions[0], 0, sizeof(laplace_proof_expr_t));
    if (store->hypotheses)  memset(&store->hypotheses[0], 0, sizeof(laplace_proof_hyp_t));
    if (store->frames)      memset(&store->frames[0], 0, sizeof(laplace_proof_frame_t));
    if (store->assertions)  memset(&store->assertions[0], 0, sizeof(laplace_proof_assertion_t));
    if (store->theorems)    memset(&store->theorems[0], 0, sizeof(laplace_proof_theorem_t));
}

laplace_error_t laplace_proof_import_symbol(
    laplace_proof_store_t* store,
    laplace_proof_symbol_kind_t kind,
    uint32_t compiler_index,
    uint32_t flags,
    laplace_proof_symbol_id_t* out_id) {

    if (!store || !store->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;
    if (kind != LAPLACE_PROOF_SYMBOL_CONSTANT && kind != LAPLACE_PROOF_SYMBOL_VARIABLE) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }
    if (store->symbol_count >= LAPLACE_PROOF_MAX_SYMBOLS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t id = store->symbol_count + 1u; /* 1-based */
    store->symbols[id].kind           = kind;
    store->symbols[id].compiler_index = compiler_index;
    store->symbols[id].flags          = flags;
    store->symbol_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_symbol_t* laplace_proof_get_symbol(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_SYMBOL_ID_INVALID || id > store->symbol_count) return NULL;
    return &store->symbols[id];
}

bool laplace_proof_symbol_is_valid(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id) {

    return laplace_proof_get_symbol(store, id) != NULL;
}

bool laplace_proof_symbol_is_variable(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id) {

    const laplace_proof_symbol_t* sym = laplace_proof_get_symbol(store, id);
    return sym && sym->kind == LAPLACE_PROOF_SYMBOL_VARIABLE;
}

bool laplace_proof_symbol_is_constant(
    const laplace_proof_store_t* store,
    laplace_proof_symbol_id_t id) {

    const laplace_proof_symbol_t* sym = laplace_proof_get_symbol(store, id);
    return sym && sym->kind == LAPLACE_PROOF_SYMBOL_CONSTANT;
}

laplace_error_t laplace_proof_import_expr(
    laplace_proof_store_t* store,
    laplace_proof_symbol_id_t typecode,
    const laplace_proof_symbol_id_t* tokens,
    uint32_t token_count,
    laplace_proof_expr_id_t* out_id) {

    if (!store || !store->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;

    if (!laplace_proof_symbol_is_constant(store, typecode)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (token_count > LAPLACE_PROOF_MAX_EXPR_TOKENS) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (token_count > 0u && !tokens) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }
    for (uint32_t i = 0u; i < token_count; ++i) {
        if (!laplace_proof_symbol_is_valid(store, tokens[i])) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    if (store->expr_count >= LAPLACE_PROOF_MAX_EXPRESSIONS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    if (store->token_pool_used + token_count > LAPLACE_PROOF_MAX_TOKEN_POOL) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t offset = store->token_pool_used;
    if (token_count > 0u) {
        memcpy(&store->token_pool[offset], tokens,
               token_count * sizeof(laplace_proof_symbol_id_t));
    }
    store->token_pool_used += token_count;

    const uint32_t id = store->expr_count + 1u;
    store->expressions[id].typecode     = typecode;
    store->expressions[id].token_offset = offset;
    store->expressions[id].token_count  = token_count;
    store->expressions[id].hash         = proof_expr_hash(typecode, tokens, token_count);
    store->expr_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_expr_t* laplace_proof_get_expr(
    const laplace_proof_store_t* store,
    laplace_proof_expr_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_EXPR_ID_INVALID || id > store->expr_count) return NULL;
    return &store->expressions[id];
}

laplace_error_t laplace_proof_get_expr_tokens(
    const laplace_proof_store_t* store,
    laplace_proof_expr_id_t id,
    const laplace_proof_symbol_id_t** out_tokens,
    uint32_t* out_count) {

    if (!store || !store->initialized || !out_tokens || !out_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_expr_t* expr = laplace_proof_get_expr(store, id);
    if (!expr) return LAPLACE_ERR_OUT_OF_RANGE;

    *out_tokens = &store->token_pool[expr->token_offset];
    *out_count  = expr->token_count;
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_import_frame(
    laplace_proof_store_t* store,
    const laplace_proof_frame_desc_t* desc,
    laplace_proof_frame_id_t* out_id) {

    if (!store || !store->initialized || !desc) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->parent_id != LAPLACE_PROOF_FRAME_ID_INVALID) {
        if (desc->parent_id > store->frame_count) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    if (desc->mandatory_var_count > 0u && !desc->mandatory_vars) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }
    for (uint32_t i = 0u; i < desc->mandatory_var_count; ++i) {
        if (!laplace_proof_symbol_is_variable(store, desc->mandatory_vars[i])) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
    }

    if (store->frame_count >= LAPLACE_PROOF_MAX_FRAMES) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    if (store->mandatory_var_count + desc->mandatory_var_count > store->mandatory_var_capacity) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t mand_offset = store->mandatory_var_count;
    if (desc->mandatory_var_count > 0u) {
        memcpy(&store->mandatory_vars[mand_offset], desc->mandatory_vars,
               desc->mandatory_var_count * sizeof(laplace_proof_symbol_id_t));
    }
    store->mandatory_var_count += desc->mandatory_var_count;

    const uint32_t id = store->frame_count + 1u;
    laplace_proof_frame_t* f = &store->frames[id];
    f->parent_id           = desc->parent_id;
    f->float_hyp_offset    = 0u;
    f->float_hyp_count     = 0u;
    f->essential_hyp_offset = 0u;
    f->essential_hyp_count = 0u;
    f->dv_offset           = 0u;
    f->dv_count            = 0u;
    f->mandatory_var_offset = mand_offset;
    f->mandatory_var_count = desc->mandatory_var_count;
    store->frame_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_frame_t* laplace_proof_get_frame(
    const laplace_proof_store_t* store,
    laplace_proof_frame_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_FRAME_ID_INVALID || id > store->frame_count) return NULL;
    return &store->frames[id];
}

laplace_error_t laplace_proof_import_float_hyp(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_hyp_id_t* out_id) {

    if (!store || !store->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;

    if (frame_id == LAPLACE_PROOF_FRAME_ID_INVALID || frame_id > store->frame_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_proof_get_expr(store, expr_id)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (store->hyp_count >= LAPLACE_PROOF_MAX_HYPOTHESES) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t id = store->hyp_count + 1u;
    laplace_proof_hyp_t* h = &store->hypotheses[id];
    h->kind     = LAPLACE_PROOF_HYP_FLOATING;
    h->expr_id  = expr_id;
    h->frame_id = frame_id;

    laplace_proof_frame_t* f = &store->frames[frame_id];
    if (f->float_hyp_count == 0u) {
        f->float_hyp_offset = id;
    }
    f->float_hyp_count++;
    h->order = f->float_hyp_count - 1u;

    store->hyp_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_import_essential_hyp(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_expr_id_t expr_id,
    laplace_proof_hyp_id_t* out_id) {

    if (!store || !store->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;

    if (frame_id == LAPLACE_PROOF_FRAME_ID_INVALID || frame_id > store->frame_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_proof_get_expr(store, expr_id)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (store->hyp_count >= LAPLACE_PROOF_MAX_HYPOTHESES) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t id = store->hyp_count + 1u;
    laplace_proof_hyp_t* h = &store->hypotheses[id];
    h->kind     = LAPLACE_PROOF_HYP_ESSENTIAL;
    h->expr_id  = expr_id;
    h->frame_id = frame_id;

    laplace_proof_frame_t* f = &store->frames[frame_id];
    if (f->essential_hyp_count == 0u) {
        f->essential_hyp_offset = id;
    }
    f->essential_hyp_count++;
    h->order = f->essential_hyp_count - 1u;

    store->hyp_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_hyp_t* laplace_proof_get_hyp(
    const laplace_proof_store_t* store,
    laplace_proof_hyp_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_HYP_ID_INVALID || id > store->hyp_count) return NULL;
    return &store->hypotheses[id];
}

laplace_error_t laplace_proof_import_dv_pair(
    laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    laplace_proof_symbol_id_t var_a,
    laplace_proof_symbol_id_t var_b,
    laplace_proof_dv_id_t* out_id) {

    if (!store || !store->initialized) return LAPLACE_ERR_INVALID_ARGUMENT;

    if (frame_id == LAPLACE_PROOF_FRAME_ID_INVALID || frame_id > store->frame_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_proof_symbol_is_variable(store, var_a) ||
        !laplace_proof_symbol_is_variable(store, var_b)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (var_a == var_b) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    laplace_proof_symbol_id_t lo = var_a < var_b ? var_a : var_b;
    laplace_proof_symbol_id_t hi = var_a < var_b ? var_b : var_a;

    laplace_proof_frame_t* f = &store->frames[frame_id];
    for (uint32_t i = 0u; i < f->dv_count; ++i) {
        const laplace_proof_dv_pair_t* p = &store->dv_pairs[f->dv_offset + i];
        if (p->var_a == lo && p->var_b == hi) {
            if (out_id) *out_id = f->dv_offset + i;
            return LAPLACE_OK;
        }
    }

    if (store->dv_pair_count >= LAPLACE_PROOF_MAX_DV_PAIRS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t idx = store->dv_pair_count;
    store->dv_pairs[idx].var_a = lo;
    store->dv_pairs[idx].var_b = hi;

    if (f->dv_count == 0u) {
        f->dv_offset = idx;
    }
    f->dv_count++;
    store->dv_pair_count++;

    if (out_id) *out_id = idx;
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_get_frame_dv_pairs(
    const laplace_proof_store_t* store,
    laplace_proof_frame_id_t frame_id,
    const laplace_proof_dv_pair_t** out_pairs,
    uint32_t* out_count) {

    if (!store || !store->initialized || !out_pairs || !out_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_frame_t* f = laplace_proof_get_frame(store, frame_id);
    if (!f) return LAPLACE_ERR_OUT_OF_RANGE;

    if (f->dv_count == 0u) {
        *out_pairs = NULL;
        *out_count = 0u;
    } else {
        *out_pairs = &store->dv_pairs[f->dv_offset];
        *out_count = f->dv_count;
    }
    return LAPLACE_OK;
}

laplace_error_t laplace_proof_import_assertion(
    laplace_proof_store_t* store,
    const laplace_proof_assertion_desc_t* desc,
    laplace_proof_assertion_id_t* out_id) {

    if (!store || !store->initialized || !desc) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->kind != LAPLACE_PROOF_ASSERTION_AXIOM &&
        desc->kind != LAPLACE_PROOF_ASSERTION_THEOREM) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->frame_id == LAPLACE_PROOF_FRAME_ID_INVALID ||
        desc->frame_id > store->frame_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (!laplace_proof_get_expr(store, desc->conclusion_id)) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->mand_float_count > 0u) {
        if (desc->mand_float_offset + desc->mand_float_count > store->hyp_count + 1u) {
            return LAPLACE_ERR_OUT_OF_RANGE;
        }
    }

    if (desc->mand_ess_count > 0u) {
        if (desc->mand_ess_offset + desc->mand_ess_count > store->hyp_count + 1u) {
            return LAPLACE_ERR_OUT_OF_RANGE;
        }
    }

    if (desc->dv_count > 0u) {
        if (desc->dv_offset + desc->dv_count > store->dv_pair_count) {
            return LAPLACE_ERR_OUT_OF_RANGE;
        }
    }

    if (store->assertion_count >= LAPLACE_PROOF_MAX_ASSERTIONS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t id = store->assertion_count + 1u;
    laplace_proof_assertion_t* a = &store->assertions[id];
    a->kind             = desc->kind;
    a->frame_id         = desc->frame_id;
    a->conclusion_id    = desc->conclusion_id;
    a->mand_float_offset = desc->mand_float_offset;
    a->mand_float_count = desc->mand_float_count;
    a->mand_ess_offset  = desc->mand_ess_offset;
    a->mand_ess_count   = desc->mand_ess_count;
    a->dv_offset        = desc->dv_offset;
    a->dv_count         = desc->dv_count;
    a->theorem_id       = LAPLACE_PROOF_THEOREM_ID_INVALID;
    a->compiler_index   = desc->compiler_index;
    store->assertion_count++;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_assertion_t* laplace_proof_get_assertion(
    const laplace_proof_store_t* store,
    laplace_proof_assertion_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_ASSERTION_ID_INVALID || id > store->assertion_count) return NULL;
    return &store->assertions[id];
}

laplace_error_t laplace_proof_import_theorem(
    laplace_proof_store_t* store,
    const laplace_proof_theorem_desc_t* desc,
    laplace_proof_theorem_id_t* out_id) {

    if (!store || !store->initialized || !desc) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_assertion_t* assn =
        laplace_proof_get_assertion(store, desc->assertion_id);
    if (!assn) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (assn->kind != LAPLACE_PROOF_ASSERTION_THEOREM) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    if (desc->step_count > 0u && !desc->steps) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0u; i < desc->step_count; ++i) {
        const laplace_proof_step_t* s = &desc->steps[i];
        if (s->kind == LAPLACE_PROOF_STEP_INVALID || s->kind >= LAPLACE_PROOF_STEP_KIND_COUNT_) {
            return LAPLACE_ERR_INVALID_ARGUMENT;
        }
        if (s->kind == LAPLACE_PROOF_STEP_HYP) {
            if (s->ref_id == LAPLACE_PROOF_HYP_ID_INVALID || s->ref_id > store->hyp_count) {
                return LAPLACE_ERR_OUT_OF_RANGE;
            }
        } else if (s->kind == LAPLACE_PROOF_STEP_ASSERTION) {
            if (s->ref_id == LAPLACE_PROOF_ASSERTION_ID_INVALID || s->ref_id > store->assertion_count) {
                return LAPLACE_ERR_OUT_OF_RANGE;
            }
        }
    }

    if (store->theorem_count >= LAPLACE_PROOF_MAX_THEOREMS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }
    if (store->proof_step_count + desc->step_count > LAPLACE_PROOF_MAX_PROOF_STEPS) {
        return LAPLACE_ERR_CAPACITY_EXHAUSTED;
    }

    const uint32_t step_offset = store->proof_step_count;
    if (desc->step_count > 0u) {
        memcpy(&store->proof_steps[step_offset], desc->steps,
               desc->step_count * sizeof(laplace_proof_step_t));
    }
    store->proof_step_count += desc->step_count;

    const uint32_t id = store->theorem_count + 1u;
    store->theorems[id].assertion_id = desc->assertion_id;
    store->theorems[id].step_offset  = step_offset;
    store->theorems[id].step_count   = desc->step_count;
    store->theorems[id].flags        = desc->flags;
    store->theorem_count++;

    store->assertions[desc->assertion_id].theorem_id = id;

    if (out_id) *out_id = id;
    return LAPLACE_OK;
}

const laplace_proof_theorem_t* laplace_proof_get_theorem(
    const laplace_proof_store_t* store,
    laplace_proof_theorem_id_t id) {

    if (!store || !store->initialized) return NULL;
    if (id == LAPLACE_PROOF_THEOREM_ID_INVALID || id > store->theorem_count) return NULL;
    return &store->theorems[id];
}

laplace_error_t laplace_proof_get_theorem_steps(
    const laplace_proof_store_t* store,
    laplace_proof_theorem_id_t id,
    const laplace_proof_step_t** out_steps,
    uint32_t* out_count) {

    if (!store || !store->initialized || !out_steps || !out_count) {
        return LAPLACE_ERR_INVALID_ARGUMENT;
    }

    const laplace_proof_theorem_t* thm = laplace_proof_get_theorem(store, id);
    if (!thm) return LAPLACE_ERR_OUT_OF_RANGE;

    if (thm->step_count == 0u) {
        *out_steps = NULL;
        *out_count = 0u;
    } else {
        *out_steps = &store->proof_steps[thm->step_offset];
        *out_count = thm->step_count;
    }
    return LAPLACE_OK;
}

uint32_t laplace_proof_symbol_count(const laplace_proof_store_t* store) {
    return store ? store->symbol_count : 0u;
}

uint32_t laplace_proof_expr_count(const laplace_proof_store_t* store) {
    return store ? store->expr_count : 0u;
}

uint32_t laplace_proof_frame_count(const laplace_proof_store_t* store) {
    return store ? store->frame_count : 0u;
}

uint32_t laplace_proof_hyp_count(const laplace_proof_store_t* store) {
    return store ? store->hyp_count : 0u;
}

uint32_t laplace_proof_assertion_count(const laplace_proof_store_t* store) {
    return store ? store->assertion_count : 0u;
}

uint32_t laplace_proof_dv_pair_count(const laplace_proof_store_t* store) {
    return store ? store->dv_pair_count : 0u;
}

uint32_t laplace_proof_theorem_count(const laplace_proof_store_t* store) {
    return store ? store->theorem_count : 0u;
}
